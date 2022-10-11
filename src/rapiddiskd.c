/*********************************************************************************
 ** Copyright © 2011 - 2022 Petros Koutoupis
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

#define SERVER
#include "rapiddiskd.h"
#include "utils.h"
#include <libgen.h>

/*
 * description: print the help menu.
 */
void online_menu(void)
{
	printf("%s %s\n%s\n\n", DAEMON, VERSION_NUM, COPYRIGHT);
	printf("%s is a daemon intended to listen for API requests.\n\n"
		   "Usage: %s [ -h | -v ] [ options ]\n\n", DAEMON, DAEMON);
	printf("Functions:\n"
		   "\t-h\tPrint this exact help menu.\n"
		   "\t-p\tChange port to listen on (default: 9118).\n"
		   "\t-V\tEnable debug messages to stdout (this is ugly).\n"
		   "\t-v\tPrint out version information.\n\n"
		   "\t-d\tRemain in foreground - implies -V.\n\n");
}

/*
 * description: make sure that the daemon is not already running.
 */
int proc_find(void)
{
	DIR *dir;
	struct dirent *list;
	char buf[NAMELEN * 2] = {0}, name[NAMELEN] = {0}, state;
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

int main(int argc, char *argv[])
{
	pid_t pid;
	int rc = SUCCESS, i;
	char *path = NULL;
	int stdin_f = -1, stdout_f = -1, stderr_f = -1;
	struct PTHREAD_ARGS *args = NULL;
	bool verbose = 0, debug = 0;
	char port[6] = {0}; // max port is 65535 so 5 chars + 1 (NULL)
	char *dirnamed_path = NULL;

//	get_status();
//	char *str = preg_replace("a", "b", "a");
//	printf("%s\n", str);
//	if (str) free(str);
//	exit(0);
#ifndef DEBUG
	if (getuid() != 0) {
		fprintf(stderr, "\nYou must be root or contain sudo permissions to initiate this\n\n");
		return -EACCES;
	}
#endif

	while ((i = getopt(argc, argv, "?hp:vVd")) != INVALID_VALUE) {
		switch (i) {
			case '?':
			case 'h':
				online_menu();
				return SUCCESS;
			case 'p':
				sprintf(port, "%s", optarg);
				break;
			case 'v':
				printf("%s %s\n%s\n\n", DAEMON, VERSION_NUM, COPYRIGHT);
				return SUCCESS;
			case 'V':
				verbose = 1;
				break;
			case 'd':
				verbose = 1;
				debug = 1;
				break;
			default:
				break;
		}
	}

	if (check_loaded_modules() < SUCCESS) {
		if (verbose)
			fprintf(stderr, "%s: The needed modules are not loaded...\n", __func__);
		syslog(LOG_ERR, "%s: %s: The needed modules are not loaded...\n", DAEMON, __func__);
		syslog(LOG_ERR, "%s: Daemon exiting.\n", DAEMON);
		return -EPERM;
	}

	/* Make sure that only a single instance is running */
	if (proc_find() != SUCCESS) {
		if (verbose)
			fprintf(stderr, "%s: The daemon is already running...\n", __func__);
		syslog(LOG_ERR, "%s: %s: The daemon is already running...\n", DAEMON, __func__);
		syslog(LOG_ERR, "%s: Daemon exiting.\n", DAEMON);
		return INVALID_VALUE;
	}

	/*
	 * We need to call realpath and dirpath before forking and calling chdir("/"). Since args will be
	 * allocated after fork() we use temp buffers ('path' allocated by realpath and 'dirnamed_path' allocated
	 * with strdup()) until args is available.
	*/
	if ((path = realpath(argv[0], NULL)) == NULL) {
		syslog(LOG_ERR, "%s, %s: realpath: %s - %s\n", DAEMON, __func__, strerror(errno), argv[0]);
		syslog(LOG_ERR, "%s: Daemon exiting.\n", DAEMON);
		if (verbose)
			fprintf(stderr,"%s, %s: realpath: %s\n", DAEMON, __func__, strerror(errno));
		return -EIO;
	}

	/*
	 * dirname() needs a duplicated buffer to work with, which must be freed later
	*/
	dirnamed_path = strdup(path);
	dirnamed_path = dirname(dirnamed_path);
	if (path) free(path);
	path = NULL;

	/*
	 * if debug is not set, daemonize and redirect STDIN/OUT/ERR to null or to a file if verbose is set
	*/
	if (!debug) {
		/*
		 * Double fork to prevent any chance for the daemon to regain a terminal
		 */
		pid = fork();
		if (pid > 0) {
			syslog(LOG_INFO, "%s: Non-Daemon exiting.\n", DAEMON);
			if (verbose)
				fprintf(stderr, "%s: Non-Daemon exiting.\n", DAEMON);
			if (dirnamed_path) free(dirnamed_path);
			exit(0);
		}
		setsid();
		pid = fork();
		if (pid > 0) {
			syslog(LOG_INFO, "%s: Second Non-Daemon exiting.\n", DAEMON);
			if (verbose)
				fprintf(stderr, "%s: Second Non-Daemon exiting.\n", DAEMON);
			if (dirnamed_path) free(dirnamed_path);
			exit(0);
		}

		chdir("/");
		umask(0);

		/*
		 * Closing STDIN, STDOUT and STDERR
		 */
		if (close(STDIN_FILENO) < SUCCESS) {
			syslog(LOG_ERR, "%s: %s: close STDIN: %s\n", DAEMON, __func__, strerror(errno));
			syslog(LOG_ERR, "%s: Daemon exiting.\n", DAEMON);
			if (verbose) {
				fprintf(stderr, "%s, %s: Close STDIN: %s\n", DAEMON, __func__, strerror(errno));
				fprintf(stderr, "%s: Daemon exiting.\n", DAEMON);
			}
			if (dirnamed_path) free(dirnamed_path);
			return INVALID_VALUE;
		}
		if (close(STDOUT_FILENO) < SUCCESS) {
			syslog(LOG_ERR, "%s, %s: Close STDOUT: %s\n", DAEMON, __func__, strerror(errno));
			syslog(LOG_ERR, "%s: Daemon exiting.\n", DAEMON);
			if (verbose) {
				fprintf(stderr, "%s, %s: Close STDOUT: %s\n", DAEMON, __func__, strerror(errno));
				fprintf(stderr, "%s: Daemon exiting.\n", DAEMON);
			}
			if (dirnamed_path) free(dirnamed_path);
			return INVALID_VALUE;
		}
		if (close(STDERR_FILENO) < SUCCESS) {
			syslog(LOG_ERR, "%s, %s: Close STDERR: %s\n", DAEMON, __func__, strerror(errno));
			syslog(LOG_ERR, "%s: Daemon exiting.\n", DAEMON);
			if (verbose) {
				fprintf(stderr, "%s, %s: Close STERR: %s\n", DAEMON, __func__, strerror(errno));
				fprintf(stderr, "%s: Daemon exiting.\n", DAEMON);
			}
			if (dirnamed_path) free(dirnamed_path);
			return INVALID_VALUE;
		}

		/*
		 * Reopening STDIN, STDOUT and STDERR, pointing to "/dev/null" or to a file in verbose mode.
		 * File names are arbitrary/temporary and can be changed.
		 */
		if ((stdin_f = open("/dev/null", O_RDONLY)) < SUCCESS) {
			syslog(LOG_ERR, "%s: %s: open STDIN: %s\n", DAEMON, __func__, strerror(errno));
			syslog(LOG_ERR, "%s: Daemon exiting.\n", DAEMON);
			/* We can't log to STDERR yet (for verbose mode) */
			if (dirnamed_path) free(dirnamed_path);
			return INVALID_VALUE;
		}

		char *new_stdout = "/dev/null";
		if (verbose) {
			new_stdout = "/tmp/rapiddiskd.log";
		}
		if ((stdout_f = open(new_stdout,
							 O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < SUCCESS) {
			syslog(LOG_ERR, "%s, %s: Error opening STDOUT as %s: %s\n", DAEMON, __func__,
				   new_stdout, strerror(errno));
			syslog(LOG_ERR, "%s: Daemon exiting.\n", DAEMON);
			close(stdin_f);
			/* We can't log to STDERR yet (for verbose mode) */
			if (dirnamed_path) free(dirnamed_path);
			return INVALID_VALUE;
		}

		char *new_stderr = "/dev/null";
		if (verbose) {
			new_stderr = "/tmp/rapiddiskd_err.log";
		}
		if ((stderr_f = open(new_stderr, O_WRONLY | O_CREAT | O_TRUNC,
							 S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < SUCCESS) {
			syslog(LOG_ERR, "%s, %s: Error opening STDERR as %s: %s\n", DAEMON, __func__, new_stderr, strerror(errno));
			syslog(LOG_ERR, "%s: Daemon exiting.\n", DAEMON);
			/* We can't log to STDERR yet (for verbose mode) */
			close(stdin_f);
			close(stdout_f);
			if (dirnamed_path) free(dirnamed_path);
			return INVALID_VALUE;
		}
	}

	/* Allocating the args structure, this was moved after the fork to avoid duplicating/freeing it for every fork() */
	args = (struct PTHREAD_ARGS *)calloc(1, sizeof(struct PTHREAD_ARGS));
	if (args == NULL) {
		syslog(LOG_ERR, "%s, %s: calloc: %s\n", DAEMON, __func__, strerror(errno));
		syslog(LOG_ERR, "%s: Daemon exiting.\n", DAEMON);
		if (verbose) {
			fprintf(stderr, "%s, %s: calloc: %s\n", DAEMON, __func__, strerror(errno));
			fprintf(stderr, "%s: Daemon exiting.\n", DAEMON);
		}
		if (dirnamed_path) free(dirnamed_path);
		return -ENOMEM;
	}

	 /* Put some config values into args */
	args->verbose = verbose;
//	args->debug = debug;

	if (strlen(port) == 0) {
		sprintf(args->port, "%s", DEFAULT_MGMT_PORT);
	} else {
		sprintf(args->port, "%s", port);
	}

	sprintf(args->path, "%s", dirnamed_path);
	if (dirnamed_path) free(dirnamed_path);
	dirnamed_path = NULL;
	syslog(LOG_INFO, "%s: Starting daemon...\n", DAEMON);
	if (args->verbose)
		fprintf(stderr, "%s: Starting daemon...\n", DAEMON);
	rc = mgmt_thread((void *)args);
	syslog(LOG_INFO, "%s: Daemon exiting.\n", DAEMON);
	if (args->verbose)
		fprintf(stderr, "%s: Daemon exiting.\n", DAEMON);
	if (fflush(NULL) == EOF) {
		syslog(LOG_ERR, "%s: Error flushing file descriptors: %s, %s\n", DAEMON, strerror(errno), __func__);
		if (args->verbose)
			fprintf(stderr, "%s: Error flushing file descriptors: %s, %s.\n", DAEMON, strerror(errno), __func__);
		return INVALID_VALUE;
	}
	free(args);
	args = NULL;
	if (stdin_f >= 0) close(stdin_f);
	if (stdout_f >= 0) close(stdout_f);
	if (stderr_f >= 0) close(stderr_f);
	exit(rc);
}
