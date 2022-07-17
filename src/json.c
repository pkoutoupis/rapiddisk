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
 ** @filename: json.c
 ** @description: This file contains the function to build and handle JSON objects.
 **
 ** @date: 15Oct10, petros@petroskoutoupis.com
 ********************************************************************************/

#include "common.h"
#include <jansson.h>

#if !defined SERVER

#include "cli.h"

/*
 * JSON output format:
 * {
 *    "status": "Success"
 * }
 */

int json_status_return(int status)
{
	json_t *root = json_object();

	json_object_set_new(root, "status", json_string((status == SUCCESS) ? "Success" : "Failed"));

	printf("%s\n", json_dumps(root, 0));

	json_decref(root);

	return SUCCESS;
}

/*
 * JSON output format:
 * {
 *   "volumes": [
 *     {
 *       "rapiddisk": [
 *         {
 *           "device": "rd1",
 *           "size": 67108864,
 *           "usage": 4096,
 *           "status": "locked"
 *         },
 *         {
 *           "device": "rd0",
 *           "size": 67108864,
 *           "usage": 65536,
 *           "status": "unlocked"
 *         }
 *       ]
 *     },
 *     {
 *       "rapiddisk_cache": [
 *         {
 *           "device": "rc-wa_loop7",
 *           "cache": "rd0",
 *           "source": "loop7",
 *           "mode": "write-around"
 *         }
 *       ]
 *     }
 *   ]
 * }
 */

int json_device_list(struct RD_PROFILE *rd, struct RC_PROFILE *rc)
{
	json_t *root, *array = json_array();
	json_t *rd_array = json_array(), *rc_array = json_array();
        json_t *rd_object = json_object(), *rc_object = json_object() ;
	unsigned char mode[0x20] = {0x0};

	while (rd != NULL) {
		json_t *object = json_object();
		json_object_set_new(object, "device", json_string(rd->device));
		json_object_set_new(object, "size", json_integer(rd->size));
		json_object_set_new(object, "usage", json_integer(rd->usage));
		memset(mode, 0x0, sizeof(mode));
		if (rd->lock_status == TRUE) {
			sprintf(mode, "locked");
		} else if (rd->lock_status == FALSE) {
			sprintf(mode, "unlocked");
		} else {
			sprintf(mode, "unavailable");
		}
		json_object_set_new(object, "status", json_string(mode));
		json_array_append_new(rd_array, object);
		rd = rd->next;
	}
	json_object_set_new(rd_object, "rapiddisk", rd_array);
	json_array_append_new(array, rd_object);

	while (rc != NULL) {
		json_t *object = json_object();
		json_object_set_new(object, "device", json_string(rc->device));
		json_object_set_new(object, "cache", json_string(rc->cache));
		json_object_set_new(object, "source", json_string(rc->source));
		memset(mode, 0x0, sizeof(mode));
                if (strncmp(rc->device, "rc-wt_", 5) == 0) {
                        sprintf(mode, "write-through");
                } else if (strncmp(rc->device, "rc-wb_", 5) == 0) {
                        sprintf(mode, "writeback");
                } else {
                        sprintf(mode, "write-around");
                }
                json_object_set_new(object, "mode", json_string(mode));
		json_array_append_new(rc_array, object);
		rc = rc->next;
	}
	json_object_set_new(rc_object, "rapiddisk_cache", rc_array);
	json_array_append_new(array, rc_object);

	root = json_pack("{s:o}", "volumes", array);

	printf("%s\n", json_dumps(root, 0));

	json_decref(rd_array);
	json_decref(rc_array);
	json_decref(array);
	json_decref(root);

	return SUCCESS;
}

/*
 * JSON output format:
 * {
 *   "resources": [
 *     {
 *       "memory": [
 *         {
 *           "mem_total": 2084130816,
 *           "mem_free": 236990464
 *         }
 *       ]
 *     },
 *     {
 *       "volumes": [
 *         {
 *           "device": "sda",
 *           "size": 26843545600,
 *           "vendor": "ATA",
 *           "model": "VBOX HARDDISK"
 *         }
 *       ]
 *     }
 *   ]
 * }
 */

int json_resources_list(struct MEM_PROFILE *mem, struct VOLUME_PROFILE *volume)
{
	json_t *root, *array = json_array(), *mem_array = json_array(), *vol_array = json_array();
	json_t *mem_object = json_object(), *vol_object = json_object() ;

	if (mem != NULL) {
		json_t *object = json_object();
		json_object_set_new(object, "mem_total", json_integer(mem->mem_total));
		json_object_set_new(object, "mem_free", json_integer(mem->mem_free));
		json_array_append_new(mem_array, object);
	}
	json_object_set_new(mem_object, "memory", mem_array);
	json_array_append_new(array, mem_object);

        while (volume != NULL) {
                json_t *object = json_object();
                json_object_set_new(object, "device", json_string(volume->device));
                json_object_set_new(object, "size", json_integer(volume->size));
                json_object_set_new(object, "vendor", json_string(volume->vendor));
                json_object_set_new(object, "model", json_string(volume->model));
                json_array_append_new(vol_array, object);
                volume = volume->next;
        }
	json_object_set_new(vol_object, "volumes", vol_array);
	json_array_append_new(array, vol_object);

	root = json_pack("{s:o}", "resources", array);

	printf("%s\n", json_dumps(root, 0));

	json_decref(mem_array);
	json_decref(vol_array);
	json_decref(array);
	json_decref(root);

	return SUCCESS;
}

