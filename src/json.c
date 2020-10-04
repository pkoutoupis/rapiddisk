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

	json_object_set_new(root, "return_status", json_string((status == SUCCESS) ? "Success" : "Failed"));

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
 *           "size": 67108864
 *         },
 *         {
 *           "device": "rd0",
 *           "size": 67108864
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

int json_device_list(unsigned char *message, struct RD_PROFILE *rd, struct RC_PROFILE *rc)
{
	json_t *root, *array = json_array();
	json_t *rd_array = json_array(), *rc_array = json_array();
        json_t *rd_object = json_object(), *rc_object = json_object() ;

	while (rd != NULL) {
		json_t *object = json_object();
		json_object_set_new(object, "device", json_string(rd->device));
		json_object_set_new(object, "size", json_integer(rd->size));
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
		json_object_set_new(object, "mode", json_string((strncmp(rc->device,
				    "rc-wt_", 5) == 0) ? "write-through" : "write-around"));
		json_array_append_new(rc_array, object);
		rc = rc->next;
	}
	json_object_set_new(rc_object, "rapiddisk_cache", rc_array);
	json_array_append_new(array, rc_object);

	root = json_pack("{s:o}", "volumes", array);

	sprintf(message, "%s\n", json_dumps(root, 0));

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

int json_resources_list(unsigned char *message, struct MEM_PROFILE *mem, struct VOLUME_PROFILE *volume)
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

	sprintf(message, "%s\n", json_dumps(root, 0));

	json_decref(mem_array);
	json_decref(vol_array);
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

#endif
