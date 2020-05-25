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
 ** @filename: core.c
 ** @description: This file contains the core routines of rapiddisk.
 **
 ** @date: 15Oct10, petros@petroskoutoupis.com
 ********************************************************************************/

#include "common.h"
#include <dirent.h>
#include <malloc.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#if !defined NO_JANSSON
#include <jansson.h>
#endif

#define FILEDATA		0x40
#define BYTES_PER_SECTOR	0x200

struct RD_PROFILE *head =  (struct RD_PROFILE *) NULL;
struct RD_PROFILE *end =   (struct RD_PROFILE *) NULL;
struct RC_PROFILE *chead = (struct RC_PROFILE *) NULL;
struct RC_PROFILE *cend =  (struct RC_PROFILE *) NULL;

unsigned char *sys_block  = "/sys/block";
unsigned char *etc_mtab   = "/etc/mtab";
unsigned char *dev_mapper = "/dev/mapper";

unsigned char *read_info(unsigned char *, unsigned char *);
struct RD_PROFILE *search_targets(void);
struct RC_PROFILE *search_cache(void);

unsigned char *read_info(unsigned char *name, unsigned char *string)
{
	int len;
	unsigned char file[NAMELEN] = {0};
	unsigned char buf[0xFF] = {0};
	static unsigned char obuf[0xFF] = {0};
	FILE *fp;

	memset(&buf, 0, sizeof(buf));
	memset(&obuf, 0, sizeof(obuf));

	sprintf(file, "%s/%s", name, string);
	fp = fopen(file, "r");
	if (fp == NULL) {
		printf("%s: fopen: %s\n", __func__, strerror(errno));
		return NULL;
	}
	fread(buf, FILEDATA, 1, fp);
	len = strlen(buf);
	strncpy(obuf, buf, (len - 1));
	sprintf(obuf, "%s", obuf);
	fclose(fp);

	return obuf;
}

struct RD_PROFILE *search_targets(void)
{
	int rc, n = 0;
	unsigned char file[NAMELEN] = {0};
	struct dirent **list;
	struct RD_PROFILE *prof;

	if ((rc = scandir(sys_block, &list, NULL, NULL)) < 0) {
		printf("%s: scandir: %s\n", __func__, strerror(errno));
		return NULL;
	}
	for (;n < rc; n++) {
		if (strncmp(list[n]->d_name, "rd", 2) == SUCCESS) {
			prof = (struct RD_PROFILE *)calloc(1, sizeof(struct RD_PROFILE));
			if (prof == NULL) {
				printf("%s: calloc: %s\n", __func__, strerror(errno));
				return NULL;
			}
			strcpy(prof->device, (unsigned char *)list[n]->d_name);
			sprintf(file, "%s/%s", sys_block, list[n]->d_name);
			prof->size = (BYTES_PER_SECTOR * strtoull(read_info(file, "size"), NULL, 10));

			if (head == NULL)
				head = prof;
			else
				end->next = prof;
			end = prof;
			prof->next = NULL;
		}
		if (list[n] != NULL) free(list[n]);
	}
	return head;
}

struct RC_PROFILE *search_cache(void)
{
	int num, num2, num3, n = 0, i, z;
	struct dirent **list, **nodes, **maps;
	unsigned char file[NAMELEN] = {0};
	struct RC_PROFILE *prof;

	if ((num = scandir(dev_mapper, &list, NULL, NULL)) < 0) return NULL;
	if ((num2 = scandir(sys_block, &nodes, NULL, NULL)) < 0) {
		printf("%s: scandir: %s\n", __func__, strerror(errno));
		return NULL;
	}

