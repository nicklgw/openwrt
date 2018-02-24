
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <libubox/uloop.h>
#include <libubox/ulog.h>
#include <json-c/json_object.h>

#include "database.h"

static sqlite3 *db = NULL;

int _chron_db_tables_create(void);
int _chron_db_version_check(void);
int _chron_db_invalidate_state(void);


// Die if we can't allocate size bytes of memory.
void* xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL && size != 0)
	{
		fprintf(stderr, "memory exhausted !!! \n");
		exit(EXIT_FAILURE);
	}
	
	return ptr;
}

// Die if we can't allocate and zero size bytes of memory.
void*  xzalloc(size_t size)
{
	void *ptr = xmalloc(size);
	memset(ptr, 0, size);
	return ptr;
}

int chron_db_open(const char *location, const char *filename)
{
	char *filepath;
	
	if(!filename) return -1;
	
	if(sqlite3_initialize() != SQLITE_OK){
		return -1;
	}
	
	if(location && strlen(location) && strcmp(filename, ":memory:")){
		filepath = malloc(strlen(location) + strlen(filename) + 1);
		if(!filepath) return -1;
		sprintf(filepath, "%s%s", location, filename);
	}else{
		filepath = (char *)filename;
	}
	/* Open without creating first. If found, check for db version.
	 * If not found, open with create.
	 */
	if(sqlite3_open_v2(filepath, &db, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK){
		if(_chron_db_version_check()){
			fprintf(stderr, "Error: Invalid database version.\n");
			return -1;
		}
	}else{
		if(sqlite3_open_v2(filepath, &db,
				SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK){
			fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
			return -1;
		}
		if(_chron_db_tables_create()) return -1;
	}
	
	if(filepath && filepath != filename){
		free(filepath);
	}
	
	return 0;
}

int chron_db_close(void)
{
	sqlite3_close(db);
	db = NULL;
	
	sqlite3_shutdown();
	
	return 0;
}

int _chron_db_tables_create(void)
{
	int rc = 0;
	char *errmsg = NULL;
	char *query;
	
	if(sqlite3_exec(db,
		"CREATE TABLE IF NOT EXISTS config(key TEXT PRIMARY KEY, value TEXT)",
		NULL, NULL, &errmsg) != SQLITE_OK){
		
		rc = -1;
	}
	if(errmsg){
		sqlite3_free(errmsg);
		errmsg = NULL;
	}
	
	query = sqlite3_mprintf("INSERT INTO config (key, value) VALUES('version','%d')", CHRON_DB_VERSION);
	if(query){
		if(sqlite3_exec(db, query, NULL, NULL, &errmsg) != SQLITE_OK){
			rc = -1;
		}
		sqlite3_free(query);
		if(errmsg){
			sqlite3_free(errmsg);
			errmsg = NULL;
		}
	}else{
		return -1;
	}
	
	if(sqlite3_exec(db,
		"CREATE TABLE IF NOT EXISTS chron("
		"id INTEGER PRIMARY KEY AUTOINCREMENT, "
		"type INTEGER, "
		"time TEXT, "
		"date TEXT, "
		"notice TEXT, "
		"state INTEGER)",
		NULL, NULL, &errmsg) != SQLITE_OK){
		
		rc = -1;
	}
	if(errmsg){
		sqlite3_free(errmsg);
		errmsg = NULL;
	}
	
	return rc;
}

int _chron_db_version_check(void)
{
	int rc = 0;
	int version;
	sqlite3_stmt *stmt = NULL;
	
	if(!db) return -1;
	
	if(sqlite3_prepare_v2(db, "SELECT value FROM config WHERE key='version'",
			-1, &stmt, NULL) == SQLITE_OK){
		
		if(sqlite3_step(stmt) == SQLITE_ROW){
			version = sqlite3_column_int(stmt, 0);
			if(version != CHRON_DB_VERSION) rc = -1;
		}else{
			rc = -1;
		}
		sqlite3_finalize(stmt);
	}else{
		rc = -1;
	}
	
	return rc;
}

int _chron_db_invalidate_state(void)
{
	int rc = 0;
	char *query = NULL;
	char *errmsg;
	
	if(!db) return -1;
	
	query = sqlite3_mprintf("UPDATE chron SET state=-1");
	if(query){
		if(sqlite3_exec(db, query, NULL, NULL, &errmsg) != SQLITE_OK){
			rc = -1;
		}
		sqlite3_free(query);
		if(errmsg){
			fprintf(stderr, "Error: %s\n", errmsg);
			sqlite3_free(errmsg);
		}
	}else{
		return -1;
	}

	return rc;
}

int chron_db_insert(int *id, int type, char *time, char *date, char *notice)
{
	int rc = 0;
	sqlite3_stmt *stmt = NULL;
	sqlite3_int64  rowid = 0;
	
	if(!db) return -1;
	
	if(sqlite3_prepare_v2(db, "INSERT INTO chron "
				"(type, time, date, notice, state) "
				"VALUES (?,?,?,?,-1)",
			-1, &stmt, NULL) != SQLITE_OK)
	{
		rc = -1;
		goto ret;
	}
	
	if(sqlite3_bind_int(stmt, 1, type) != SQLITE_OK) rc = -1;
	if(sqlite3_bind_text(stmt, 2, time, strlen(time), SQLITE_STATIC) != SQLITE_OK) rc = -1;
	if(sqlite3_bind_text(stmt, 3, date, strlen(date), SQLITE_STATIC) != SQLITE_OK) rc = -1;
	if(sqlite3_bind_text(stmt, 4, notice, strlen(notice), SQLITE_STATIC) != SQLITE_OK) rc = -1;
	
	if(sqlite3_step(stmt) != SQLITE_DONE) 
	{
		rc = -1;
		goto ret;
	}
	
	rowid = sqlite3_last_insert_rowid(db);
	if((bool)rowid)
		if(id) *id = (int)rowid;
	else
		rc = -1;
	
ret:
	sqlite3_finalize(stmt);
	return rc;
}

int chron_db_count(int id)
{
	int rc = 0;
	sqlite3_stmt *stmt = NULL;
	
	if(!db) return -1;
	
	if(sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM chron WHERE id=?",
			-1, &stmt, NULL) != SQLITE_OK)
	{
		rc = -1;
		goto ret;
	}
	
	if(sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) rc = -1;
	
	if(SQLITE_ROW != sqlite3_step(stmt))
	{
		rc = -1;
		goto ret;
	}
	
	rc = sqlite3_column_int(stmt, 0);
	
ret:
	
	sqlite3_finalize(stmt);
	
	return rc;
}

int chron_db_delete(int id)
{
	int rc = 0;
	char *query = NULL;
	char *errmsg;
	
	if(!db) return -1;
	
	query = sqlite3_mprintf("DELETE FROM chron WHERE id=%d", id);
	if(query){
		if(sqlite3_exec(db, query, NULL, NULL, &errmsg) != SQLITE_OK){
			rc = -1;
		}
		sqlite3_free(query);
		if(errmsg){
			fprintf(stderr, "Error: %s\n", errmsg);
			sqlite3_free(errmsg);
		}
	}else{
		return -1;
	}
	
	return rc;
}

// type:-1, time:NULL, date:NULL, notice:NULL, state:-1 默认值不修改
int chron_db_update(int id, int type, char *time, char *date, char *notice, int state)
{
	int rc = 0;
	char *errmsg = NULL;
	bool have = false;
	
	if(!db) return -1;
	
	char query[2048] = { 0 };
	char buffer[256] = { 0 };
	
	strcat(query, "UPDATE chron SET ");
	if(-1 != type)
	{
		sprintf(buffer, "type=%d", type);
		strcat(query, buffer);
		have = true;
	}
	if(time)
	{
		if(have)
			sprintf(buffer, ", time='%s'", time);
		else
			sprintf(buffer, "time='%s'", time);
		strcat(query, buffer);		
		have = true;			
	}
	if(date)
	{
		if(have)
			sprintf(buffer, ", date='%s'", date);
		else
			sprintf(buffer, "date='%s'", date);
		strcat(query, buffer);		
		have = true;
	}
	if(notice)
	{
		if(have)
			sprintf(buffer, ", notice='%s'", notice);
		else
			sprintf(buffer, "notice='%s'", notice);
		strcat(query, buffer);		
		have = true;
	}
	if(-1 != state)
	{
		if(have)
			sprintf(buffer, ", state=%d", state);
		else
			sprintf(buffer, "state=%d", state);
		strcat(query, buffer);
		have = true;
	}
	if(have)
	{
		sprintf(buffer, " WHERE id=%d", id);
		strcat(query, buffer);
	}
	else
		return -1;
	
	ULOG_INFO("chron_db_update query: %s \n", query);
	if(sqlite3_exec(db, query, NULL, NULL, &errmsg) != SQLITE_OK) rc = -1;
	if(errmsg)
	{
		fprintf(stderr, "Error: %s\n", errmsg);
		sqlite3_free(errmsg);
	}
	
	return rc;
}

int chron_db_query(int id, int *type, char **time, char **date, char **notice, int *state) // time/date/notice字段返回后需要释放free
{
	int rc = 0;
	sqlite3_stmt *stmt = NULL;
	char *tmp;
	
	if(!db) return -1;
	
	if(sqlite3_prepare_v2(db, "SELECT type,time,date,notice,state FROM chron WHERE id=?",
			-1, &stmt, NULL) == SQLITE_OK)
	{		
		if(sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) rc = -1;
		
		if(sqlite3_step(stmt) == SQLITE_ROW)
		{
			if(type) *type = sqlite3_column_int(stmt, 0);
			
			tmp = (char*)sqlite3_column_text(stmt, 1);
			if(tmp && time)
			{
				*time = xzalloc(strlen(tmp)+1);
				strcpy(*time, tmp);
			}
			tmp = (char*)sqlite3_column_text(stmt, 2);
			if(tmp && date)
			{
				*date = xzalloc(strlen(tmp)+1);
				strcpy(*date, tmp);
			}
			tmp = (char*)sqlite3_column_text(stmt, 3);
			if(tmp && notice)
			{
				*notice = xzalloc(strlen(tmp)+1);
				strcpy(*notice, tmp);
			}
			
			if(state) *state = sqlite3_column_int(stmt, 4);
			
		}
		else
			rc = -1;
		
		sqlite3_finalize(stmt);
	}
	else
		rc = -1;
	
	return rc;
}

int chron_db_get_all(char **json)
{
	int rc = 0;
	char *query = NULL;
	char *errmsg = NULL;
	
	char **result = NULL;
	int rows = 0;
	int cols = 0;	
	int r = 0; 
	
	int id;
	int type; 
	char *time; 
	char *date; 
	char *notice;
	int state;
	
	const char *jstr;
	
	if(!db) return -1;
	
	query = sqlite3_mprintf("SELECT id, type, time, date, notice, state FROM chron");
	if(!query)
		return -1;
	
	rc = sqlite3_get_table(db, query, &result, &rows, &cols, &errmsg);
	if(SQLITE_OK != rc)
	{
		fprintf(stderr, "Can't get table! rc:%d, errmsg:%s \n", rc, errmsg);
		sqlite3_free(errmsg);
		errmsg = NULL;
		return -1;
	}
	
	json_object *json_array = json_object_new_array();
	
	for(r = 1; r < rows+1; r++)
	{
		id = atoi(result[r*cols+0]);
		type = atoi(result[r*cols+1]);
		time = result[r*cols+2];
		date = result[r*cols+3];
		notice = result[r*cols+4];
		state = atoi(result[r*cols+5]);
		
		json_object *json_record = json_object_new_object();
		json_object_object_add(json_record, "id", json_object_new_int(id));
		json_object_object_add(json_record, "type", json_object_new_int(type));
		json_object_object_add(json_record, "time", json_object_new_string(time));
		json_object_object_add(json_record, "date", json_object_new_string(date));
		json_object_object_add(json_record, "notice", json_object_new_string(notice));
		json_object_object_add(json_record, "state", json_object_new_int(state));
		
		json_object_array_add(json_array, json_record);		
	}
	
	jstr = json_object_to_json_string(json_array);
	ULOG_INFO("all chron task json : %s \n", jstr);
	if(json)
	{
		*json = xzalloc(strlen(jstr)+1);
		strcpy(*json, jstr);
	}
	
	sqlite3_free_table(result);
	sqlite3_free(query);
	json_object_put(json_array);
	
	return rc;
}

int chron_db_changes(void)
{
	return sqlite3_changes(db);
}

