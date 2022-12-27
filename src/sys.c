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

#include "sys.h"
#include "utils.h"
#include "rdsk.h"
#include <sys/sysinfo.h>

struct VOLUME_PROFILE *volume_head = NULL;
struct VOLUME_PROFILE *volume_end = NULL;

#if !defined SERVER
/**
 * It prints out the memory and block device information
 *
 * @param mem A pointer to a struct MEM_PROFILE.
 * @param volumes A pointer to the first element of a linked list of VOLUME_PROFILE structures.
 *
 * @return The function status result
 */
int resources_list(struct MEM_PROFILE *mem, struct VOLUME_PROFILE *volumes)
{
	int num = 1;

	printf("List of memory usage:\n\n");
	if (mem != NULL) {
		printf(" Memory total: %llu\n Memory free: %llu\n", mem->mem_total, mem->mem_free);
	}

	printf("\nList of block device(s):\n\n");

	while (volumes != NULL) {
		printf(" Block Device %d:\n\tDevice: %s\n\tSize (MB): %llu\n"
		       "\tVendor: %s\n\tModel: %s\n", num, volumes->device,
		       ((volumes->size / 1024) / 1024), volumes->vendor, volumes->model);
		num++;
		volumes = volumes->next;
	}
	printf("\n");
	return SUCCESS;
}

#endif

/**
 * It scans the /sys/block directory for block devices and populates a linked list of struct VOLUME_PROFILE with the device
 * name, size, model, and vendor
 *
 * @param return_message This is a pointer to a string that will be populated with an error message if the function fails.
 *
 * @return A linked list of struct VOLUME_PROFILE.
 */
struct VOLUME_PROFILE *search_volumes_targets(char *return_message)
{
	int rc, n = 0, i;
	char file[NAMELEN] = {0}, test[NAMELEN + 32] = {0};
	struct dirent **list;
	struct VOLUME_PROFILE *volume = NULL;
	char *msg = NULL;

	volume_head = NULL;
	volume_end = NULL;

	if ((rc = scandir(SYS_BLOCK, &list, NULL, NULL)) < 0) {
		msg = "%s: scandir: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return NULL;
	}
	for (;n < rc; n++) {
		if ((strncmp(list[n]->d_name, "sd", 2) == SUCCESS) || \
		(strncmp(list[n]->d_name, "nvme", 4) == SUCCESS) || \
		(strncmp(list[n]->d_name, "pmem", 4) == SUCCESS)) {
			volume = (struct VOLUME_PROFILE *)calloc(1, sizeof(struct VOLUME_PROFILE));
			if (volume == NULL) {
				msg = ERR_CALLOC;
				print_error(msg, return_message, __func__, strerror(errno));
				list = clean_scandir(list, rc);
				free_linked_lists(NULL, NULL, volume_head);
				return NULL;
			}
			strcpy(volume->device, (char *)list[n]->d_name);
			sprintf(file, "%s/%s", SYS_BLOCK, list[n]->d_name);
			char *info_size = read_info(file, "size", return_message);
			if (info_size == NULL) {
				free(volume);
				volume = NULL;
				list = clean_scandir(list, rc);
				free_linked_lists(NULL, NULL, volume_head);
				return NULL;
			}
			volume->size = (BYTES_PER_SECTOR * strtoull(info_size, NULL, 10));
			sprintf(file, "%s/%s/device", SYS_BLOCK, list[n]->d_name);
			sprintf(test, "%s/model", file);
			if (access(test, F_OK) != INVALID_VALUE) {
				char *info_model = read_info(file, "model", return_message);
				if (info_model == NULL) {
					free(volume);
					volume = NULL;
					list = clean_scandir(list, rc);
					free_linked_lists(NULL, NULL, volume_head);
					return NULL;
				}
				sprintf(volume->model, "%s", info_model);
			} else
				sprintf(volume->model, "UNAVAILABLE");
			/* trim whitespace of model string */
			for (i = 0; i < strlen(volume->model); i++) {
				if ((strncmp(volume->model + i, " ", 1) == SUCCESS) || (strncmp(volume->model + i, "\n", 1) == SUCCESS))
					volume->model[i] = '\0';
			}
			sprintf(test, "%s/vendor", file);
			if (access(test, F_OK) != INVALID_VALUE) {
				char *info_vendor = read_info(file, "vendor", return_message);
				if (info_vendor == NULL) {
					free(volume);
					volume = NULL;
					list = clean_scandir(list, rc);
					free_linked_lists(NULL, NULL, volume_head);
					return NULL;
				}
				sprintf(volume->vendor, "%s", info_vendor);
			}
			else
				sprintf(volume->vendor, "UNAVAILABLE");
			/* trim whitespace of vendor string */
			for (i = 0; i < strlen(volume->vendor); i++) {
				if ((strncmp(volume->vendor+ i, " ", 1) == SUCCESS) || (strncmp(volume->vendor + i, "\n", 1) == SUCCESS))
					volume->vendor[i] = '\0';
			}

			if (volume_head == NULL)
				volume_head = volume;
			else
				volume_end->next = volume;
			volume_end = volume;
			volume->next = NULL;
		}
	}
	list = clean_scandir(list, rc);
	return volume_head;
}

/**
 * It retrieves the total and free memory of the system and stores it in the struct MEM_PROFILE
 *
 * @param mem A pointer to a struct MEM_PROFILE.
 * @param return_message A pointer to a string that will be populated with an error message if the function fails.
 *
 * @return The function status result.
 */
int get_memory_usage(struct MEM_PROFILE *mem, char *return_message)
{
	struct sysinfo si;
	if (sysinfo(&si) < 0) {
		char *msg = "%s (%d): Unable to retrieve memory usage.";
		print_error(msg, return_message, __func__, __LINE__);
		return INVALID_VALUE;
	}
	mem->mem_total = si.totalram;
	mem->mem_free = si.freeram;
	return SUCCESS;
}