/*
 * JSON output format:
 *  {
 *    "statistics": [
 *      {
 *        "cache_stats": [
 *          {
 *            "device": "rc-wt_loop7",
 *            "reads": 527,
 *            "writes": 1,
 *            "cache_hits": 264,
 *            "replacement": 0,
 *            "write_replacement": 0,
 *            "read_invalidates": 1,
 *            "write_invalidates": 1,
 *            "uncached_reads": 1,
 *            "uncached_writes": 0,
 *            "disk_reads": 263,
 *            "disk_writes": 1,
 *            "cache_reads": 264,
 *            "cache_writes": 263
 *          }
 *        ]
 *      }
 *    ]
 *  }
 */

int json_cache_statistics(struct RC_STATS *stats)
{
	json_t *root, *array = json_array(), *stats_array = json_array(), *stats_object = json_object();

	if (stats != NULL) {
		json_t *object = json_object();
		json_object_set_new(object, "device", json_string(stats->device));
		json_object_set_new(object, "reads", json_integer(stats->reads));
		json_object_set_new(object, "writes", json_integer(stats->writes));
		json_object_set_new(object, "cache_hits", json_integer(stats->cache_hits));
		json_object_set_new(object, "replacement", json_integer(stats->replacement));
		json_object_set_new(object, "write_replacement", json_integer(stats->write_replacement));
		json_object_set_new(object, "read_invalidates", json_integer(stats->read_invalidates));
		json_object_set_new(object, "write_invalidates", json_integer(stats->write_invalidates));
		json_object_set_new(object, "uncached_reads", json_integer(stats->uncached_reads));
		json_object_set_new(object, "uncached_writes", json_integer(stats->uncached_writes));
		json_object_set_new(object, "disk_reads", json_integer(stats->disk_reads));
		json_object_set_new(object, "disk_writes", json_integer(stats->disk_writes));
		json_object_set_new(object, "cache_reads", json_integer(stats->cache_reads));
		json_object_set_new(object, "cache_writes", json_integer(stats->cache_writes));
		json_array_append_new(stats_array, object);
	}
	json_object_set_new(stats_object, "cache_stats", stats_array);
	json_array_append_new(array, stats_object);

	root = json_pack("{s:o}", "statistics", array);

	printf("%s\n", json_dumps(root, 0));

        json_decref(stats_array);
	json_decref(root);

	return SUCCESS;
}

/*
 * JSON output format:
 *  {
 *    "statistics": [
 *      {
 *        "cache_stats": [
 *          {
 *            "device": "rc-wb_sdd",
 *            "errors": 0,
 *            "num_blocks": 16320,
 *            "num_free_blocks": 16320,
 *            "num_wb_blocks": 0
 *          }
 *        ]
 *      }
 *    ]
 *  }
 */

int json_cache_wb_statistics(struct WC_STATS *stats)
{
        json_t *root, *array = json_array(), *stats_array = json_array(), *stats_object = json_object();

	if (stats != NULL) {
		json_t *object = json_object();
		json_object_set_new(object, "device", json_string(stats->device));
		json_object_set_new(object, "errors", json_integer(stats->errors));
		json_object_set_new(object, "num_blocks", json_integer(stats->num_blocks));
		json_object_set_new(object, "num_free_blocks", json_integer(stats->num_free_blocks));
		json_object_set_new(object, "num_wb_blocks", json_integer(stats->num_wb_blocks));
		if (stats->expanded == TRUE) {
			json_object_set_new(object, "num_read_req", json_integer(stats->num_read_req));
			json_object_set_new(object, "num_read_cache_hits", json_integer(stats->num_read_cache_hits));
			json_object_set_new(object, "num_write_req", json_integer(stats->num_write_req));
			json_object_set_new(object, "num_write_uncommitted_blk_hits", json_integer(stats->num_write_uncommitted_blk_hits));
			json_object_set_new(object, "num_write_committed_blk_hits", json_integer(stats->num_write_committed_blk_hits));
			json_object_set_new(object, "num_write_cache_bypass", json_integer(stats->num_write_cache_bypass));
			json_object_set_new(object, "num_write_cache_alloc", json_integer(stats->num_write_cache_alloc));
			json_object_set_new(object, "num_write_freelist_blocked", json_integer(stats->num_write_freelist_blocked));
			json_object_set_new(object, "num_flush_req", json_integer(stats->num_flush_req));
			json_object_set_new(object, "num_discard_req", json_integer(stats->num_discard_req));
		}
		json_array_append_new(stats_array, object);
	}
	json_object_set_new(stats_object, "cache_stats", stats_array);
	json_array_append_new(array, stats_object);

	root = json_pack("{s:o}", "statistics", array);

	printf("%s\n", json_dumps(root, 0));

	json_decref(stats_array);
	json_decref(root);

	return SUCCESS;
}

