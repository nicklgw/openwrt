
#include "chron.h"


const char *chron_strerror(int chron_errno)
{
	switch(chron_errno){
		case CHRON_ERR_FAILED:
			return "Unknown error.";
		case CHRON_ERR_OK:
			return "OK.";
		case CHRON_ERR_CREATE:
			return "Fail to create scheduled task.";			
		case CHRON_ERR_NOT_FOUND:
			return "The specified ID not found.";
		case CHRON_ERR_DELETE:
			return "Failed to delete scheduled task.";
		case CHRON_ERR_UPDATE:
			return "Failed to update scheduled task.";
		case CHRON_ERR_QUERY:
			return "Failed to query scheduled task.";			
		default:
			return "Unknown error.";
	}
}



