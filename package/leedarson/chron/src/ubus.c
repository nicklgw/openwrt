
#include <sys/types.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <unistd.h>

#include <libubus.h>
#include <libubox/vlist.h>
#include <libubox/uloop.h>
#include <libubox/ulog.h>
#include <json-c/json_object.h>

#include "ubus.h"
#include "cron.h"
#include "database.h"
#include "chron.h"

static struct ubus_auto_conn conn;
static struct blob_buf b;


enum {
	CREATE_TYPE,
	CREATE_TIME,
	CREATE_DATE,
	CREATE_NOTICE,
	__CREATE_MAX
};

static const struct blobmsg_policy create_policy[] = {
	[CREATE_TYPE]= { "type", BLOBMSG_TYPE_INT32 },
	[CREATE_TIME]	= { "time", BLOBMSG_TYPE_STRING },
	[CREATE_DATE]	= { "date", BLOBMSG_TYPE_STRING },
	[CREATE_NOTICE]	= { "notice", BLOBMSG_TYPE_STRING },
};

static int
chron_create(struct ubus_context *ctx, struct ubus_object *obj,
		    struct ubus_request_data *req, const char *method,
		    struct blob_attr *msg)
{
	struct blob_attr *data[__CREATE_MAX];
	int rc = 0;
	void *tbl;
	
	int id;
	int type;
	char *time;
	char *date;
	char *notice;
	
	blobmsg_parse(create_policy, __CREATE_MAX, data, blob_data(msg), blob_len(msg));
	if (!data[CREATE_TYPE] || !data[CREATE_TIME] 
		|| !data[CREATE_DATE] || !data[CREATE_NOTICE])
		return UBUS_STATUS_INVALID_ARGUMENT;
	
	type = blobmsg_get_u32(data[CREATE_TYPE]);
	time = blobmsg_get_string(data[CREATE_TIME]);
	date = blobmsg_get_string(data[CREATE_DATE]);
	notice = blobmsg_get_string(data[CREATE_NOTICE]);
	
	rc = chron_db_insert(&id, type, time, date, notice);
	
	blobmsg_buf_init(&b);
	if(!rc)
	{
		tbl = blobmsg_open_table(&b, "result"); 
		blobmsg_add_u32(&b, "id", id);	
		cron_startup();
	}
	else
	{
		tbl = blobmsg_open_table(&b, "error");
		blobmsg_add_u32(&b, "code", CHRON_ERR_CREATE);
		blobmsg_add_string(&b, "message", chron_strerror(CHRON_ERR_CREATE));
	}
	blobmsg_close_table(&b, tbl);
	
	return ubus_send_reply(ctx, req, b.head);
}


enum {
	DELETE_ID,
 	__DELETE_MAX
};

static const struct blobmsg_policy delete_policy[] = {
	[DELETE_ID]= { "id", BLOBMSG_TYPE_INT32 },
};

static int
chron_delete(struct ubus_context *ctx, struct ubus_object *obj,
		    struct ubus_request_data *req, const char *method,
		    struct blob_attr *msg)
{
	struct blob_attr *data[__DELETE_MAX];
	int rc = 0;
	void *tbl;
	int id;
	int changes;
	
	blobmsg_parse(delete_policy, __DELETE_MAX, data, blob_data(msg), blob_len(msg));
	if (!data[DELETE_ID])
		return UBUS_STATUS_INVALID_ARGUMENT;
	
	id = blobmsg_get_u32(data[DELETE_ID]);
	ULOG_INFO("id: %d \n", id);
	
	rc = chron_db_delete(id);
	changes = chron_db_changes();
	ULOG_INFO("rc: %d, changes: %d \n", rc, changes);
	
	blobmsg_buf_init(&b);
	if(!rc && changes)
	{
		tbl = blobmsg_open_array(&b, "result");
		blobmsg_add_string(&b, NULL, "OK");		
		cron_startup();
	}
	else
	{
		tbl = blobmsg_open_table(&b, "error"); 
		blobmsg_add_u32(&b, "code", CHRON_ERR_DELETE);	
		blobmsg_add_string(&b, "message", chron_strerror(CHRON_ERR_DELETE));	
	}
	blobmsg_close_table(&b, tbl);
	
	return ubus_send_reply(ctx, req, b.head);	
}


enum {
	UPDATE_ID,
	UPDATE_ATTRS,
	__UPDATE_MAX
};

static const struct blobmsg_policy update_policy[] = {
	[UPDATE_ID]= { "id", BLOBMSG_TYPE_INT32 },
	[UPDATE_ATTRS]= { "attrs", BLOBMSG_TYPE_TABLE },
};
// type:-1, time:NULL, date:NULL, notice:NULL, state:-1 默认值不修改
static int
chron_update(struct ubus_context *ctx, struct ubus_object *obj,
		    struct ubus_request_data *req, const char *method,
		    struct blob_attr *msg)
{
	struct blob_attr *data[__UPDATE_MAX];
	int rc = 0;
	void *tbl;
	
	struct blob_attr *attrs, *cur;
	int rem;
	
	int id = 0;
	int type = -1;
	char *time = NULL;
	char *date = NULL;
	char *notice = NULL;
	int state = -1;
	
	blobmsg_parse(update_policy, __UPDATE_MAX, data, blob_data(msg), blob_len(msg));
	if (!data[UPDATE_ID] || !data[UPDATE_ATTRS])
		return UBUS_STATUS_INVALID_ARGUMENT;
	
	id = blobmsg_get_u32(data[UPDATE_ID]);
	attrs = data[UPDATE_ATTRS];
	
