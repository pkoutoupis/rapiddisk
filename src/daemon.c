/*********************************************************************************
 ** Copyright © 2011 - 2020 Petros Koutoupis
 ** All rights reserved.
 **
 ** This file is part of RapidDisk.
 **
 ** RapidDisk is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** RapidDisk is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with RapidDisk.  If not, see <http://www.gnu.org/licenses/>.
 **
 ** SPDX-License-Identifier: GPL-2.0-or-later
 **
 ** @project: rapiddisk
 **
 ** @filename: daemon.c
 ** @description: This is the main daemon file for the RapidDisk userland tool.
 **
 ** @date: 29Jul20, petros@petroskoutoupis.com
 ********************************************************************************/

#define _GNU_SOURCE
#define _OPEN_THREADS
#include "common.h"
#include "daemon.h"
#include <pthread.h>
#include <signal.h>

#define THREAD_CHECK_DELAY	10  /* in seconds */

struct PTHREAD_ARGS *args;

/*
 * description: print the help menu.
 */
void online_menu(void)
{
        printf("%s is a daemon intended to listen for API requests.\n\n"
               "Usage: %s [ -h | -V ] [ options ]\n\n", DAEMON, DAEMON);
        printf("Functions:\n"
               "\t-h\tPrint this exact help menu.\n"
               "\t-p\tChange port to listen on (default: 9118).\n"
               "\t-v\tEnable debug messages to stdout (this is ugly).\n"
               "\t-V\tPrint out version information.\n\n");
}

/*
 * description: make sure that the daemon is not already running.
 */
int proc_find(void)
{
	DIR *dir;
	struct dirent *list;
	char buf[NAMELEN * 2] = {0}, name[NAMELEN] = {0,}, state;
	long pid;
	int count = 0, rc = SUCCESS;
	FILE *fp = NULL;

	if (!(dir = opendir("/proc"))) {
		syslog(LOG_ERR, "%s: %s (%d): opendir: %s.\n", DAEMON,
		       __func__, __LINE__, strerror(errno));
		return INVALID_VALUE;
	}
	while ((list = readdir(dir)) != NULL) {
		pid = atol(list->d_name);
		if (pid < 0)
			continue;

		snprintf(buf, sizeof(buf), "/proc/%ld/stat", pid);
		fp = fopen(buf, "r");
		if (fp) {
			if ((fscanf(fp, "%ld (%[^)]) %c", &pid, name, &state)) != 3 ) {
				syslog(LOG_ERR, "%s: %s (%d): fscanf: %s.\n",
				       DAEMON, __func__, __LINE__, strerror(errno));
				fclose(fp);
				rc = INVALID_VALUE;
				goto proc_exit_on_failure;
			}
			fclose(fp);
			if (!strcmp(name, DAEMON))
				count++;
		}
	}
	if (count > 1)
		rc = INVALID_VALUE;
proc_exit_on_failure:
	closedir(dir);
	return rc;
}

/*
 * description: check the status of the thread (to restart).
 */
int thread_check_status(pthread_t tid)
{
	struct pthread *thread = (struct pthread *)tid;
	if ((tid == INVALID_VALUE) || (!thread))
		return INVALID_VALUE;
	if (pthread_kill(tid, 0) != 0)
		return INVALID_VALUE;
	return SUCCESS;
}

/*
 *
 */

int main(int argc, char *argv[])
{
	pid_t pid;
	int rc = SUCCESS, i;
	pthread_t mgmt_tid = INVALID_VALUE;
	unsigned char path[DEFAULT_STRSZ] = {0};

	printf("%s %s\n%s\n\n", DAEMON, VERSION_NUM, COPYRIGHT);

	/* Make sure that only a single instance is running */
	if ((rc = proc_find())) {
		printf("%s: The daemon is already running...\n", __func__);
		syslog(LOG_ERR, "%s: %s: The daemon is already running...\n",
		       DAEMON, __func__);
		goto exit_on_failure;
	}

	args = (struct PTHREAD_ARGS *)calloc(1, sizeof(struct PTHREAD_ARGS));
	if (args == NULL) {
		syslog(LOG_ERR, "%s: %s: calloc: %s\n", DAEMON, __func__, strerror(errno));
		printf("%s: %s: calloc: %s\n", DAEMON, __func__, strerror(errno));
		return -ENOMEM;
	}

	sprintf(args->port, "%s", DEFAULT_MGMT_PORT);

	if (realpath(argv[0], path) == NULL) {
		syslog(LOG_ERR, "%s: %s: realpath: %s\n", DAEMON, __func__, strerror(errno));
		printf("%s: %s: realpath: %s\n", DAEMON, __func__, strerror(errno));
		return -EIO;
	}
	sprintf(args->path, "%s", dirname(path));

	while ((i = getopt(argc, argv, "hvp:V")) != INVALID_VALUE) {
		switch (i) {
		case 'h':
			online_menu();
			return SUCCESS;
			break;
		case 'v':
			args->verbose = 1;
			break;
		case 'p':
			sprintf(args->port, "%s", optarg);
			break;
		case 'V':
			return SUCCESS;
			break;
		case '?':
			online_menu();
			return SUCCESS;
			break;
		}
	}

	if ((pid = fork()) < SUCCESS) {
		rc = pid;
		goto exit_on_failure;
	} else if (pid != SUCCESS)
		exit(SUCCESS);

	setsid();
	chdir("/");

	syslog(LOG_INFO, "%s: Daemon started.\n", DAEMON);

	while (1) {
		if (thread_check_status(mgmt_tid) == INVALID_VALUE) {
			if ((rc = pthread_create(&mgmt_tid, NULL, &mgmt_thread,
						 (void *)args)) != SUCCESS) {
				syslog(LOG_ERR, "%s: %s: pthread_create (management): %s\n",
				       DAEMON, __func__, strerror(errno));
				if (args->verbose)
					printf("%s: %s: pthread_create (management): %s\n", DAEMON, __func__, strerror(errno));
				goto exit_on_failure;
			}
		}
		sleep(THREAD_CHECK_DELAY);
	}

exit_on_failure:

	syslog(LOG_INFO, "%s: Daemon exiting.\n", DAEMON);
	return rc;
}
