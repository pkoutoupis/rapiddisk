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
 ** @filename: net.c
 ** @description: This file contains the function to build and handle JSON objects.
 **
 ** @date: 2Aug20, petros@petroskoutoupis.com
 ********************************************************************************/

#include "common.h"
#include "daemon.h"
#include <jansson.h>
#include <sys/socket.h>
#include <linux/version.h>
#include <microhttpd.h>

bool verbose;

/*
 * The responses to our GET requests. Although, we are not SPECIFICALLY checking that they are GETs.
 * We are just check the URL and the string command.
 */
static int answer_to_connection(void *cls, struct MHD_Connection *connection, const char *url,
                                const char *method, const char *version, const char *upload_data,
                                 size_t *upload_data_size, void **con_cls)
{
	struct MHD_Response *response;
	int rc;

	unsigned char *page = (unsigned char *)calloc(1, BUFSZ);
	if (page == NULL) {
		printf("%s: %s: calloc: %s\n", DAEMON, __func__, strerror(errno));
		return INVALID_VALUE;
	}

/*
	if (strcmp(url, CMD_PING_DAEMON) == SUCCESS) {
		json_check_status(page);
	} else if (strcmp(url, CMD_LIST_VOLUMES) == SUCCESS) {
		json_list_devices(volumes, page);
	}
*/

answer_to_connection_out:
	response = MHD_create_response_from_buffer(strlen(page), (void *)page, MHD_RESPMEM_MUST_COPY);
	rc = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
	if (page) free(page);

	return rc;
}

/*
 * description: the thread that listens for network or cli requests.
 */
void *mgmt_thread(void *arg)
{
	struct PTHREAD_ARGS *args = (struct PTHREAD_ARGS *)arg;
	verbose = args->verbose;

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
