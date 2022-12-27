/**
 * @file net.c
 * @brief Daemon functions implementation
 * @details This file contains all the function related to the daemon
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
#include "rdsk.h"
#include "sys.h"
#include "json.h"
#include "nvmet.h"
#include <jansson.h>
#include <microhttpd.h>
#include <signal.h>

int server_stop_requested = 0;

/**
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
	unsigned long long size;
	char device[NAMELEN] = {0}, source[NAMELEN] = {0};
	char *dup = NULL, *token = NULL;
	char *json_str = NULL;
	struct RD_PROFILE *disk = NULL;
	struct RC_PROFILE *cache = NULL;
	struct VOLUME_PROFILE *volumes = NULL;
	struct MEM_PROFILE *mem = NULL;
	struct PTHREAD_ARGS *args = (struct PTHREAD_ARGS *) cls;
	char *error_message = calloc(1, NAMELEN);
	char *split_arr[64] = {NULL};
	char msg[NAMELEN] = {0};

	if (strcmp(method, "GET") == SUCCESS) {
		if (strcmp(url, CMD_PING_DAEMON) == SUCCESS) {
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, D_RECV_REQ), DAEMON, CMD_PING_DAEMON);
			json_status_check(&json_str);
		} else if (strcmp(url, CMD_LIST_RESOURCES) == SUCCESS) {
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, D_RECV_REQ), DAEMON, CMD_LIST_RESOURCES);
			volumes = search_volumes_targets(error_message);
			if ((volumes == NULL) && (strlen(error_message) != 0)) {
				rc = INVALID_VALUE;
				syslog(LOG_ERR|LOG_DAEMON, "%s.", error_message);
				if (args->verbose)
					fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
				json_status_return(rc, error_message, &json_str, TRUE);
			} else {
				mem = (struct MEM_PROFILE *) calloc(1, sizeof(struct MEM_PROFILE));
				if ((rc = get_memory_usage(mem, error_message)) == INVALID_VALUE) {
					syslog(LOG_ERR|LOG_DAEMON, "%s.", error_message);
					if (args->verbose)
						fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
					json_status_return(rc, error_message, &json_str, TRUE);
				} else {
					json_resources_list(mem, volumes, &json_str, TRUE);
				}
				if (mem) free(mem);
				mem = NULL;
				free_linked_lists(NULL, NULL, volumes);
				volumes = NULL;
			}
		} else if (strcmp(url, CMD_LIST_RD_VOLUMES) == SUCCESS) {
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, D_RECV_REQ), DAEMON, CMD_LIST_RD_VOLUMES);
			disk = search_rdsk_targets(error_message);
			if ((disk == NULL) && (strlen(error_message) != 0)) {
				syslog(LOG_ERR|LOG_DAEMON, "%s", error_message);
				if (args->verbose)
					fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
				json_status_return(INVALID_VALUE, error_message, &json_str, TRUE);
			} else {
				cache = search_cache_targets(error_message);
				if ((cache == NULL) && (strlen(error_message) != 0)) {
					syslog(LOG_ERR|LOG_DAEMON, "%s", error_message);
					if (args->verbose)
						fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
					json_status_return(INVALID_VALUE, error_message, &json_str, TRUE);
					free_linked_lists(NULL, disk, NULL);
					disk = NULL;
				} else {
					json_device_list(disk, cache, &json_str, TRUE);
					free_linked_lists(cache, disk, NULL);
					cache = NULL;
					disk = NULL;
				}
			}
		} else if (strncmp(url, CMD_RCACHE_STATS, sizeof(CMD_RCACHE_STATS) - 1) == SUCCESS) {
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, D_RECV_REQ), DAEMON, CMD_RCACHE_STATS);
			dup = strdup(url);
			preg_replace(CMD_RCACHE_STATS, "", dup, dup, strlen(dup));
			int last = split(dup, split_arr, "/");
			if (last != 0) {
				status = MHD_HTTP_BAD_REQUEST;
				syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_MALFORMED);
				if (args->verbose)
					fprintf(stderr, verbose_msg(msg, ERR_MALFORMED), DAEMON);
				json_status_return(INVALID_VALUE, ERR_MALFORMED, &json_str, TRUE);
			} else {
				token = split_arr[last];
				if (!token) {
					status = MHD_HTTP_BAD_REQUEST;
					syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_INVALIDURL);
					if (args->verbose)
						fprintf(stderr, verbose_msg(msg, ERR_INVALIDURL), DAEMON);
					json_status_return(INVALID_VALUE, ERR_INVALIDURL, &json_str, TRUE);
				} else {
					if (strstr(token, "rc-wb") != NULL) {
						struct WC_STATS *stats = dm_get_status(token, WRITEBACK);
						if (stats) {
							json_cache_wb_statistics(stats, &json_str, TRUE);
							free(stats);
						} else {
							status = MHD_HTTP_BAD_REQUEST;
							syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_DEV_STATUS);
							if (args->verbose)
								fprintf(stderr, verbose_msg(msg, ERR_DEV_STATUS), DAEMON);
							json_status_return(INVALID_VALUE, ERR_DEV_STATUS, &json_str, TRUE);
						}
					} else {
						struct RC_STATS *stats = dm_get_status(token, WRITETHROUGH);
						if (stats) {
							json_cache_statistics(stats, &json_str, TRUE);
							free(stats);
						} else {
							status = MHD_HTTP_BAD_REQUEST;
							syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_DEV_STATUS);
							if (args->verbose)
								fprintf(stderr, verbose_msg(msg, ERR_DEV_STATUS), DAEMON);
							json_status_return(INVALID_VALUE, ERR_DEV_STATUS, &json_str, TRUE);
						}
					}
				}
			}
		} else if (strncmp(url, CMD_LIST_NVMET, sizeof(CMD_LIST_NVMET) - 1) == SUCCESS) {
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, D_RECV_REQ), DAEMON, CMD_LIST_NVMET);
			rc = nvmet_view_exports_json(error_message, &json_str);
			if (rc != SUCCESS) {
				json_status_return(rc, error_message, &json_str, TRUE);
			}
		} else if (strncmp(url, CMD_LIST_NVMET_PORTS, sizeof(CMD_LIST_NVMET_PORTS) - 1) == SUCCESS) {
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, D_RECV_REQ), DAEMON, CMD_LIST_NVMET_PORTS);
			rc = nvmet_view_ports_json(error_message, &json_str);
			if (rc != SUCCESS) {
				json_status_return(rc, error_message, &json_str, TRUE);
			}
		} else {
			syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_UNSUPPORTED);
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, ERR_UNSUPPORTED), DAEMON);
			json_status_return(INVALID_VALUE, ERR_UNSUPPORTED, &json_str, TRUE);
			status = MHD_HTTP_BAD_REQUEST;
		}
	} else if (strcmp(method, "POST") == SUCCESS) {
		if ((strncmp(url, CMD_RDSK_CREATE, sizeof(CMD_RDSK_CREATE) - 1) == SUCCESS) && \
		    (strncmp(url, CMD_RCACHE_CREATE, sizeof(CMD_RCACHE_CREATE) - 1) != SUCCESS)) {
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, D_RECV_REQ), DAEMON, CMD_RDSK_CREATE);
			dup = strdup(url);
			preg_replace(CMD_RDSK_CREATE, "", dup, dup, strlen(dup));
			int last = split(dup, split_arr, "/");
			if (last != 0) {
				status = MHD_HTTP_BAD_REQUEST;
				syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_MALFORMED);
				if (args->verbose)
					fprintf(stderr, verbose_msg(msg, ERR_MALFORMED), DAEMON);
				json_status_return(INVALID_VALUE, ERR_MALFORMED, &json_str, TRUE);
			} else {
				token = split_arr[last];
				if (!token) {
					status = MHD_HTTP_BAD_REQUEST;
					syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_INVALIDURL);
					if (args->verbose)
						fprintf(stderr, verbose_msg(msg, ERR_INVALIDURL), DAEMON);
					json_status_return(INVALID_VALUE, NULL, &json_str, TRUE);
				} else {
					errno = 0;
					char *end_ptr = NULL;
					size = strtoull(token, &end_ptr, 10);
					if (errno != 0) {
						syslog(LOG_ERR|LOG_DAEMON, "%s, %s", ERR_INVALID_SIZE, strerror(errno));
						if (args->verbose)
							fprintf(stderr, verbose_msg(msg, "%s, %s"), DAEMON, ERR_INVALID_SIZE, strerror(errno));
						json_status_return(INVALID_VALUE, ERR_INVALID_SIZE, &json_str, TRUE);
					} else if (end_ptr == token) {
						syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_NOTANUMBER);
						if (args->verbose)
							fprintf(stderr, verbose_msg(msg, ERR_NOTANUMBER), DAEMON);
						json_status_return(INVALID_VALUE, ERR_NOTANUMBER, &json_str, TRUE);
					} else {
						disk = search_rdsk_targets(error_message);
						if ((disk == NULL) && (strlen(error_message) != 0)) {
							syslog(LOG_ERR | LOG_DAEMON, "%s", error_message);
							if (args->verbose)
								fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
							json_status_return(INVALID_VALUE, error_message, &json_str, TRUE);
						} else {
							rc = mem_device_attach(disk, size, error_message);
							json_status_return(rc, error_message, &json_str, TRUE);
							int pri = LOG_INFO;
							if (rc < SUCCESS)
								pri = LOG_ERR;
							syslog(pri | LOG_DAEMON, "%s", error_message);
							if (args->verbose)
								fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
							free_linked_lists(NULL, disk, NULL);
							disk = NULL;
						}
					}
				}
			}
		} else if ((strncmp(url, CMD_RDSK_REMOVE, sizeof(CMD_RDSK_REMOVE) - 1) == SUCCESS) && \
			   (strncmp(url, CMD_RCACHE_REMOVE, sizeof(CMD_RCACHE_REMOVE) - 1) != SUCCESS)) {
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, D_RECV_REQ), DAEMON, CMD_RCACHE_REMOVE);
			dup = strdup(url);
			preg_replace(CMD_RDSK_REMOVE, "", dup, dup, strlen(dup));
			int last = split(dup, split_arr, "/");
			if (last != 0) {
				status = MHD_HTTP_BAD_REQUEST;
				syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_MALFORMED);
				if (args->verbose)
					fprintf(stderr, verbose_msg(msg, ERR_MALFORMED), DAEMON);
				json_status_return(INVALID_VALUE, ERR_MALFORMED, &json_str, TRUE);
			} else {
				token = split_arr[last];
				if (!token) {
					status = MHD_HTTP_BAD_REQUEST;
					syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_INVALIDDEVNAME);
					if (args->verbose)
						fprintf(stderr, verbose_msg(msg, ERR_INVALIDDEVNAME), DAEMON);
					json_status_return(INVALID_VALUE, ERR_INVALIDDEVNAME, &json_str, TRUE);
				} else {
					disk = search_rdsk_targets(error_message);
					if ((disk == NULL) && (strlen(error_message) != 0)) {
						syslog(LOG_ERR|LOG_DAEMON, "%s", error_message);
						if (args->verbose)
							fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
						json_status_return(INVALID_VALUE, error_message, &json_str, TRUE);
					} else {
						cache = search_cache_targets(error_message);
						if (cache == NULL && strlen(error_message) != 0) {
							syslog(LOG_ERR|LOG_DAEMON, "%s", error_message);
							if (args->verbose)
								fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
							json_status_return(INVALID_VALUE, error_message, &json_str, TRUE);
							free_linked_lists(NULL, disk, NULL);
							disk = NULL;
						} else {
							rc = mem_device_detach(disk, cache, token, error_message);
							int pri = LOG_INFO;
							if (rc < SUCCESS)
								pri = LOG_ERR;
							syslog(pri|LOG_DAEMON, "%s", error_message);
							if (args->verbose)
								fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
							json_status_return(rc, error_message, &json_str, TRUE);
							free_linked_lists(cache, disk, NULL);
							disk = NULL;
							cache = NULL;
						}
					}
				}
			}
		} else if (strncmp(url, CMD_RDSK_RESIZE, sizeof(CMD_RDSK_RESIZE) - 1) == SUCCESS) {
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, D_RECV_REQ), DAEMON, CMD_RDSK_RESIZE);
			dup = strdup(url);
			preg_replace(CMD_RDSK_RESIZE, "", dup, dup, strlen(dup));
			int last = split(dup, split_arr, "/");
			if (last != 1) {
				status = MHD_HTTP_BAD_REQUEST;
				syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_MALFORMED);
				if (args->verbose)
					fprintf(stderr, verbose_msg(msg, ERR_MALFORMED), DAEMON);
				json_status_return(INVALID_VALUE, ERR_MALFORMED, &json_str, TRUE);
			} else {
				token = split_arr[last - 1];
				if (!token) {
					status = MHD_HTTP_BAD_REQUEST;
					syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_INVALIDURL);
					if (args->verbose)
						fprintf(stderr, verbose_msg(msg, ERR_INVALIDURL), DAEMON);
					json_status_return(INVALID_VALUE, NULL, &json_str, TRUE);
				} else {
					sprintf(device, "%s", token);
					token = split_arr[last];    /* time to get the size     */
					if (!token) {
						status = MHD_HTTP_BAD_REQUEST;
						syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_INVALIDURL);
						if (args->verbose)
							fprintf(stderr, verbose_msg(msg, ERR_INVALIDURL), DAEMON);
						json_status_return(INVALID_VALUE, NULL, &json_str, TRUE);
					} else {
						char *end_ptr = NULL;
						errno = 0;
						size = strtoull(token, &end_ptr, 10);
						if (errno != 0) {
							syslog(LOG_ERR|LOG_DAEMON, "%s, %s", ERR_INVALID_SIZE, strerror(errno));
							if (args->verbose)
								fprintf(stderr, verbose_msg(msg, "%s, %s"), DAEMON, ERR_INVALID_SIZE, strerror(errno));
							json_status_return(INVALID_VALUE, ERR_INVALID_SIZE, &json_str, TRUE);
						} else if (end_ptr == token) {
							syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_NOTANUMBER);
							if (args->verbose)
								fprintf(stderr, verbose_msg(msg, ERR_NOTANUMBER), DAEMON);
							json_status_return(INVALID_VALUE, ERR_NOTANUMBER, &json_str, TRUE);
						} else {
							if ((disk = search_rdsk_targets(error_message)) == NULL) {
								syslog(LOG_ERR | LOG_DAEMON, "%s", error_message);
								if (args->verbose)
									fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
								json_status_return(INVALID_VALUE, error_message, &json_str, TRUE);
							} else {
								rc = mem_device_resize(disk, device, size, error_message);
								int pri = LOG_INFO;
								if (rc < SUCCESS)
									pri = LOG_ERR;
								syslog(pri | LOG_DAEMON, "%s", error_message);
								if (args->verbose)
									fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
								json_status_return(rc, error_message, &json_str, TRUE);
								free_linked_lists(NULL, disk, NULL);
								disk = NULL;
							}
						}
					}
				}
			}
		} else if (strncmp(url, CMD_RDSK_FLUSH, sizeof(CMD_RDSK_FLUSH) - 1) == SUCCESS) {
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, D_RECV_REQ), DAEMON, CMD_RDSK_FLUSH);
			dup = strdup(url);
			preg_replace(CMD_RDSK_FLUSH, "", dup, dup, strlen(dup));
			int last = split(dup, split_arr, "/");
			if (last != 0) {
				status = MHD_HTTP_BAD_REQUEST;
				syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_MALFORMED);
				if (args->verbose)
					fprintf(stderr, verbose_msg(msg, ERR_MALFORMED), DAEMON);
				json_status_return(INVALID_VALUE, ERR_MALFORMED, &json_str, TRUE);
			} else {
				token = split_arr[last];
				if (!token) {
					status = MHD_HTTP_BAD_REQUEST;
					syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_INVALIDDEVNAME);
					if (args->verbose)
						fprintf(stderr, verbose_msg(msg, ERR_INVALIDDEVNAME), DAEMON);
					json_status_return(INVALID_VALUE, ERR_INVALIDDEVNAME, &json_str, TRUE);
				} else {
					disk = search_rdsk_targets(error_message);
					if ((disk == NULL) && (strlen(error_message) != 0)) {
						syslog(LOG_ERR|LOG_DAEMON, "%s", error_message);
						if (args->verbose)
							fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
						json_status_return(INVALID_VALUE, error_message, &json_str, TRUE);
					} else {
						cache = search_cache_targets(error_message);
						if (cache == NULL && strlen(error_message) != 0) {
							syslog(LOG_ERR|LOG_DAEMON, "%s", error_message);
							if (args->verbose)
								fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
							json_status_return(INVALID_VALUE, error_message, &json_str, TRUE);
							free_linked_lists(NULL, disk, NULL);
							disk = NULL;
						} else {
							rc = mem_device_flush(disk, cache, token, error_message);
							int pri = LOG_INFO;
							if (rc < SUCCESS)
								pri = LOG_ERR;
							syslog(pri|LOG_DAEMON, "%s", error_message);
							if (args->verbose)
								fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
							json_status_return(rc, error_message, &json_str, TRUE);
							free_linked_lists(cache, disk, NULL);
							disk = NULL;
							cache = NULL;
						}
					}
				}
			}
		} else if (strncmp(url, CMD_RCACHE_CREATE, sizeof(CMD_RCACHE_CREATE) - 1) == SUCCESS) {
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, D_RECV_REQ), DAEMON, CMD_RCACHE_CREATE);
			int cache_mode = INVALID_VALUE;
			char block_dev[NAMELEN + 5] = {0};
			dup = strdup(url);
			preg_replace(CMD_RCACHE_CREATE, "", dup, dup, strlen(dup));
			int last = split(dup, split_arr, "/");
			if (last != 2) {
				status = MHD_HTTP_BAD_REQUEST;
				syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_MALFORMED);
				if (args->verbose)
					fprintf(stderr, verbose_msg(msg, ERR_MALFORMED), DAEMON);
				json_status_return(INVALID_VALUE, ERR_MALFORMED, &json_str, TRUE);
			} else {
				token = split_arr[last - 2];
				if (!token) {
					status = MHD_HTTP_BAD_REQUEST;
					syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_INVALIDURL);
					if (args->verbose)
						fprintf(stderr, verbose_msg(msg, ERR_INVALIDURL), DAEMON);
					json_status_return(INVALID_VALUE, NULL, &json_str, TRUE);
				} else {
					sprintf(device, "%s", token);
					token = split_arr[last - 1];    /* time to get the size     */
					if (!token) {
						status = MHD_HTTP_BAD_REQUEST;
						syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_INVALIDURL);
						if (args->verbose)
							fprintf(stderr, verbose_msg(msg, ERR_INVALIDURL), DAEMON);
						json_status_return(INVALID_VALUE, ERR_INVALIDURL, &json_str, TRUE);
					} else {
						sprintf(source, "%s", token);
						token = split_arr[last];    /* time to get the size     */
						if (!token) {
							status = MHD_HTTP_BAD_REQUEST;
							syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_INVALIDURL);
							if (args->verbose)
								fprintf(stderr, verbose_msg(msg, ERR_INVALIDURL), DAEMON);
							json_status_return(INVALID_VALUE, NULL, &json_str, TRUE);
						} else {
							if (strcmp(token, "write-through") == SUCCESS) {
								cache_mode = WRITETHROUGH;
							} else if (strcmp(token, "write-around") == SUCCESS) {
								cache_mode = WRITEAROUND;
							} else if (strcmp(token, "write-back") == SUCCESS) {
								cache_mode = WRITEBACK;
							} else {
								status = MHD_HTTP_BAD_REQUEST;
								syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_INVALID_MODE);
								if (args->verbose)
									fprintf(stderr, verbose_msg(msg, ERR_INVALID_MODE), DAEMON);
								json_status_return(INVALID_VALUE, ERR_INVALID_MODE, &json_str, TRUE);
							}
							if (cache_mode >= SUCCESS) {
								disk = search_rdsk_targets(error_message);
								if ((disk == NULL) && (strlen(error_message) != 0)) {
									syslog(LOG_ERR | LOG_DAEMON, "%s", error_message);
									if (args->verbose)
										fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
									json_status_return(INVALID_VALUE, error_message, &json_str, TRUE);
								} else {
									cache = search_cache_targets(error_message);
									if (cache == NULL && strlen(error_message) != 0) {
										syslog(LOG_ERR | LOG_DAEMON, "%s", error_message);
										if (args->verbose)
											fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
										json_status_return(INVALID_VALUE, error_message, &json_str, TRUE);
										free_linked_lists(NULL, disk, NULL);
										disk = NULL;
									} else {
										sprintf(block_dev, "/dev/%s", source);
										rc = cache_device_map(disk, cache, device, block_dev, cache_mode,
															  error_message);
										int pri = LOG_INFO;
										if (rc < SUCCESS)
											pri = LOG_ERR;
										syslog(pri | LOG_DAEMON, "%s", error_message);
										if (args->verbose)
											fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
										json_status_return(rc, error_message, &json_str, TRUE);
										free_linked_lists(cache, disk, NULL);
										cache = NULL;
										disk = NULL;
									}
								}
							}
						}
					}
				}
			}
		} else if (strncmp(url, CMD_RCACHE_REMOVE, sizeof(CMD_RCACHE_REMOVE) - 1) == SUCCESS) {
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, D_RECV_REQ), DAEMON, CMD_RCACHE_REMOVE);
			dup = strdup(url);
			preg_replace(CMD_RCACHE_REMOVE, "", dup, dup, strlen(dup));
			int last = split(dup, split_arr, "/");
			if (last != 0) {
				status = MHD_HTTP_BAD_REQUEST;
				syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_MALFORMED);
				if (args->verbose)
					fprintf(stderr, verbose_msg(msg, ERR_MALFORMED), DAEMON);
				json_status_return(INVALID_VALUE, ERR_MALFORMED, &json_str, TRUE);
			} else {
				token = split_arr[last];
				if (!token) {
					status = MHD_HTTP_BAD_REQUEST;
					syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_INVALIDURL);
					if (args->verbose)
						fprintf(stderr, verbose_msg(msg, ERR_INVALIDURL), DAEMON);
					json_status_return(INVALID_VALUE, ERR_INVALIDURL, &json_str, TRUE);
				} else {
					cache = search_cache_targets(error_message);
					if (cache == NULL && strlen(error_message) != 0) {
						syslog(LOG_ERR|LOG_DAEMON, "%s", error_message);
						if (args->verbose)
							fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
						json_status_return(INVALID_VALUE, error_message, &json_str, TRUE);
					} else {
						rc = cache_device_unmap(cache, token, error_message);
						int pri = LOG_INFO;
						if (rc < SUCCESS)
							pri = LOG_ERR;
						syslog(pri|LOG_DAEMON, "%s", error_message);
						if (args->verbose)
							fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
						json_status_return(rc, error_message, &json_str, TRUE);
						free_linked_lists(cache, NULL, NULL);
						cache = NULL;
					}
				}
			}
		} else if (strncmp(url, CMD_RDSK_LOCK, sizeof(CMD_RDSK_LOCK) - 1) == SUCCESS) {
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, D_RECV_REQ), DAEMON, CMD_RDSK_LOCK);
			dup = strdup(url);
			preg_replace(CMD_RDSK_LOCK, "", dup, dup, strlen(dup));
			int last = split(dup, split_arr, "/");
			if (last != 0) {
				status = MHD_HTTP_BAD_REQUEST;
				syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_MALFORMED);
				if (args->verbose)
					fprintf(stderr, verbose_msg(msg, ERR_MALFORMED), DAEMON);
				json_status_return(INVALID_VALUE, ERR_MALFORMED, &json_str, TRUE);
			} else {
				token = split_arr[last];
				if (!token) {
					status = MHD_HTTP_BAD_REQUEST;
					syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_INVALIDURL);
					if (args->verbose)
						fprintf(stderr, verbose_msg(msg, ERR_INVALIDURL), DAEMON);
					json_status_return(INVALID_VALUE, ERR_INVALIDURL, &json_str, TRUE);
				} else {
					disk = search_rdsk_targets(error_message);
					if ((disk == NULL) && (strlen(error_message) != 0)) {
						syslog(LOG_ERR|LOG_DAEMON, "%s", error_message);
						if (args->verbose)
							fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
						json_status_return(INVALID_VALUE, error_message, &json_str, TRUE);
					} else {
						rc = mem_device_lock(disk, token, TRUE, error_message);
						int pri = LOG_INFO;
						if (rc < SUCCESS)
							pri = LOG_ERR;
						syslog(pri|LOG_DAEMON, "%s", error_message);
						if (args->verbose)
							fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
						json_status_return(rc, error_message, &json_str, TRUE);
						free_linked_lists(NULL, disk, NULL);
						disk = NULL;
					}
				}
			}
		} else if (strncmp(url, CMD_RDSK_UNLOCK, sizeof(CMD_RDSK_UNLOCK) - 1) == SUCCESS) {
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, D_RECV_REQ), DAEMON, CMD_RDSK_UNLOCK);
			dup = strdup(url);
			preg_replace(CMD_RDSK_UNLOCK, "", dup, dup, strlen(dup));
			int last = split(dup, split_arr, "/");
			if (last != 0) {
				status = MHD_HTTP_BAD_REQUEST;
				syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_MALFORMED);
				if (args->verbose)
					fprintf(stderr, verbose_msg(msg, ERR_MALFORMED), DAEMON);
				json_status_return(INVALID_VALUE, ERR_MALFORMED, &json_str, TRUE);
			} else {
				token = split_arr[last];
				if (!token) {
					status = MHD_HTTP_BAD_REQUEST;
					syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_INVALIDURL);
					if (args->verbose)
						fprintf(stderr, verbose_msg(msg, ERR_INVALIDURL), DAEMON);
					json_status_return(INVALID_VALUE, NULL, &json_str, TRUE);
				} else {
					disk = search_rdsk_targets(error_message);
					if ((disk == NULL) && (strlen(error_message) != 0)) {
						syslog(LOG_ERR|LOG_DAEMON, "%s", error_message);
						if (args->verbose)
							fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
						json_status_return(INVALID_VALUE, error_message, &json_str, TRUE);
					} else {
						rc = mem_device_lock(disk, token, FALSE, error_message);
						int pri = LOG_INFO;
						if (rc < SUCCESS)
							pri = LOG_ERR;
						syslog(pri|LOG_DAEMON, "%s", error_message);
						if (args->verbose)
							fprintf(stderr, verbose_msg(msg, error_message), DAEMON);
						json_status_return(rc, error_message, &json_str, TRUE);
						free_linked_lists(NULL, disk, NULL);
						disk = NULL;
					}
				}
			}
		} else {
			syslog(LOG_ERR|LOG_DAEMON, "%s", ERR_UNSUPPORTED);
			if (args->verbose)
				fprintf(stderr, verbose_msg(msg, ERR_UNSUPPORTED), DAEMON);
			json_status_return(INVALID_VALUE, ERR_UNSUPPORTED, &json_str, TRUE);
			status = MHD_HTTP_BAD_REQUEST;
		}
	} else
		status = MHD_HTTP_BAD_REQUEST;

	if (json_str == NULL) {
		json_status_return(INVALID_VALUE, "", &json_str, TRUE);
	}
	response = MHD_create_response_from_buffer(strlen(json_str), (void *) json_str, MHD_RESPMEM_MUST_COPY);
	rc = MHD_queue_response (connection, status, response);
	MHD_destroy_response (response);
	if (dup) free(dup);
	if (json_str) free(json_str);
	if (error_message) free(error_message);
	dup = NULL;
	json_str = NULL;
	error_message = NULL;
	return rc;
}

