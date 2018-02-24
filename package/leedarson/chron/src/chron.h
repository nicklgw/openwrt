
#ifndef __CHRON_H__
#define __CHRON_H__

#define CHRON_DB_LOCATION "/root/"
#define CHRON_DB_FILENAME "chron.db"


/* Error values */
enum chron_err_t {
	
	CHRON_ERR_FAILED = -1,
	CHRON_ERR_OK = 0,
	CHRON_ERR_CREATE = -10001,
	CHRON_ERR_NOT_FOUND = -10002,
	CHRON_ERR_DELETE = -10003,	
	CHRON_ERR_UPDATE = -10004,
	CHRON_ERR_QUERY = -10005,
	
};

const char *chron_strerror(int chron_errno);


#endif // __CHRON_H__