	for (;n < num; n++) {
		if (strncmp(list[n]->d_name, "rc", 2) == SUCCESS) {
			prof = (struct RC_PROFILE *)calloc(1, sizeof(struct RC_PROFILE));
			if (prof == NULL) {
				printf("%s: calloc: %s\n", __func__, strerror(errno));
				return NULL;
			}
			strcpy(prof->device, (unsigned char *)list[n]->d_name);
			for (i = 0;i < num2; i++) {
				if (strncmp(nodes[i]->d_name, "dm-", 3) == SUCCESS) {
					sprintf(file, "%s/%s", sys_block, nodes[i]->d_name);
					if (strncmp(read_info(file, "dm/name"), prof->device,
					    sizeof(prof->device)) == 0) {
						sprintf(file, "%s/%s/slaves", sys_block, nodes[i]->d_name);
						if ((num3 = scandir(file, &maps, NULL, NULL)) < 0) {
							printf("%s: scandir: %s\n", __func__,
							       strerror(errno));
							return NULL;
						}
						for (z=0;z < num3; z++) {
							if (strncmp(maps[z]->d_name, ".", 1) != SUCCESS) {
								if (strncmp(maps[z]->d_name, "rd", 2) == SUCCESS)
									strcpy(prof->cache, (unsigned char *)maps[z]->d_name);
								else
									strcpy(prof->source, (unsigned char *)maps[z]->d_name);
							}
							if (maps[z] != NULL) free(maps[z]);
						}
					}
				}
			}
			if (chead == NULL)
				chead = prof;
			else
				cend->next = prof;
			cend = prof;
			prof->next = NULL;
		}
		if (list[n] != NULL) free(list[n]);
	}
	for (i = 0;i < num2; i++) if (nodes[i] != NULL) free(nodes[i]);
	return chead;
}

int list_devices(struct RD_PROFILE *rd_prof, struct RC_PROFILE *rc_prof)
{
	int num = 1;

	printf("List of RapidDisk device(s):\n\n");

	while (rd_prof != NULL) {
		printf(" RapidDisk Device %d: %s\tSize (KB): %llu\n", num,
		       rd_prof->device, (rd_prof->size / 1024));
		num++;
		rd_prof = rd_prof->next;
	}
	printf("\nList of RapidDisk-Cache mapping(s):\n\n");
	if (rc_prof == NULL) {
		printf("  None\n");
		goto list_out;
	}
	num = 1;
	while (rc_prof != NULL) {
	printf(" RapidDisk-Cache Target %d: %s\tCache: %s  Target: %s (%s)\n",
			num, rc_prof->device, rc_prof->cache, rc_prof->source,
			(strncmp(rc_prof->device, "rc-wt_", 5) == 0) ? \
			"WRITE THROUGH" : "WRITE AROUND");
		num++;
		rc_prof = rc_prof->next;
	}
list_out:
	printf("\n");
	return SUCCESS;
}

int stat_cache_mapping(struct RC_PROFILE *rc_prof, unsigned char *cache)
{
	unsigned char cmd[NAMELEN] = {0};

	if (rc_prof == NULL) {
		printf("  No RapidDisk-Cache Mappings exist.\n\n");
		return 1;
	}
	sprintf(cmd, "dmsetup status %s", cache);
	system(cmd);
	printf("\n");
	return SUCCESS;
}

int short_list_devices(struct RD_PROFILE *rd_prof, struct RC_PROFILE *rc_prof)
{
	if ((rd_prof == NULL) && (rc_prof == NULL)) {
		printf("There are no RapidDisk or RapidDisk-Cache devices.\n");
		goto short_list_out;
	}

	while (rd_prof != NULL) {
		printf("%s:%llu\n", rd_prof->device, rd_prof->size);
		rd_prof = rd_prof->next;
	}
	while (rc_prof != NULL) {
		printf("%s:%s,%s\n", rc_prof->device, rc_prof->cache, rc_prof->source);
		rc_prof = rc_prof->next;
	}
short_list_out:
	return SUCCESS;
}

#if !defined NO_JANSSON

/*
 * JSON output format:
 * ./rapiddisk --list-json|sed '1,3d'| jq .
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
 */

int list_devices_json(struct RD_PROFILE *rd_prof, struct RC_PROFILE *rc_prof)
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

	printf("%s\n", json_dumps(root, 0));
	//printf("%s\n", json_dumps(root, JSON_INDENT(2)));

	json_decref(array);
	json_decref(root);

	return SUCCESS;
}
#endif