/**
 * This function nullify the effects of a SIGPIPE signal (needed by libmicrohttpd:
 * https://www.gnu.org/software/libmicrohttpd/manual/html_node/microhttpd_002dintro.html#SIGPIPE )
 */
static void catcher (int sig) {
}

/**
 * This function is called upon receiving a signal, it sets server_stop_requested so the main loop will exit
 */
static void signal_handler(int sig) {
	char msg[NAMELEN] = {0};
	server_stop_requested = 1;
	fprintf(stderr, verbose_msg(msg, D_SIGNAL_RECEIVED), DAEMON, strsignal(sig));
	syslog(LOG_INFO|LOG_DAEMON, D_SIGNAL_RECEIVED, strsignal(sig));
}

/**
 * Installs the SIGPIPE signal catcher
 */
static void ignore_sigpipe (PTHREAD_ARGS *args) {
	struct sigaction oldsig;
	struct sigaction sig;
	char msg[NAMELEN] = {0};

	sig.sa_handler = &catcher;
	sigemptyset (&sig.sa_mask);
	sig.sa_flags = SA_RESTART;
	if (0 != sigaction (SIGPIPE, &sig, &oldsig)) {
		syslog(LOG_ERR|LOG_DAEMON, ERR_SIGPIPE_HANDLER, strerror(errno), __func__);
		if (args->verbose)
			fprintf (stderr, verbose_msg(msg, ERR_SIGPIPE_HANDLER), DAEMON, strerror(errno), __func__);
	}
}

