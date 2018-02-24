
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdlib.h>

#include <json-c/json.h>
#include <libubox/uloop.h>
#include <libubox/ulog.h>

#include "cron.h"
#include "ubus.h"
#include "database.h"

#define ARRAY_SIZE(x) ((unsigned)(sizeof(x) / sizeof((x)[0])))

typedef enum date_type {
	
    DATE_DAY = 0, 		// 特定日期 day		[ 2017-5-1]代表2017.5.1执行
    DATE_WEEK,			// 星期 week 		[1,2,3] 代表周一，周二，周三执行
    DATE_DURING,		// 日期区间 during  [2017-3-9~2017-3-10] 代表2017-3-9,2017-3-10执行
    
} date_type;

typedef enum cron_state {
	
	STATE_INVALID = -1, 	// 无效状态
	STATE_IDEL = 0, 		// 空闲或执行完毕
	STATE_READY, 			// 符合条件,准备运行 
	STATE_RUNNING,			// 正在运行
	
} cron_state;

typedef struct cron_line {
	
	struct cron_line *cl_next;
	
	cron_state 	cl_state;
	
	int cl_id;
	
	date_type cl_type;	// 0 特定日期, 1 周期, 2 日期区间
	
	int cl_hour;   // Hours (0-23)
	int cl_min;    // Minutes (0-59)
	
	union {
		struct {
			int year; 	// 2017
			int mon;	// 1-12
			int mday;	// 1-31
		} day;
		struct {
			int dow[7];	// 7 /* Day of the week (0-6, Sunday = 0) */
		} week;
		struct {
			time_t t1; // 2018-1-31 => 1517328000
			time_t t2; // 2018-2-14 => 1518537600
		} during ;
	} cl_date;
	
	char cl_notice[1024];
	
} cron_line;

static cron_line *cron_line_base = NULL;



static int parse_time(cron_line *line, const char *tim) // 验证time合法性,并转化为cl_hour,cl_min
{
	int hour = -1, minute = -1;
	int *time[2] = { &hour, &minute };
	int count = 0;
	char cur_time[32] = { 0 };
	char *tmp, *p, *last_ptr;
	
	strcpy(cur_time, tim);
	tmp = cur_time;
	
	while(NULL!=(p = strtok_r(tmp, ":", &last_ptr)) && count<ARRAY_SIZE(time))
	{
		tmp = NULL;
		
		*(time[count]) = atoi(p);
		printf("time[%d]: %d \n", count, *time[count]);
		
		count++;
	}
	
	if(hour<0 || hour>23 || minute <0 || minute > 60) return -1;
	
	line->cl_hour = hour;
	line->cl_min = minute;
	ULOG_INFO("OK line->cl_hour: %d, line->cl_min: %d \n", line->cl_hour, line->cl_min);
	
	return 0;
}

static int parse_date(cron_line *line, const char *date) // 验证date合法性,并转化为cl_date
{
	char cur_date[64] = { 0 };
	char *tmp, *p, *last_ptr;
	int count = 0;
	int wday = -1;   /* Day of the week (0-6, Sunday = 0) */
	
	strcpy(cur_date, date);
	
	switch(line->cl_type)
	{
		case DATE_DAY:	// [ 2017-5-1]代表2017.5.1执行
			{
				int *day[3] = { &line->cl_date.day.year, &line->cl_date.day.mon, &line->cl_date.day.mday };
				
				tmp = cur_date;
				count = 0;
				while(NULL!=(p = strtok_r(tmp, "-[]", &last_ptr)) && count<ARRAY_SIZE(day))
				{
					tmp = NULL;
					
					*(day[count]) = atoi(p);
					printf("day[%d]: %d \n", count, *day[count]);
					
					count++;
				}
				
				ULOG_INFO("DATE_DAY line->cl_date.day.year: %d, line->cl_date.day.mon: %d, line->cl_date.day.mday: %d \n",
					line->cl_date.day.year, line->cl_date.day.mon, line->cl_date.day.mday);
			}
			break;
		case DATE_WEEK: 	// [1,2,3] 代表周一，周二，周三执行
			{
				tmp = cur_date;
				while(NULL != (p = strtok_r(tmp, ",[]", &last_ptr)))
				{
					tmp = NULL;
					
					wday = atoi(p);
					printf("wday: %d \n", wday);
					if(wday>=0 && wday<=6)  line->cl_date.week.dow[wday] = 1;
				}
			}
			break;
		case DATE_DURING:	// [2017-3-9~2017-3-10] 代表2017-3-9,2017-3-10执行
			{
				int year1;	// 2017
				int mon1;	// 1-12
				int mday1;	// 1-31
				int year2;	// 2017
				int mon2;	// 1-12
				int mday2;	// 1-31
				
				struct tm tm1 = { 0 }, tm2 = { 0 };
				
				int *during[6] = { &year1, &mon1, &mday1, &year2, &mon2, &mday2 };
				
				tmp = cur_date;
				count = 0;
				while(NULL!=(p = strtok_r(tmp, "~-[]", &last_ptr)) && count<ARRAY_SIZE(during))
				{
					tmp = NULL;
					
					*(during[count]) = atoi(p);
					printf("*during[%d]: %d \n", count, *during[count]);
					
					count++;
				}
				
				tm1.tm_year = year1 - 1900;
				tm1.tm_mon	= mon1 - 1;
				tm1.tm_mday = mday1;			
				
				tm2.tm_year = year2 - 1900;
				tm2.tm_mon	= mon2 - 1;
				tm2.tm_mday = mday2;
				
				line->cl_date.during.t1 = mktime(&tm1);
				line->cl_date.during.t2 = mktime(&tm2);
			}
			break;		
		default:
			return -1;
	}
	
	return 0;
}