int attach_device(struct RD_PROFILE *prof, unsigned long size)
{
	int dsk;
	FILE *fp;
	unsigned char string[BUFSZ], name[16];

	/* echo "rapiddisk attach 64" > /sys/kernel/rapiddisk/mgmt <- in sectors*/
	for (dsk = 0; prof != NULL; dsk++) {
		strcat(string, ",");
		strcat(string, prof->device);
		prof = prof->next;
	}

	while (dsk >= 0) {
		sprintf(name, "rd%d", dsk);
		if (strstr(string, (const char *)name) == NULL) {
			break;
		}
		dsk--;
	}
	if ((fp = fopen(SYS_RDSK, "w")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, SYS_RDSK, strerror(errno));
		return -ENOENT;
	}
	if (fprintf(fp, "rapiddisk attach %llu\n",
		((unsigned long long)size * 1024)) < 0) {
		printf("%s: fprintf: %s\n", __func__, strerror(errno));
		return -EIO;
	}
	printf("Attached device rd%d of size %lu Mbytes\n", dsk, size);
	fclose(fp);
	return SUCCESS;
}

int detach_device(struct RD_PROFILE *rd_prof, RC_PROFILE * rc_prof, unsigned char *string)
{
	int rc = INVALID_VALUE;
	FILE *fp;
	unsigned char *buf;

	/* echo "rapiddisk detach 1" > /sys/kernel/rapiddisk/mgmt */
	while (rd_prof != NULL) {
		if (strcmp(string, rd_prof->device) == SUCCESS)
			rc = SUCCESS;
		rd_prof = rd_prof->next;
	}
	if (rc != SUCCESS) {
		printf("Error. Device %s does not exist.\n", string);
		return INVALID_VALUE;
	}
	/* Check to make sure RapidDisk device isn't in a mapping */
	while (rc_prof != NULL) {
		if (strcmp(string, rc_prof->cache) == SUCCESS) {
			printf("Error. Unable to remove %s.\nThis RapidDisk device is currently"
				" mapped as a cache drive to %s.\n\n", string, rc_prof->device);
			return INVALID_VALUE;
		}
		rc_prof = rc_prof->next;
	}

	if ((buf = (char *)malloc(BUFSZ)) == NULL) {
		printf("%s: malloc: Unable to allocate memory.\n", __func__);
		return INVALID_VALUE;
	}

	/* Here we are starting to check to see if the device is mounted */
	if ((fp = fopen(etc_mtab, "r")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, etc_mtab, strerror(errno));
		return -ENOENT;
	}
	fread(buf, BUFSZ, 1, fp);
	fclose(fp);
	if ((strstr(buf, string) != NULL)) {
		printf("%s is currently mounted. Please \"umount\" and retry.\n", string);
		return INVALID_VALUE;
	}

	/* This is where we begin to detach the block device */
	if ((fp = fopen(SYS_RDSK, "w")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, SYS_RDSK, strerror(errno));
		return -ENOENT;
	}

	if (fprintf(fp, "rapiddisk detach %s\n", string + 2) < 0) {
		printf("%s: fprintf: %s\n", __func__, strerror(errno));
		return -EIO;
	}
	printf("Detached device %s\n", string);
	fclose(fp);

	return SUCCESS;
}

int resize_device(struct RD_PROFILE *prof, unsigned char *string, unsigned long size)
{
	int rc = INVALID_VALUE;
	FILE *fp;

	/* echo "rapiddisk resize 1 128" > /sys/kernel/rapiddisk/mgmt */
	while (prof != NULL) {
		if (strcmp(string, prof->device) == SUCCESS) {
			if (((unsigned long long)size * 1024) <= (prof->size / 1024)) {
				printf("Error. Please specify a size larger than %lu Mbytes\n", size);
				return -EINVAL;
			}
			rc = SUCCESS;
		}
		prof = prof->next;
	}
	if (rc != SUCCESS) {
		printf("Error. Device %s does not exist.\n", string);
		return -ENOENT;
	}

	/* This is where we begin to detach the block device */
	if ((fp = fopen(SYS_RDSK, "w")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, SYS_RDSK, strerror(errno));
		return -ENOENT;
	}

	if (fprintf(fp, "rapiddisk resize %s %llu\n", string + 2,
		((unsigned long long)size * 1024)) < 0) {
		printf("%s: fprintf: %s\n", __func__, strerror(errno));
		return -EIO;
	}
	printf("Resized device %s to %lu Mbytes\n", string, size);
	fclose(fp);

	return 0;
}

