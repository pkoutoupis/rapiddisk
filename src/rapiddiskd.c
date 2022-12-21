/**
 * @copyright @verbatim
Copyright © 2011 - 2022 Petros Koutoupis

All rights reserved.

This file is part of RapidDisk.

RapidDisk is free software: you can redistribute it and/or modify@n
		it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

RapidDisk is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RapidDisk.  If not, see <http://www.gnu.org/licenses/>.

SPDX-License-Identifier: GPL-2.0-or-later
@endverbatim
* @author Petros Koutoupis \<petros\@petroskoutoupis.com\>
* @author Matteo Tenca \<matteo.tenca\@gmail.com\>
* @version 8.2.0
* @date 26 September 2022
*/

#define SERVER
#include "rapiddiskd.h"
#include "utils.h"

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
		   "\t-V\tEnable debug messages to stderr (this is ugly).\n"
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
		syslog(LOG_ERR|LOG_DAEMON, "%s (%d): opendir: %s.",
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
				syslog(LOG_ERR|LOG_DAEMON, "%s (%d): fscanf: %s.", __func__, __LINE__, strerror(errno));
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
	int stdin_f = -1, stdout_f = -1, stderr_f = -1;
	struct PTHREAD_ARGS *args = NULL;
	bool verbose = FALSE, debug = FALSE;
	char port[6] = {0}; // max port is 65535 so 5 chars + 1 (NULL)
	char msg[NAMELEN] = {0};

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
				/**
				 * This checks prevents a buffer overflow, and checks if the provided string
				 * can be converted to an int, and if the int is > 0 and < 65536.
				 * This is needed because libmicrohttpd would choose a random port if the provided one
				 * is 0 or > 65535. In case the port is in use, libmicrohttpd will return an (handled) error later.
				 */

				/**
				 * This very first check with strlen() allows to discriminate between
				 * strtol() errors due to conversion issues from the ones related to
				 * the number provided being too big.
				 */
				if (strlen(optarg) >= 6) {
					fprintf(stderr, "The provided port number should be > 0 and <= 65535.\n");
					return EXIT_FAILURE;
				}
				errno = 0;
				char *endptr = NULL;
				long port_int = strtol(optarg, &endptr, 10);
				if (errno != 0) {
					fprintf(stderr, "Error: %s\n", strerror(errno));
					return EXIT_FAILURE;
				}
				if (endptr == optarg) {
					fprintf(stderr, "The provided port is not a number.\n");
					return EXIT_FAILURE;
				}
				if ((port_int <= 0) || (port_int > 65535)) {
					fprintf(stderr, "The provided port number should be > 0 and <= 65535.\n");
					return EXIT_FAILURE;
				}
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

	openlog(DAEMON, LOG_PID, LOG_DAEMON);
	if (check_loaded_modules() < SUCCESS) {
		if (verbose) {
			fprintf(stderr, verbose_msg(msg, ERR_MODULES), DAEMON, __func__);
			fprintf(stderr, verbose_msg(msg, D_EXITING), DAEMON);
		}
		syslog(LOG_ERR|LOG_DAEMON, ERR_MODULES, __func__);
		syslog(LOG_ERR|LOG_DAEMON, D_EXITING);
		return -EPERM;
	}

	/* Make sure that only a single instance is running */
	if (proc_find() != SUCCESS) {
		if (verbose) {
			fprintf(stderr, verbose_msg(msg, ERR_ALREADY_RUNNING), DAEMON, __func__);
			fprintf(stderr, verbose_msg(msg, D_EXITING), DAEMON);
		}
		syslog(LOG_ERR|LOG_DAEMON, ERR_ALREADY_RUNNING, __func__);
		syslog(LOG_ERR|LOG_DAEMON, D_EXITING);
		return INVALID_VALUE;
	}

	/*
	 * if debug is not set, daemonize and redirect STDIN/OUT/ERR to null or to a file if verbose is set
	*/
	if (!debug) {
		/*
		 * Double fork to prevent any chance for the daemon to regain a terminal
		 */
		pid = fork();
		if (pid > 0) {
			/**
			 * This is the parent process
			 */
			if (verbose)
				fprintf(stderr, "%s: First Non-Daemon exiting.\n", DAEMON);
			closelog();
			fflush(NULL);
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			_exit(EXIT_SUCCESS);
		}
		setsid(); /** Sets the first child PID as Session ID */
		pid = fork();
		if (pid > 0) {
			/**
			 * This is the first child, which owns the Session
			 */
			if (verbose)
				fprintf(stderr, "%s: Second Non-Daemon exiting.\n", DAEMON);
			closelog();
			fflush(NULL);
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			_exit(EXIT_SUCCESS);
		}
		/**
		 * This is the second child, whose PID is different from its SID.
		 * This means it can't regain access to a terminal once and for all.
		 */

		chdir("/");
		umask(0);

		/**
		 * Closing STDIN, STDOUT and STDERR
		 */
		if (close(STDIN_FILENO) < SUCCESS) {
			syslog(LOG_ERR | LOG_DAEMON, "%s: Close STDIN: %s", __func__, strerror(errno));
			syslog(LOG_ERR | LOG_DAEMON, D_EXITING);
			if (verbose) {
				fprintf(stderr, "%s, %s: Close STDIN: %s\n", DAEMON, __func__, strerror(errno));
				fprintf(stderr, "%s: Daemon exiting.\n", DAEMON);
			}
			return INVALID_VALUE;
		}
		if (close(STDOUT_FILENO) < SUCCESS) {
			syslog(LOG_ERR | LOG_DAEMON, "%s: Close STDOUT: %s", __func__, strerror(errno));
			syslog(LOG_ERR | LOG_DAEMON, D_EXITING);
			if (verbose) {
				fprintf(stderr, "%s, %s: Close STDOUT: %s\n", DAEMON, __func__, strerror(errno));
				fprintf(stderr, "%s: Daemon exiting.\n", DAEMON);
			}
			return INVALID_VALUE;
		}
		if (close(STDERR_FILENO) < SUCCESS) {
			syslog(LOG_ERR | LOG_DAEMON, "%s: Close STDERR: %s", __func__, strerror(errno));
			syslog(LOG_ERR | LOG_DAEMON, D_EXITING);
			if (verbose) {
				fprintf(stderr, "%s, %s: Close STDERR: %s\n", DAEMON, __func__, strerror(errno));
				fprintf(stderr, "%s: Daemon exiting.\n", DAEMON);
			}
			return INVALID_VALUE;
		}

		/**
		 * Reopening STDIN, STDOUT and STDERR, pointing to "/dev/null" or to a file in verbose mode.
		 * File names are arbitrary/temporary and can be changed.
		 */
		if ((stdin_f = open("/dev/null", O_RDONLY)) < SUCCESS) {
			syslog(LOG_ERR | LOG_DAEMON, "%s: open STDIN: %s", __func__, strerror(errno));
			syslog(LOG_ERR | LOG_DAEMON, D_EXITING);
			/* We can't log to STDERR yet (for verbose mode) */
			return INVALID_VALUE;
		}

		char *new_stdout = "/dev/null";
		if (verbose) {
			new_stdout = D_STDOUT_LOG;
		}
		if ((stdout_f = open(new_stdout,
							 O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) <
			SUCCESS) {
			syslog(LOG_ERR | LOG_DAEMON, "%s: Error opening STDOUT as %s: %s", __func__,
				   new_stdout, strerror(errno));
			syslog(LOG_ERR | LOG_DAEMON, D_EXITING);
			close(stdin_f);
			/* We can't log to STDERR yet (for verbose mode) */
			return INVALID_VALUE;
		}

		char *new_stderr = "/dev/null";
		if (verbose) {
			new_stderr = D_STDERR_LOG;
		}
		if ((stderr_f = open(new_stderr, O_WRONLY | O_CREAT | O_TRUNC,
							 S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < SUCCESS) {
			syslog(LOG_ERR | LOG_DAEMON, "%s: Error opening STDERR as %s: %s", __func__, new_stderr,
				   strerror(errno));
			syslog(LOG_ERR | LOG_DAEMON, D_EXITING);
			/* We can't log to STDERR yet (for verbose mode) */
			close(stdin_f);
			close(stdout_f);
			return INVALID_VALUE;
		}

	}

	pid = getpid();
	FILE *fh = NULL;
	if ((fh = fopen(PID_FILE, "w")) != NULL) {
		if (fprintf(fh, "%d\n", pid) < 0) {
			syslog(LOG_ERR|LOG_DAEMON, "Impossible to write to pidfile /run/rapiddiskd.pid.");
			if (verbose) {
				fprintf(stderr, "%s: Impossible to write to pidfile /run/rapiddiskd.pid.\n", DAEMON);
			}
		}
		fclose(fh);
	} else {
		syslog(LOG_ERR|LOG_DAEMON, "Impossible to open pidfile /run/rapiddiskd.pid.");
		if (verbose) {
			fprintf(stderr, "%s: Impossible to open pidfile /run/rapiddiskd.pid.\n", DAEMON);
		}
	}

	/* Allocating the args structure, this was moved after the fork to avoid duplicating/freeing it for every fork() */
	args = (struct PTHREAD_ARGS *)calloc(1, sizeof(struct PTHREAD_ARGS));
	if (args == NULL) {
		syslog(LOG_ERR|LOG_DAEMON, ERR_CALLOC, __func__, strerror(errno));
		syslog(LOG_ERR|LOG_DAEMON, D_EXITING);
		if (verbose) {
			fprintf(stderr, verbose_msg(msg, ERR_CALLOC), DAEMON, __func__, strerror(errno));
			fprintf(stderr, verbose_msg(msg, D_EXITING), DAEMON);
		}
		return -ENOMEM;
	}

	 /** Put some config values into args */
	args->verbose = verbose;

	if (strlen(port) == 0) {
		sprintf(args->port, "%s", DEFAULT_MGMT_PORT);
	} else {
		sprintf(args->port, "%s", port);
	}

	syslog(LOG_INFO|LOG_DAEMON, D_STARTING);
	if (args->verbose)
		fprintf(stderr, verbose_msg(msg, D_STARTING), DAEMON, D_STARTING);

	rc = mgmt_thread((void *)args);
	if (fflush(NULL) == EOF) {
		syslog(LOG_ERR|LOG_DAEMON, ERR_FLUSHING, strerror(errno), __func__);
		if (args->verbose)
			fprintf(stderr, verbose_msg(msg, ERR_FLUSHING), DAEMON, strerror(errno), __func__);
		rc = INVALID_VALUE;
	}
	syslog(LOG_INFO|LOG_DAEMON, D_EXITING);
	if (args->verbose)
		fprintf(stderr, verbose_msg(msg, D_EXITING), DAEMON);

	free(args);
	args = NULL;
	if (stdin_f == 0) close(stdin_f);
	if (stdout_f > 0) close(stdout_f);
	if (stderr_f > 0) close(stderr_f);
	closelog();
	if (access(PID_FILE, F_OK) == 0) {
		unlink(PID_FILE);
	}
	exit(rc);
}