// 验证并解析输入参数的合法性, 并输出到line
static int parse_line(cron_line *line, 
	int id, int type, const char *time, const char *date, const char *notice, int state) 
{
	int rc = 0;
	
	if(!line || !time || !date || !notice)
		return -1;
	
	if(id > 0)
		line->cl_id = id;
	else
		return -1;
	
	switch(type)
	{
		case DATE_DAY:
		case DATE_WEEK:
		case DATE_DURING:
			line->cl_type = type;
			break;
		default:
			return -1;
	}
	
	switch(state)
	{
		case STATE_INVALID:
		case STATE_IDEL:
		case STATE_READY:
		case STATE_RUNNING:
			line->cl_state = state;
			break;
		default:
			return -1;
	}
	
	memset(line->cl_notice, 0, sizeof(line->cl_notice));	
	strncpy(line->cl_notice, notice, ARRAY_SIZE(line->cl_notice)-1);
	
	rc = parse_time(line, time);
	if(rc < 0)
		return -1;
	
	rc = parse_date(line, date);
	if(rc < 0)
		return -1;
	
	return 0;
}

static void delete_all_crons(void)
{
	cron_line **pline;
	cron_line *line;
	
	pline = &cron_line_base;
	
	while ((line = *pline) != NULL) {
		*pline = line->cl_next;
		free(line);
	}
	
	*pline = NULL;
}

static int synchronize_database(void) // 将数据库中记录同步到内存链表
{
	char *json = NULL;
	int rc = 0;
	struct json_object *root_obj = NULL;
	struct json_object *line_obj = NULL;
	int i = 0;
	
	int id = 0;
	int type = -1;
	const char *time = NULL;
	const char *date = NULL;
	const char *notice = NULL;
	int state = -1;
	
	cron_line **pline = NULL;
	cron_line *line = NULL;
	
	delete_all_crons();
	
	rc = chron_db_get_all(&json);
	if(rc < 0)
		return -1;
	
	root_obj = json_tokener_parse(json); 
	if(!root_obj)
	{		
		if(json) { free(json); json = NULL; }
		return -1;
	}
	
	pline = &cron_line_base;
	
	for(i = 0; i < json_object_array_length(root_obj); i++)
	{
		line_obj = json_object_array_get_idx(root_obj, i);
		
		id  = json_object_get_int(json_object_object_get(line_obj, "id"));
		type  = json_object_get_int(json_object_object_get(line_obj, "type"));
		time  = json_object_get_string(json_object_object_get(line_obj, "time"));
		date  = json_object_get_string(json_object_object_get(line_obj, "date"));
		notice  = json_object_get_string(json_object_object_get(line_obj, "notice"));
		state  = json_object_get_int(json_object_object_get(line_obj, "state"));
		
		if(STATE_IDEL != state)
		{
			state = STATE_IDEL;
			chron_db_update(id, -1, NULL, NULL, NULL, state);
		}
		
		cron_line cl = { 0 };
		rc = parse_line(&cl, id, type, time, date, notice, state);
		if(rc < 0) continue;
		
		*pline = line = xzalloc(sizeof(*line));
		memcpy(line, &cl, sizeof(cron_line));
		
		pline = &line->cl_next;
	}
	
	*pline = NULL;
	json_object_put(root_obj);
	if(json) { free(json); json = NULL; }
	
	return 0;
}

static void check_updates(void)
{
	synchronize_database();
}