/*
 * JSON output format:
 * {
 *   "targets": [
 *     {
 *       "nvmet_targets": [
 *         {
 *           "nqn": "rd1-test",
 *           "namespace": 2,
 *           "device": "/dev/rd2",
 *           "enabled": "true"
 *         },
 *         {
 *           "nqn": "rd1-test",
 *           "namespace": 1,
 *           "device": "/dev/rd1",
 *           "enabled": "true"
 *         }
 *       ]
 *     },
 *     {
 *       "nvmet_ports": [
 *         {
 *           "port": 1,
 *           "address": "10.0.0.185",
 *           "protocol": "tcp",
 *           "nqn": "rd1-test"
 *         }
 *       ]
 *     }
 *   ]
 * }
 */

int json_nvmet_view_exports(struct NVMET_PROFILE *nvmet, struct NVMET_PORTS *ports)
{
	json_t *root, *array = json_array(), *nvmet_array = json_array(), *ports_array = json_array();
	json_t *nvmet_object = json_object(), *ports_object = json_object();

	while (nvmet != NULL) {
		json_t *object = json_object();
		json_object_set_new(object, "nqn", json_string(nvmet->nqn));
		json_object_set_new(object, "namespace", json_integer(nvmet->namespc));
		json_object_set_new(object, "device", json_string(nvmet->device));
		json_object_set_new(object, "enabled", json_string((nvmet->enabled == ENABLED) ? "true" : "false"));
		json_array_append_new(nvmet_array, object);
		nvmet = nvmet->next;
	}
	json_object_set_new(nvmet_object, "nvmet_targets", nvmet_array);
	json_array_append_new(array, nvmet_object);

	while (ports != NULL) {
		json_t *object = json_object();
		json_object_set_new(object, "port", json_integer(ports->port));
		json_object_set_new(object, "address", json_string(ports->addr));
		json_object_set_new(object, "protocol", json_string(ports->protocol));
		json_object_set_new(object, "nqn", json_string(ports->nqn));
		json_array_append_new(ports_array, object);
		ports = ports->next;
	}
	json_object_set_new(ports_object, "nvmet_ports", ports_array);
	json_array_append_new(array, ports_object);

	root = json_pack("{s:o}", "targets", array);

	printf("%s\n", json_dumps(root, 0));

	json_decref(nvmet_array);
	json_decref(ports_array);
	json_decref(array);
	json_decref(root);

	return SUCCESS;
}

/*
 * JSON output format:
 * {
 *   "targets": [
 *     {
 *       "nvmet_ports": [
 *         {
 *           "port": 1,
 *           "address": "10.0.0.185",
 *           "protocol": "tcp"
 *         }
 *       ]
 *     }
 *   ]
 * }
 */
int json_nvmet_view_ports(struct NVMET_PORTS *ports)
{
	json_t *root, *array = json_array(), *ports_array = json_array();
	json_t *ports_object = json_object();

	while (ports != NULL) {
		json_t *object = json_object();
		json_object_set_new(object, "port", json_integer(ports->port));
		json_object_set_new(object, "address", json_string(ports->addr));
		json_object_set_new(object, "protocol", json_string(ports->protocol));
		json_array_append_new(ports_array, object);
		ports = ports->next;
	}
	json_object_set_new(ports_object, "nvmet_ports", ports_array);
	json_array_append_new(array, ports_object);

	root = json_pack("{s:o}", "targets", array);

	printf("%s\n", json_dumps(root, 0));

	json_decref(ports_array);
	json_decref(array);
	json_decref(root);

	return SUCCESS;
}

#else

/*
 * JSON output format:
 * {
 *    "status": "OK",
 *    "version": "7.0.0"
 * }
 */

int json_status_check(unsigned char *message)
{
	json_t *root = json_object();

	json_object_set_new(root, "status", json_string("OK"));
	json_object_set_new(root, "version", json_string(VERSION_NUM));

	sprintf(message, "%s\n", json_dumps(root, 0));

	json_decref(root);

	return SUCCESS;
}

/*
 * JSON output format:
 * {
 *    "status": "Unsupported"
 * }
 */

int json_status_unsupported(unsigned char *message)
{
	json_t *root = json_object();

	json_object_set_new(root, "status", json_string("Unsupported"));

	sprintf(message, "%s\n", json_dumps(root, 0));

	json_decref(root);

	return SUCCESS;
}

#endif