	blobmsg_for_each_attr(cur, attrs, rem)
	{
		fprintf(stdout, " key: %s, value: %s \n", 
			(char *) blobmsg_name(cur),
			(char *) blobmsg_data(cur));
		
		if(!strcmp(blobmsg_name(cur), "type")) type = blobmsg_get_u32(cur);
		if(!strcmp(blobmsg_name(cur), "time")) time = blobmsg_get_string(cur);
		if(!strcmp(blobmsg_name(cur), "date")) date = blobmsg_get_string(cur);
		if(!strcmp(blobmsg_name(cur), "notice")) notice = blobmsg_get_string(cur);
		if(!strcmp(blobmsg_name(cur), "state")) state = blobmsg_get_u32(cur);
	}
	
	rc = chron_db_update(id, type, time, date, notice, state);
	
	blobmsg_buf_init(&b);
	if(!rc)
	{
		tbl = blobmsg_open_array(&b, "result");
		blobmsg_add_string(&b, NULL, "OK");		
		cron_startup();
	}
	else
	{
		tbl = blobmsg_open_table(&b, "error"); 
		blobmsg_add_u32(&b, "code", CHRON_ERR_UPDATE);	
		blobmsg_add_string(&b, "message", chron_strerror(CHRON_ERR_UPDATE));	
	}
	blobmsg_close_table(&b, tbl);
	
	return ubus_send_reply(ctx, req, b.head);
}

enum {
	QUERY_ID,
 	__QUERY_MAX
};

static const struct blobmsg_policy query_policy[] = {
	[QUERY_ID]= { "id", BLOBMSG_TYPE_INT32 },
};

static int
chron_query(struct ubus_context *ctx, struct ubus_object *obj,
		    struct ubus_request_data *req, const char *method,
		    struct blob_attr *msg)
{
	struct blob_attr *data[__QUERY_MAX] = { 0 };	
	void *tbl = NULL;
	int rc = 0;

	char *json = NULL;
	int id = 0; 
	int type = 0; 
	char *time = NULL; 
	char *date = NULL; 
	char *notice = NULL; 
	int state = 0;
	
	blobmsg_parse(query_policy, __QUERY_MAX, data, blob_data(msg), blob_len(msg));
	if (!data[QUERY_ID])
		return UBUS_STATUS_INVALID_ARGUMENT;
	
	id = blobmsg_get_u32(data[QUERY_ID]);
	ULOG_INFO("id: %d \n", id);
	
	if(-1 == id) // 查询所有任务
		rc = chron_db_get_all(&json);
	else // 查询单个任务
		rc = chron_db_query(id, &type, &time, &date, &notice, &state);
	
	blobmsg_buf_init(&b);
	if(rc) // 0 != rc 发生错误时
	{
		tbl = blobmsg_open_table(&b, "error");
		blobmsg_add_u32(&b, "code", CHRON_ERR_QUERY);
		blobmsg_add_string(&b, "message", chron_strerror(CHRON_ERR_QUERY));
		blobmsg_close_table(&b, tbl);
	}
	else if(-1 == id) // 成功且是查询所有任务
	{
		json_object *json_root = json_object_new_object();
		json_object *json_array = json_tokener_parse(json);
		json_object_object_add(json_root, "result", json_array);
		blobmsg_add_object(&b, json_root);
		json_object_put(json_root);
	}
	else // 成功且查询单个任务
	{
		tbl = blobmsg_open_table(&b, "result");	
		blobmsg_add_u32(&b, "id", id);
		blobmsg_add_u32(&b, "type", type);	
		blobmsg_add_string(&b, "time", time);
		blobmsg_add_string(&b, "date", date);
		blobmsg_add_string(&b, "notice", notice);
		blobmsg_add_u32(&b, "state", state);		
		blobmsg_close_table(&b, tbl);
	}
	
	if(time) { free(time); time = NULL; }
	if(date) { free(date); date = NULL; }
	if(notice) { free(notice); notice = NULL; }
	if(json) { free(json); json = NULL; }
	
	return ubus_send_reply(ctx, req, b.head);
}


static const struct ubus_method chron_methods[] = {
	UBUS_METHOD("create", chron_create, create_policy),
	UBUS_METHOD("delete", chron_delete, delete_policy),
	UBUS_METHOD("update", chron_update, update_policy),
	UBUS_METHOD("query", chron_query, query_policy),
};

static struct ubus_object_type chron_object_type =
	UBUS_OBJECT_TYPE("chron", chron_methods);

static struct ubus_object chron_object = {
	.name = "chron",
	.type = &chron_object_type,
	.methods = chron_methods,
	.n_methods = ARRAY_SIZE(chron_methods),
};

int chron_time_up(int id, char *notice)
{
	blob_buf_init(&b, 0);
	blobmsg_add_u32(&b, "id", id);
	blobmsg_add_string(&b, "notice", notice);
	
	ULOG_INFO("\"chron.time_up\": {\"id\":%d,\"notice\":\"%s\"}", id, notice);
	
	return ubus_notify(&conn.ctx, &chron_object, "time_up", b.head, -1);
}

static void
ubus_connect_handler(struct ubus_context *ctx)
{
	int ret;

	ret = ubus_add_object(ctx, &chron_object);
	if (ret)
		fprintf(stderr, "Failed to add object: %s\n", ubus_strerror(ret));
}

void
ubus_startup(void)
{
	conn.cb = ubus_connect_handler;
	ubus_auto_connect(&conn);
}

