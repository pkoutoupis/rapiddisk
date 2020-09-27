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
#include "cli.h"
#include <jansson.h>

/*
 * JSON output format:
 * {
 *    "status": "OK",
 *    "version": "7.0.0"
 * }
 */

int json_check_status(unsigned char *message)
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
 *    "status": "Success",
 * }
 */

int json_return_status(unsigned char *message, int status)
{
	json_t *root = json_object();

	json_object_set_new(root, "return_status", json_string((status == SUCCESS) ? "Success" : "Failed"));

	sprintf(message, "%s\n", json_dumps(root, 0));

	json_decref(root);

	return SUCCESS;
}

/*
 * JSON output format:
 * {
 *    "volumes": [
 *        {
 *            "rapidDisk": "rd0",
 *            "size": 67108864
 *        },
 *        {
 *            "rapidDisk": "rd1",
 *            "size": 134217728
 *        },
 *        {
 *            "rapidDisk-Cache": "rc-wa_sdb",
 *            "cache": "rd0",
 *            "source": "sdb",
 *            "mode" : "write-around"
 *        }
 *    ]
 * }
 * TODO: add iostat data and memory memory total/free and cpu util data
 */

int json_device_list(unsigned char *message, struct RD_PROFILE *rd_prof, struct RC_PROFILE *rc_prof)
{
	json_t *root, *array  = json_array();

	while (rd_prof != NULL) {
		json_t *object = json_object();
		json_object_set_new(object, "rapidDisk", json_string(rd_prof->device));
		json_object_set_new(object, "size", json_integer(rd_prof->size));
		json_array_append_new(array, object);
		rd_prof = rd_prof->next;
	}
	while (rc_prof != NULL) {
		json_t *object = json_object();
		json_object_set_new(object, "rapidDisk-Cache", json_string(rc_prof->device));
		json_object_set_new(object, "cache", json_string(rc_prof->cache));
		json_object_set_new(object, "source", json_string(rc_prof->source));
		json_object_set_new(object, "mode",
				    json_string((strncmp(rc_prof->device,
				    "rc-wt_", 5) == 0) ? "write-through" : "write-around"));
		json_array_append_new(array, object);
		rc_prof = rc_prof->next;
	}

	root = json_pack("{s:o}", "volumes", array);

	sprintf(message, "%s\n", json_dumps(root, 0));

	json_decref(array);
	json_decref(root);

	return SUCCESS;
}
