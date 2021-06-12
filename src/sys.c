/*********************************************************************************
 ** Copyright © 2011 - 2021 Petros Koutoupis
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
 ** @filename: sys.c
 ** @description: This file contains the function to build and handle JSON objects.
 **
 ** @date: 1Aug20, petros@petroskoutoupis.com
 ********************************************************************************/

#include "common.h"
#include "cli.h"
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#include <sys/time.h>

struct VOLUME_PROFILE *volume_head =  (struct VOLUME_PROFILE *) NULL;
struct VOLUME_PROFILE *volume_end =   (struct VOLUME_PROFILE *) NULL;

int get_memory_usage(struct MEM_PROFILE *mem)
{       
	struct sysinfo si; 
	if (sysinfo(&si) < 0) { 
		printf("%s (%d): Unable to retrieve memory usage.\n", __func__, __LINE__); 

		return INVALID_VALUE;
	}
	mem->mem_total = si.totalram;
	mem->mem_free = si.freeram;
	return SUCCESS;
}

struct VOLUME_PROFILE *search_volumes_targets(void)
{
	int rc, n = 0, i;
        unsigned char file[NAMELEN] = {0}, test[NAMELEN + 32] = {0};
        struct dirent **list;
        struct VOLUME_PROFILE *volume = NULL;

        if ((rc = scandir(SYS_BLOCK, &list, NULL, NULL)) < 0) {
                printf("%s: scandir: %s\n", __func__, strerror(errno));
                return NULL;
        }
        for (;n < rc; n++) {
                if ((strncmp(list[n]->d_name, "sd", 2) == SUCCESS) || \
		    (strncmp(list[n]->d_name, "nvme", 4) == SUCCESS) || \
		    (strncmp(list[n]->d_name, "pmem", 4) == SUCCESS)) {
                        volume = (struct VOLUME_PROFILE *)calloc(1, sizeof(struct VOLUME_PROFILE));
                        if (volume == NULL) {
                                printf("%s: calloc: %s\n", __func__, strerror(errno));
                                return NULL;
                        }
                        strcpy(volume->device, (unsigned char *)list[n]->d_name);
                        sprintf(file, "%s/%s", SYS_BLOCK, list[n]->d_name);
                        volume->size = (BYTES_PER_SECTOR * strtoull(read_info(file, "size"), NULL, 10));
                        sprintf(file, "%s/%s/device", SYS_BLOCK, list[n]->d_name);
			sprintf(test, "%s/model", file);
			if (access(test, F_OK) != INVALID_VALUE)
				sprintf(volume->model, "%s", read_info(file, "model"));
			else
				sprintf(volume->model, "UNAVAILABLE");
			/* trim whitespace of model string */
			for (i = 0; i < strlen(volume->model); i++) {
				if ((strncmp(volume->model + i, " ", 1) == SUCCESS) || (strncmp(volume->model + i, "\n", 1) == SUCCESS))
					volume->model[i] = '\0';
			}
			sprintf(test, "%s/vendor", file);
			if (access(test, F_OK) != INVALID_VALUE)
				sprintf(volume->vendor, "%s", read_info(file, "vendor"));
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
                if (list[n] != NULL) free(list[n]);
        }
        return volume_head;
}

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