int cache_map(struct RD_PROFILE *rd_prof, struct RC_PROFILE * rc_prof,
	        unsigned char *cache, unsigned char *source, int mode)
{
	int rc = INVALID_VALUE, node, fd;
	unsigned long long source_sz = 0, cache_sz = 0;
	FILE *fp;
	unsigned char *buf, string[BUFSZ], name[NAMELEN] = {0}, str[NAMELEN - 6] = {0}, *dup, *token;

	while (rd_prof != NULL) {
		if (strcmp(cache, rd_prof->device) == SUCCESS) {
			rc = SUCCESS;
		}
		rd_prof = rd_prof->next;
	}
	if (rc != SUCCESS) {
		printf("Error. Device %s does not exist.\n", cache);
		return -ENOENT;
	}

	/* Check to make sure it is a normal block device found in /dev */
	if (strncmp(source, "/dev/", 5) != SUCCESS) {
		printf("Error. Source device does not seem to be a normal block device listed\n"
			"in the /dev directory path.\n\n");
		return INVALID_VALUE;
	}
	/* Check to make sure that cache/source devices are not in a mapping already */
	while (rc_prof != NULL) {
		if ((strcmp(cache, rc_prof->cache) == SUCCESS) || \
		    (strcmp(source+5, rc_prof->source) == SUCCESS)) {
			printf("Error. At least one of your cache/source devices is currently mapped to %s.\n\n",
				rc_prof->device);
			return INVALID_VALUE;
		}
		rc_prof = rc_prof->next;
	}

	if ((buf = (char *)malloc(BUFSZ)) == NULL) {
		printf("%s: malloc: Unable to allocate memory.\n", __func__);
		return INVALID_VALUE;
	}
	if ((fp = fopen(etc_mtab, "r")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, etc_mtab, strerror(errno));
		return INVALID_VALUE;
	}
	fread(buf, BUFSZ, 1, fp);
	fclose(fp);
	if ((strstr(buf, cache) != NULL)) {
		printf("%s is currently mounted. Please \"umount\" and retry.\n", cache);
		return INVALID_VALUE;
	}
	if ((strstr(buf, source) != NULL)) {
		printf("%s is currently mounted. Please \"umount\" and retry.\n", source);
		return INVALID_VALUE;
	}

	if ((fd = open(source, O_RDONLY)) < SUCCESS) {
		printf("%s: open: %s\n", __func__, strerror(errno));
		return -ENOENT;
	}

	if (ioctl(fd, BLKGETSIZE, &source_sz) == INVALID_VALUE) {
		printf("%s: ioctl: %s\n", __func__, strerror(errno));
		return -EIO;
	}
	close(fd);

	sprintf(name, "/dev/%s", cache);
	if ((fd = open(name, O_RDONLY)) < SUCCESS) {
		printf("%s: open: %s\n", __func__, strerror(errno));
		return -ENOENT;
	}

	if (ioctl(fd, BLKGETSIZE, &cache_sz) == INVALID_VALUE) {
		printf("%s: ioctl: %s\n", __func__, strerror(errno));
		return -EIO;
	}
	close(fd);
	memset(name, 0x0, sizeof(name));

	dup = strdup(source);
	token = strtok((char *)dup, "/");
	while (token != NULL) {
		sprintf(str, "%s", token);
		token = strtok(NULL, "/");
	}
	if (mode == WRITETHROUGH)
		sprintf(name, "rc-wt_%s", str);
	else
		sprintf(name, "rc-wa_%s", str);

	memset(string, 0x0, BUFSZ);
	/* echo 0 4194303 rapiddisk-cache /dev/sdb /dev/rd0 0 196608|dmsetup create rc-wt_sdb    */
	/* Param after echo 0 & 1: starting and ending offsets of source device                  *
	 * Param 3: always rapiddisk-cache; Param 4 & 5: path to source device and RapidDisk dev *
	 * Param 6: is the size of the cache  */
	sprintf(string, "echo 0 %llu rapiddisk-cache %s /dev/%s %llu %d|dmsetup create %s\n",
		source_sz, source, cache, cache_sz, mode, name);

	if ((rc = system(string)) == SUCCESS) {
		printf("Command to map %s with %s and %s has been sent.\nVerify with \"--list\"\n\n",
			name, cache, source);
	} else
		printf("Error. Unable to create map. Please verify all input values are correct.\n\n");

	return rc;
}

