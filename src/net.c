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
 ** @filename: net.c
 ** @description: This file contains the function to build and handle JSON objects.
 **
 ** @date: 2Aug20, petros@petroskoutoupis.com
 ********************************************************************************/

#include "common.h"
#include "daemon.h"
#include <jansson.h>
#include <sys/socket.h>
#include <microhttpd.h>

bool verbose;
unsigned char path[NAMELEN] = {0};

/*
 * The responses to our GET requests. Although, we are not SPECIFICALLY checking that they are GETs.
 * We are just check the URL and the string command.
 */
#if MHD_VERSION >= 0x00097100
static enum MHD_Result answer_to_connection(void *cls, struct MHD_Connection *connection, const char *url,
#else
static int answer_to_connection(void *cls, struct MHD_Connection *connection, const char *url,
#endif
                                const char *method, const char *version, const char *upload_data,
                                 size_t *upload_data_size, void **con_cls)
{
	struct MHD_Response *response;
	int rc, status = MHD_HTTP_OK;
	unsigned long long size = 0;
	unsigned char device[NAMELEN] = {0}, source[NAMELEN] = {0}, command[NAMELEN * 4] = {0};
	unsigned char *dup = NULL, *token = NULL;
	FILE *stream;

	unsigned char *page = (unsigned char *)calloc(1, BUFSZ);
	if (page == NULL) {
		printf("%s: %s: calloc: %s\n", DAEMON, __func__, strerror(errno));
#if MHD_VERSION >= 0x00097100
		return MHD_NO;
#else
		return INVALID_VALUE;
#endif
	}

	if (strcmp(method, "GET") == SUCCESS) {
		if (strcmp(url, CMD_PING_DAEMON) == SUCCESS) {
			json_status_check(page);
		} else if (strcmp(url, CMD_LIST_RESOURCES) == SUCCESS) {
			sprintf(command, "%s/rapiddisk -q -j -g", path);
			stream = popen(command, "r");
			if (stream) {
				while (fgets(page, BUFSZ, stream) != NULL);
				pclose(stream);
			} else
				status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		} else if (strcmp(url, CMD_LIST_RD_VOLUMES) == SUCCESS) {
			sprintf(command, "%s/rapiddisk -l -j -g", path);
			stream = popen(command, "r");
			if (stream) {
				while (fgets(page, BUFSZ, stream) != NULL);
				pclose(stream);
			} else
				status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		} else if (strncmp(url, CMD_RCACHE_STATS, sizeof(CMD_RCACHE_STATS) - 1) == SUCCESS) {
			dup = strdup(url);
			token = strtok((char *)dup, "/");
			token = strtok(NULL, "/");    /* skip first "/" delimeter */
			token = strtok(NULL, "/");    /* and second delimeter     */
			if (!token) {
				status = MHD_HTTP_BAD_REQUEST;
				goto answer_to_connection_out;
			}
			sprintf(command, "%s/rapiddisk -s %s -j -g", path, token);
			stream = popen(command, "r");
			if (stream) {
				while (fgets(page, BUFSZ, stream) != NULL);
				pclose(stream);
			} else
				status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		} else if (strncmp(url, CMD_LIST_NVMET, sizeof(CMD_LIST_NVMET) - 1) == SUCCESS) {
			sprintf(command, "%s/rapiddisk -n -j -g", path);
			stream = popen(command, "r");
			if (stream) {
				while (fgets(page, BUFSZ, stream) != NULL);
				pclose(stream);
			} else
				status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		} else if (strncmp(url, CMD_LIST_NVMET_PORTS, sizeof(CMD_LIST_NVMET_PORTS) - 1) == SUCCESS) {
			sprintf(command, "%s/rapiddisk -N -j -g", path);
			stream = popen(command, "r");
			if (stream) {
				while (fgets(page, BUFSZ, stream) != NULL);
				pclose(stream);
			} else
				status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		} else {
			json_status_unsupported(page);
			status = MHD_HTTP_BAD_REQUEST;
		}
	} else if (strcmp(method, "POST") == SUCCESS) {
		if ((strncmp(url, CMD_RDSK_CREATE, sizeof(CMD_RDSK_CREATE) - 1) == SUCCESS) && \
		    (strncmp(url, CMD_RCACHE_CREATE, sizeof(CMD_RCACHE_CREATE) - 1) != SUCCESS)) {
			dup = strdup(url);
			token = strtok((char *)dup, "/");
			token = strtok(NULL, "/");    /* skip first "/" delimeter */
			token = strtok(NULL, "/");    /* and second delimeter     */
			if (!token) {
				status = MHD_HTTP_BAD_REQUEST;
				goto answer_to_connection_out;
			}
			size = strtoull(token, (char **)NULL, 10);
			sprintf(command, "%s/rapiddisk -a %llu -j -g", path, size);
			stream = popen(command, "r");
			if (stream) {
				while (fgets(page, BUFSZ, stream) != NULL);
				pclose(stream);
			} else
				status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		} else if ((strncmp(url, CMD_RDSK_REMOVE, sizeof(CMD_RDSK_REMOVE) - 1) == SUCCESS) && \
			   (strncmp(url, CMD_RCACHE_REMOVE, sizeof(CMD_RCACHE_REMOVE) - 1) != SUCCESS)) {
			dup = strdup(url);
			token = strtok((char *)dup, "/");
			token = strtok(NULL, "/");    /* skip first "/" delimeter */
			token = strtok(NULL, "/");    /* and second delimeter     */
			if (!token) {
				status = MHD_HTTP_BAD_REQUEST;
				goto answer_to_connection_out;
			}
			sprintf(command, "%s/rapiddisk -d %s -j -g", path, token);
			stream = popen(command, "r");
			if (stream) {
				while (fgets(page, BUFSZ, stream) != NULL);
				pclose(stream);
			} else
				status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		} else if (strncmp(url, CMD_RDSK_RESIZE, sizeof(CMD_RDSK_RESIZE) - 1) == SUCCESS) {
			dup = strdup(url);
			token = strtok((char *)dup, "/");
			token = strtok(NULL, "/");    /* skip first "/" delimeter */
			token = strtok(NULL, "/");    /* and second delimeter     */
			if (!token) {
				status = MHD_HTTP_BAD_REQUEST;
				goto answer_to_connection_out;
			}
			sprintf(device, "%s", token);
			token = strtok(NULL, "/");    /* time to get the size     */
			if (!token) {
				status = MHD_HTTP_BAD_REQUEST;
				goto answer_to_connection_out;
			}
			size = strtoull(token, (char **)NULL, 10);
			sprintf(command, "%s/rapiddisk -r %s -c %llu -j -g", path, device, size);
			stream = popen(command, "r");
			if (stream) {
				while (fgets(page, BUFSZ, stream) != NULL);
				pclose(stream);
			} else
				status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		} else if (strncmp(url, CMD_RDSK_FLUSH, sizeof(CMD_RDSK_FLUSH) - 1) == SUCCESS) {
			dup = strdup(url);
			token = strtok((char *)dup, "/");
			token = strtok(NULL, "/");    /* skip first "/" delimeter */
			token = strtok(NULL, "/");    /* and second delimeter     */
			if (!token) {
				status = MHD_HTTP_BAD_REQUEST;
				goto answer_to_connection_out;
			}
			sprintf(command, "%s/rapiddisk -f %s -j -g", path, token);
			stream = popen(command, "r");
			if (stream) {
				while (fgets(page, BUFSZ, stream) != NULL);
				pclose(stream);
			} else
				status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		} else if (strncmp(url, CMD_RCACHE_CREATE, sizeof(CMD_RCACHE_CREATE) - 1) == SUCCESS) {
			dup = strdup(url);
			token = strtok((char *)dup, "/");
			token = strtok(NULL, "/");    /* skip first "/" delimeter */
			token = strtok(NULL, "/");    /* and second delimeter     */
			if (!token) {
				status = MHD_HTTP_BAD_REQUEST;
				goto answer_to_connection_out;
			}
			sprintf(device, "%s", token);
			token = strtok(NULL, "/");    /* get the backing store    */
			if (!token) {
				status = MHD_HTTP_BAD_REQUEST;
				goto answer_to_connection_out;
			}
			sprintf(source, "%s", token);
			token = strtok(NULL, "/");    /* get the caching policy   */
			if (!token) {
				status = MHD_HTTP_BAD_REQUEST;
				goto answer_to_connection_out;
			}
			if (strcmp(token, "write-through") == SUCCESS) {
				sprintf(command, "%s/rapiddisk -m %s -b /dev/%s -p wt -j -g", path, device, source);
			} else if (strcmp(token, "write-around") == SUCCESS) {
				sprintf(command, "%s/rapiddisk -m %s -b /dev/%s -p wa -j -g", path, device, source);
			} else if (strcmp(token, "writeback") == SUCCESS) {
				sprintf(command, "%s/rapiddisk -m %s -b /dev/%s -p wb -j -g", path, device, source);
			} else {
				status = MHD_HTTP_BAD_REQUEST;
				goto answer_to_connection_out;
			}
			stream = popen(command, "r");
			if (stream) {
				while (fgets(page, BUFSZ, stream) != NULL);
				pclose(stream);
			} else
				status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		} else if (strncmp(url, CMD_RCACHE_REMOVE, sizeof(CMD_RCACHE_REMOVE) - 1) == SUCCESS) {
			dup = strdup(url);
			token = strtok((char *)dup, "/");
			token = strtok(NULL, "/");    /* skip first "/" delimeter */
			token = strtok(NULL, "/");    /* and second delimeter     */
			if (!token) {
				status = MHD_HTTP_BAD_REQUEST;
				goto answer_to_connection_out;
			}
			sprintf(command, "%s/rapiddisk -u %s -j -g", path, token);
			stream = popen(command, "r");
			if (stream) {
				while (fgets(page, BUFSZ, stream) != NULL);
				pclose(stream);
			} else
				status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		} else if (strncmp(url, CMD_RDSK_LOCK, sizeof(CMD_RDSK_LOCK) - 1) == SUCCESS) {
			dup = strdup(url);
			token = strtok((char *)dup, "/");
			token = strtok(NULL, "/");    /* skip first "/" delimeter */
			token = strtok(NULL, "/");    /* and second delimeter     */
			if (!token) {
				status = MHD_HTTP_BAD_REQUEST;
				goto answer_to_connection_out;
			}
			sprintf(command, "%s/rapiddisk -L %s -j -g", path, token);
			stream = popen(command, "r");
			if (stream) {
				while (fgets(page, BUFSZ, stream) != NULL);
				pclose(stream);
			} else
				status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		} else if (strncmp(url, CMD_RDSK_UNLOCK, sizeof(CMD_RDSK_UNLOCK) - 1) == SUCCESS) {
			dup = strdup(url);
			token = strtok((char *)dup, "/");
			token = strtok(NULL, "/");    /* skip first "/" delimeter */
			token = strtok(NULL, "/");    /* and second delimeter     */
			if (!token) {
				status = MHD_HTTP_BAD_REQUEST;
				goto answer_to_connection_out;
			}
			sprintf(command, "%s/rapiddisk -U %s -j -g", path, token);
			stream = popen(command, "r");
			if (stream) {
				while (fgets(page, BUFSZ, stream) != NULL);
				pclose(stream);
			} else
				status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		} else {
			json_status_unsupported(page);
			status = MHD_HTTP_BAD_REQUEST;
		}
	} else
		status = MHD_HTTP_BAD_REQUEST;

answer_to_connection_out:
	response = MHD_create_response_from_buffer(strlen(page), (void *)page, MHD_RESPMEM_MUST_COPY);
	rc = MHD_queue_response (connection, status, response);
	MHD_destroy_response (response);
	if (page) free(page);
	if (dup) free(dup);

	return rc;
}

/*
 * description: the thread that listens for network or cli requests.
 */
void *mgmt_thread(void *arg)
{
	struct PTHREAD_ARGS *args = (struct PTHREAD_ARGS *)arg;
	verbose = args->verbose;
	sprintf(path, "%s", args->path);

	struct MHD_Daemon *daemon;

	daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, atoi(args->port), NULL, NULL,
				  &answer_to_connection, NULL, MHD_OPTION_END);
	if (daemon == NULL)
		goto exit_on_failure;

	while (1) sleep(1);      /* I need to figure out a better way to keep this thread alive after MHD_start_daemon. */

	MHD_stop_daemon(daemon); /* This part is pointless until I figure out the above. */

exit_on_failure:
	syslog(LOG_INFO, "%s: Management thread exiting: %s.\n", DAEMON, __func__);
}
