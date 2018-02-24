
#ifndef __DATABASE_H__
#define __DATABASE_H__

/* Database macros */
#define CHRON_DB_VERSION 0

int chron_db_open(const char *location, const char *filename);

int chron_db_close(void);

int chron_db_insert(int *id, int type, char *time, char *date, char *notice);

int chron_db_count(int id);

int chron_db_delete(int id);

int chron_db_update(int id, int type, char *time, char *date, char *notice, int state);

int chron_db_query(int id, int *type, char **time, char **date, char **notice, int *state); // time/date/notice�ֶη��غ���Ҫ�ͷ�free

int chron_db_get_all(char **json); // ��ȡ����chron����, ���Ϊjson�����ַ���

int chron_db_changes(void);

#endif // __DATABASE_H__