int cache_unmap(struct RC_PROFILE *prof, unsigned char *string)
{
	int rc = INVALID_VALUE;
	FILE *fp;
	unsigned char *buf;
	unsigned char cmd[NAMELEN] = {0};

	/* dmsetup remove rc-wt_sdb */
	while (prof != NULL) {
		if (strcmp(string, prof->device) == SUCCESS)
			rc = SUCCESS;
		prof = prof->next;
	}
	if (rc != SUCCESS) {
		printf("Error. Cache target %s does not exist.\n", string);
		return -ENOENT;
	}

	if ((buf = (char *)malloc(BUFSZ)) == NULL) {
		printf("%s: malloc: Unable to allocate memory.\n", __func__);
		return -ENOMEM;
	}

	/* Here we are starting to check to see if the device is mounted */
	if ((fp = fopen(etc_mtab, "r")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, etc_mtab, strerror(errno));
		return -ENOENT;
	}
	fread(buf, BUFSZ, 1, fp);
	fclose(fp);
	if ((strstr(buf, string) != NULL)) {
		printf("%s is currently mounted. Please \"umount\" and retry.\n", string);
		return -EBUSY;
	}

	sprintf(cmd, "dmsetup remove %s\n", string);
	if ((rc = system(cmd)) == SUCCESS)
		printf("Command to unmap %s has been sent\nVerify with \"--list\"\n\n", string);
	else {
		printf("Error. Unable to unmap %s. Please check to make sure "
		       "nothing is wrong.n\n", string);
	}
	return rc;
}

int rdsk_flush(struct RD_PROFILE *rd_prof, RC_PROFILE *rc_prof, unsigned char *string)
{
	int fd, rc = INVALID_VALUE;
	unsigned char file[NAMELEN] = {0}, *buf;
	FILE *fp;

	while (rd_prof != NULL) {
		if (strcmp(string, rd_prof->device) == SUCCESS)
			rc = SUCCESS;
		rd_prof = rd_prof->next;
	}
	if (rc != SUCCESS) {
		printf("Error. Device %s does not exist.\n", string);
		return -ENOENT;
	}
	/* Check to make sure RapidDisk device isn't in a mapping */
	while (rc_prof != NULL) {
		if (strcmp(string, rc_prof->cache) == SUCCESS) {
			printf("Error. Unable to remove %s.\nThis RapidDisk device "
			       "is currently mapped as a cache drive to %s.\n\n",
			       string, rc_prof->device);
			return -EBUSY;
		}
		rc_prof = rc_prof->next;
	}

	if ((buf = (char *)malloc(BUFSZ)) == NULL) {
		printf("%s: malloc: Unable to allocate memory.\n", __func__);
		return -ENOMEM;
	}

	/* Here we are starting to check to see if the device is mounted */
	if ((fp = fopen(etc_mtab, "r")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, etc_mtab, strerror(errno));
		return -ENOENT;
	}
	fread(buf, BUFSZ, 1, fp);
	fclose(fp);
	if ((strstr(buf, string) != NULL)) {
		printf("%s is currently mounted. Please \"umount\" and retry.\n", string);
		return -EBUSY;
	}

	sprintf(file, "/dev/%s", string);

	if ((fd = open(file, O_WRONLY)) < SUCCESS) {
		printf("%s: open: %s\n", __func__, strerror(errno));
		return -ENOENT;
	}

	if (ioctl(fd, BLKFLSBUF, 0) == INVALID_VALUE) {
		printf("%s: ioctl: %s\n", __func__, strerror(errno));
		return -EIO;
	}
	close(fd);
	printf("Flushed all data from device %s\n", string);

	return SUCCESS;
}
