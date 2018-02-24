
#include <sys/stat.h>
#include <sys/types.h>

#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <getopt.h>
#include <resolv.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>

#include <libubus.h>
#include <libubox/uloop.h>

#include "ubus.h"
#include "cron.h"
#include "chron.h"
#include "database.h"

static void
signal_shutdown(int signal)
{
	uloop_end();
}

int main(int argc, char *argv[])
{
	int rc = 0;
	
	uloop_init();
	
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, signal_shutdown);
	signal(SIGKILL, signal_shutdown);
	
	rc = chron_db_open(CHRON_DB_LOCATION, CHRON_DB_FILENAME);
	if(rc < 0)
		return -1;
	
	ubus_startup();
	
	cron_startup();
	
	uloop_run();
	uloop_done();
	
	chron_db_close();
	
	return 0;
}