/**
 * Catch all the appropriate signals to be able to exit in a clean way
 */
static void catch_exit_signals() {
	struct sigaction sig;

	sig.sa_handler = &signal_handler;
	sigemptyset (&sig.sa_mask);
	sig.sa_flags = SA_RESTART;
	sigaction(SIGHUP, &sig, NULL);
	sigaction(SIGUSR1, &sig, NULL);
	sigaction(SIGUSR2, &sig, NULL);
	sigaction(SIGALRM, &sig, NULL);
	sigaction(SIGINT, &sig, NULL);
	sigaction(SIGTERM, &sig, NULL);

}

/**
 * It creates a daemon that listens on the specified port and calls the function `answer_to_connection` when a connection
 * is received
 *
 * @param arg a pointer to the arguments passed to the thread function.
 *
 * @return The return value of the function is the exit status of the program.
 */
int mgmt_thread(void *arg)
{
	struct PTHREAD_ARGS *args = (struct PTHREAD_ARGS *)arg;
	struct MHD_Daemon *mydaemon = NULL;
	char msg[NAMELEN] = {0};

	ignore_sigpipe(args);
	catch_exit_signals();

	/*
	 * Now 'args' is passed to the callback function 'answer_to_connection'
	 */
	mydaemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_AUTO, atoi(args->port), NULL, NULL,
							  &answer_to_connection, args, MHD_OPTION_END);
	/*
	 * The `daemon` var was replaced by `mydaemon` cause it clashed with linux/kernel headers var with the same name
	 */
	if (mydaemon == NULL) {
		syslog(LOG_INFO|LOG_DAEMON, ERR_NEW_MHD_DAEMON, strerror(errno), __func__);
		if (args->verbose)
			fprintf(stderr, verbose_msg(msg, ERR_NEW_MHD_DAEMON), DAEMON, strerror(errno), __func__);
		return INVALID_VALUE;
	}
	while (!server_stop_requested) {
		/* On any signal, sleep is always interrupted and this allows to the signal handler function to be invoked */
		sleep(60 * 8);
	}

	MHD_stop_daemon(mydaemon);

	syslog(LOG_INFO|LOG_DAEMON, D_LOOP_EXITING, __func__);
	if (args->verbose)
		fprintf(stderr, verbose_msg(msg, D_LOOP_EXITING), DAEMON, __func__);

	return SUCCESS;
}