static int match_job(cron_line *cl, struct tm *ptm)
{
	if(!cl || !ptm)
		return -1;
	
	// 时间(time) 是否匹配 cl_hour, tm_hour
	if(cl->cl_hour != ptm->tm_hour || cl->cl_min != ptm->tm_min)
		return 0;
	
	// 日期(date)是否匹配
	switch(cl->cl_type)
	{
	case DATE_DAY:	// [2017-5-1]代表2017.5.1执行
		{
			if(cl->cl_date.day.year == (1900+ptm->tm_year)
			&& cl->cl_date.day.mon == (1+ptm->tm_mon)
			&& cl->cl_date.day.mday == ptm->tm_mday)
				return 1;
		}
		break;
	case DATE_WEEK: 	// [1,2,3] 代表周一，周二，周三执行
		{
			if(cl->cl_date.week.dow[ptm->tm_wday])
				return 1;
		}
		break;
	case DATE_DURING:	// [2017-3-9~2017-3-10] 代表2017-3-9,2017-3-10执行
		{
			struct tm tm0 = { 0 };
			time_t t0, t1, t2;
			
			tm0.tm_year = ptm->tm_year;
			tm0.tm_mon  = ptm->tm_mon;
			tm0.tm_mday = ptm->tm_mday;
			
			t0 = mktime(&tm0);
			t1 = cl->cl_date.during.t1;
			t2 = cl->cl_date.during.t2;
			
			printf("t0: %ld, t1: %ld, t2: %ld \n", t0, t1, t2);
			
			if(t0>=t1 && t0<=t2)
				return 1;
		}
		break;
	default:
		return -1;	
	}
	
	return 0;
}

static int test_jobs(time_t t1, time_t t2) // 检查是否符合条件,并将cl_state置为STATE_READY
{
	int n_jobs = 0;
	time_t t;
	struct tm *ptm;
	cron_line *line;
	
	/* Find jobs > t1 and <= t2 */
	for (t = t1 - t1 % 60; t <= t2; t += 60) 
	{
		if (t <= t1)
			continue;
		
		ptm = localtime(&t);
		
		for (line = cron_line_base; line; line = line->cl_next) 
		{
			if(match_job(line, ptm) > 0)	
			{
				if(STATE_RUNNING == line->cl_state)
					fprintf(stderr, "already running id: %d, notice: %s \n", 
						line->cl_id, line->cl_notice);
				else if(STATE_IDEL == line->cl_state) 
				{
					line->cl_state = STATE_READY;					
					chron_db_update(line->cl_id, -1, NULL, NULL, NULL, line->cl_state);
					++n_jobs; // 需要运行的个数					
				}
			}
		}
	}
	
	return n_jobs;
}

static void run_job(cron_line *line) // 执行符合条件任务,并将状态置为STATE_RUNNING
{
	if(!line)
		return;
	
	chron_time_up(line->cl_id, line->cl_notice);
	
	if(STATE_READY == line->cl_state)
	{
		line->cl_state = STATE_RUNNING;
		chron_db_update(line->cl_id, -1, NULL, NULL, NULL, line->cl_state);
	}
}

static void run_jobs(void)
{
	cron_line *line = NULL;
	
	for (line = cron_line_base; line; line = line->cl_next) 
	{
		if (STATE_READY != line->cl_state) 
			continue;
		
		run_job(line);
	}
}

static void check_jobs(void) // 将执行完的任务状态置为STATE_IDEL
{
	cron_line *line = NULL;
	
	for (line = cron_line_base; line; line = line->cl_next) 
	{
		if (STATE_RUNNING != line->cl_state)
			continue;
		
		line->cl_state = STATE_IDEL;		
		chron_db_update(line->cl_id, -1, NULL, NULL, NULL, line->cl_state);
	}	
}

static void cron_queue_cb(struct uloop_timeout *timeout)
{
	static time_t t1 = 0;
	
	int sleep_period = 60; // 睡眠周期, 一分钟
	time_t t2;
	long dt;	
	int sdt;
	
	t2 = time(NULL);
	dt = (long)t2 - (long)t1;
	
	check_updates();
	
	if (dt < -60 * 60 || dt > 60 * 60) 
	{
		fprintf(stderr, "time disparity of %ld minutes detected \n", dt / 60);
	}
	else if (dt > 0) 
	{
		test_jobs(t1, t2);
		
		run_jobs();
		
		check_jobs();
	}
	
	t1 = t2;
	
	sdt = (sleep_period + 1) - (time(NULL) % sleep_period);
	fprintf(stderr, "continue to sleep ( %d / %d ) \n", sdt, sleep_period);
	uloop_timeout_set(timeout, sdt*1000);
}

static struct uloop_timeout cron_queue_timer = {
	.cb = cron_queue_cb
};

void cron_startup(void)
{
	uloop_timeout_set(&cron_queue_timer, 100);
}

