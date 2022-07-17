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
 ** @filename: rdsk.c
 ** @description: This file contains the core routines of rapiddisk.
 **
 ** @date: 15Oct10, petros@petroskoutoupis.com
 ********************************************************************************/

#include "common.h"
#include "cli.h"
#include <malloc.h>
#include <linux/fs.h>
#include <sys/ioctl.h>

struct RD_PROFILE *rdsk_head =  (struct RD_PROFILE *) NULL;
struct RD_PROFILE *rdsk_end =   (struct RD_PROFILE *) NULL;
struct RC_PROFILE *cache_head = (struct RC_PROFILE *) NULL;
struct RC_PROFILE *cache_end =  (struct RC_PROFILE *) NULL;

unsigned char *read_info(unsigned char *name, unsigned char *string)
{
	int len;
	unsigned char file[NAMELEN] = {0};
	unsigned char buf[0xFF] = {0};
	static unsigned char obuf[0xFF] = {0};
	FILE *fp = NULL;

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

struct RD_PROFILE *search_rdsk_targets(void)
{
	int rc, n = 0;
	unsigned char file[NAMELEN] = {0};
	struct dirent **list;
	struct RD_PROFILE *prof = NULL;

	if ((rc = scandir(SYS_BLOCK, &list, NULL, NULL)) < 0) {
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
			sprintf(file, "%s/%s", SYS_BLOCK, list[n]->d_name);
			prof->size = (BYTES_PER_SECTOR * strtoull(read_info(file, "size"), NULL, 10));
			prof->lock_status = mem_device_lock_status(prof->device);
			prof->usage = mem_device_get_usage(prof->device);

			if (rdsk_head == NULL)
				rdsk_head = prof;
			else
				rdsk_end->next = prof;
			rdsk_end = prof;
			prof->next = NULL;
		}
		if (list[n] != NULL) free(list[n]);
	}
	return rdsk_head;
}

struct RC_PROFILE *search_cache_targets(void)
{
	int num, num2, num3, n = 0, i, z;
	struct dirent **list, **nodes, **maps;
	unsigned char file[NAMELEN] = {0};
	struct RC_PROFILE *prof = NULL;

	if ((num = scandir(DEV_MAPPER, &list, NULL, NULL)) < 0) return NULL;
	if ((num2 = scandir(SYS_BLOCK, &nodes, NULL, NULL)) < 0) {
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
					sprintf(file, "%s/%s", SYS_BLOCK, nodes[i]->d_name);
					if (strncmp(read_info(file, "dm/name"), prof->device,
					    sizeof(prof->device)) == 0) {
						sprintf(file, "%s/%s/slaves", SYS_BLOCK, nodes[i]->d_name);
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
			if (cache_head == NULL)
				cache_head = prof;
			else
				cache_end->next = prof;
			cache_end = prof;
			prof->next = NULL;
		}
		if (list[n] != NULL) free(list[n]);
	}
	for (i = 0;i < num2; i++) if (nodes[i] != NULL) free(nodes[i]);
	return cache_head;
}

int mem_device_list(struct RD_PROFILE *rd_prof, struct RC_PROFILE *rc_prof)
{
	int num = 1;
	unsigned char status[0xf] = {0};

	printf("List of RapidDisk device(s):\n\n");

	while (rd_prof != NULL) {
		memset(status, 0x0, sizeof(status));
		if (rd_prof->lock_status == TRUE) {
			sprintf(status, "Locked");
		} else if (rd_prof->lock_status == FALSE) {
			sprintf(status, "Unlocked");
		} else {
			sprintf(status, "Unavailable");
		}

		printf(" RapidDisk Device %d: %s\tSize (KB): %llu\tUsage (KB): %llu\tStatus: %s\n", num,
		       rd_prof->device, (rd_prof->size / 1024), (rd_prof->usage / 1024), status);
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
		if (strstr(rc_prof->device, "rc-wb") != NULL) {
			printf(" dm-writecache Target   %d: %s\tCache: %s  Target: %s (WRITEBACK)\n",
				num, rc_prof->device, rc_prof->cache, rc_prof->source);
		} else {
			printf(" RapidDisk-Cache Target %d: %s\tCache: %s  Target: %s (%s)\n",
				num, rc_prof->device, rc_prof->cache, rc_prof->source,
				(strncmp(rc_prof->device, "rc-wt_", 5) == 0) ? \
				"WRITE THROUGH" : "WRITE AROUND");
		}
		num++;
		rc_prof = rc_prof->next;
	}
list_out:
	printf("\n");
	return SUCCESS;
}

int cache_device_stat(struct RC_PROFILE *rc_prof, unsigned char *cache)
{
	unsigned char cmd[NAMELEN] = {0};

	if (rc_prof == NULL) {
		printf("  No RapidDisk-Cache Mappings exist.\n\n");
		return 1;
	}
	sprintf(cmd, "dmsetup status %s", cache);
	system(cmd);
	return SUCCESS;
}

int cache_device_stat_json(struct RC_PROFILE *rc_prof, unsigned char *cache)
{
	int rc = INVALID_VALUE;
	unsigned char cmd[NAMELEN] = {0};
	FILE *stream;
	unsigned char *buf = NULL, *dup = NULL, *token = NULL;
	struct RC_STATS *stats = NULL;

	if (rc_prof == NULL) {
		printf("  No RapidDisk-Cache Mappings exist.\n\n");
		return 1;
	}

	buf = (unsigned char *)calloc(1, BUFSZ);
	if (buf == NULL) {
		printf("%s: %s: calloc: %s\n", DAEMON, __func__, strerror(errno));
		return INVALID_VALUE;
	}

	stats = (struct RC_STATS *)calloc(1, sizeof(struct RC_STATS));
	if (stats == NULL) {
		printf("%s: calloc: %s\n", __func__, strerror(errno));
		return INVALID_VALUE;
	}

	/* This whole thing is ugly */
	sprintf(cmd, "OUTPUT=`dmsetup status %s|tail -n +2|sed -e 's/\\t//g' -e 's/ /_/g' -e 's/(/ /g' -e 's/)//g' -e 's/,_/ /g'`; echo $OUTPUT", cache);
	stream = popen(cmd, "r");
	if (stream) {
		while (fgets(buf, BUFSZ, stream) != NULL);
		pclose(stream);
	}

	sprintf(stats->device, "%s", cache);
	dup = strdup(buf);
	token = strtok((char *)dup, " "); /* reads */
	token = strtok(NULL, " ");
	stats->reads = atoi(token);
	token = strtok(NULL, " ");        /* writes */
	token = strtok(NULL, " ");
	stats->writes = atoi(token);
	token = strtok(NULL, " ");        /* cache hits */
	token = strtok(NULL, " ");
	stats->cache_hits = atoi(token);
	token = strtok(NULL, " ");        /* replacement */
	token = strtok(NULL, " ");
	stats->replacement = atoi(token);
	token = strtok(NULL, " ");        /* writes replacement */
	token = strtok(NULL, " ");
	stats->write_replacement = atoi(token);
	token = strtok(NULL, " ");        /* read invalidates */
	token = strtok(NULL, " ");
	stats->read_invalidates = atoi(token);
	token = strtok(NULL, " ");        /* write invalidates */
	token = strtok(NULL, " ");
	stats->write_invalidates = atoi(token);
	token = strtok(NULL, " ");        /* uncached reads */
	token = strtok(NULL, " ");
	stats->uncached_reads = atoi(token);
	token = strtok(NULL, " ");        /* uncached writes */
	token = strtok(NULL, " ");
	stats->uncached_writes = atoi(token);
	token = strtok(NULL, " ");        /* disk reads */
	token = strtok(NULL, " ");
	stats->disk_reads = atoi(token);
	token = strtok(NULL, " ");        /* disk writes */
	token = strtok(NULL, " ");
	stats->disk_writes = atoi(token);
	token = strtok(NULL, " ");        /* cache reads */
	token = strtok(NULL, " ");
	stats->cache_reads = atoi(token);
	token = strtok(NULL, " ");        /* cache writes */
	token = strtok(NULL, " ");
	stats->cache_writes = atoi(token);

	rc = json_cache_statistics(stats);

	if (dup) free(dup);
	if (stats) free(stats);
	if (buf) free(buf);
	return rc;
}

int cache_wb_device_stat_json(struct RC_PROFILE *rc_prof, unsigned char *cache)
{
	int rc = INVALID_VALUE;
	unsigned char cmd[NAMELEN] = {0};
	FILE *stream;
	unsigned char *buf = NULL, *dup = NULL, *token = NULL;
	struct WC_STATS *stats = NULL;

	if (rc_prof == NULL) {
		printf("  No RapidDisk-Cache Mappings exist.\n\n");
		return 1;
	}

	buf = (unsigned char *)calloc(1, BUFSZ);
	if (buf == NULL) {
		printf("%s: %s: calloc: %s\n", DAEMON, __func__, strerror(errno));
		return INVALID_VALUE;
	}

	stats = (struct WC_STATS *)calloc(1, sizeof(struct WC_STATS));
	if (stats == NULL) {
		printf("%s: calloc: %s\n", __func__, strerror(errno));
		return INVALID_VALUE;
	}

	sprintf(cmd, "OUTPUT=`dmsetup status %s`; echo $OUTPUT", cache);
	stream = popen(cmd, "r");
	if (stream) {
		while (fgets(buf, BUFSZ, stream) != NULL);
		pclose(stream);
	}

	sprintf(stats->device, "%s", cache);
	stats->expanded = FALSE;
	dup = strdup(buf);

	token = strtok((char *)dup, " ");
	token = strtok(NULL, " ");
	token = strtok(NULL, " ");
	token = strtok(NULL, " ");     /* errors */
	stats->errors = atoi(token);
	token = strtok(NULL, " ");     /* num blocks */
	stats->num_blocks = atoi(token);
	token = strtok(NULL, " ");     /* free blocks */
	stats->num_free_blocks = atoi(token);
	token = strtok(NULL, " ");     /* num wb blocks */
	stats->num_wb_blocks = atoi(token);
	token = strtok(NULL, " ");     /* num read requests */
	if (!token) goto abort_stat_collect;
	stats->expanded = TRUE;
	stats->num_read_req = atoi(token);
	token = strtok(NULL, " ");     /* num read cache hits */
	if (!token) goto abort_stat_collect;
	stats->num_read_cache_hits = atoi(token);
	token = strtok(NULL, " ");     /* num write requests */
	if (!token) goto abort_stat_collect;
	stats->num_write_req = atoi(token);
	token = strtok(NULL, " ");     /* num write uncommitted block hits */
	if (!token) goto abort_stat_collect;
	stats->num_write_uncommitted_blk_hits = atoi(token);
	token = strtok(NULL, " ");     /* num write committed block hits */
	if (!token) goto abort_stat_collect;
	stats->num_write_committed_blk_hits = atoi(token);
	token = strtok(NULL, " ");     /* num write cache bypassed */
	if (!token) goto abort_stat_collect;
	stats->num_write_cache_bypass = atoi(token);
	token = strtok(NULL, " ");     /* num write cache allocated */
	if (!token) goto abort_stat_collect;
	stats->num_write_cache_alloc = atoi(token);
	token = strtok(NULL, " ");     /* num writes blocked on freelist */
	if (!token) goto abort_stat_collect;
	stats->num_write_freelist_blocked = atoi(token);
	token = strtok(NULL, " ");     /* num flush requests */
	if (!token) goto abort_stat_collect;
	stats->num_flush_req = atoi(token);
	token = strtok(NULL, " ");     /* num discard requests */
	if (!token) goto abort_stat_collect;
	stats->num_discard_req = atoi(token);

abort_stat_collect:

	rc = json_cache_wb_statistics(stats);

	if (dup) free(dup);
	if (stats) free(stats);
	if (buf) free(buf);
	return rc;
}

int mem_device_attach(struct RD_PROFILE *prof, unsigned long long size)
{
	int dsk;
	FILE *fp = NULL;
	unsigned char string[BUFSZ] = {0}, name[16] = {0};

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
	if (fprintf(fp, "rapiddisk attach %d %llu\n", dsk,
		(size * 1024 * 1024)) < 0) {
		printf("%s: fprintf: %s\n", __func__, strerror(errno));
		return -EIO;
	}

	printf("Attached device rd%d of size %llu Mbytes\n", dsk, size);
	fclose(fp);
	return SUCCESS;
}

int mem_device_detach(struct RD_PROFILE *rd_prof, RC_PROFILE * rc_prof, unsigned char *string)
{
	int rc = INVALID_VALUE;
	FILE *fp = NULL;
	unsigned char *buf = NULL;

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
	if ((fp = fopen(ETC_MTAB, "r")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, ETC_MTAB, strerror(errno));
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

	if (buf) free(buf);

	return SUCCESS;
}

int mem_device_resize(struct RD_PROFILE *prof, unsigned char *string, unsigned long long size)
{
	int rc = INVALID_VALUE;
	FILE *fp = NULL;

	/* echo "rapiddisk resize 1 128" > /sys/kernel/rapiddisk/mgmt */
	while (prof != NULL) {
		if (strcmp(string, prof->device) == SUCCESS) {
			if ((size * 1024) <= (prof->size / 1024)) {
				printf("Error. Please specify a size larger than %llu Mbytes\n", size);
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

	if (fprintf(fp, "rapiddisk resize %s %llu\n", string + 2, (size * 1024 * 1024)) < 0) {
		printf("%s: fprintf: %s\n", __func__, strerror(errno));
		return -EIO;
	}
	printf("Resized device %s to %llu Mbytes\n", string, size);
	fclose(fp);

	return 0;
}

int cache_device_map(struct RD_PROFILE *rd_prof, struct RC_PROFILE * rc_prof,
		     unsigned char *cache, unsigned char *source, int mode)
{
	int rc = INVALID_VALUE, node, fd;
	unsigned long long source_sz = 0, cache_sz = 0;
	FILE *fp = NULL;
	unsigned char *buf, string[BUFSZ] = {0}, name[NAMELEN] = {0}, str[NAMELEN - 6] = {0}, *dup = NULL, *token = NULL;

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
	if ((fp = fopen(ETC_MTAB, "r")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, ETC_MTAB, strerror(errno));
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
	else if (mode == WRITEBACK)    /* very dangerous mode */
		sprintf(name, "rc-wb_%s", str);
	else
		sprintf(name, "rc-wa_%s", str);

	memset(string, 0x0, BUFSZ);
	if (mode == WRITEBACK)    /* very dangerous mode */
		sprintf(string, "echo 0 %llu writecache s %s /dev/%s 4096 0|dmsetup create %s\n",
			source_sz, source, cache, name);
	else {
		/* echo 0 4194303 rapiddisk-cache /dev/sdb /dev/rd0 0 196608|dmsetup create rc-wt_sdb    */
		/* Param after echo 0 & 1: starting and ending offsets of source device                  *
		 * Param 3: always rapiddisk-cache; Param 4 & 5: path to source device and RapidDisk dev *
		 * Param 6: is the size of the cache  */
		sprintf(string, "echo 0 %llu rapiddisk-cache %s /dev/%s %llu %d|dmsetup create %s\n",
			source_sz, source, cache, cache_sz, mode, name);
	}

	if ((rc = system(string)) == SUCCESS) {
		printf("Command to map %s with %s and %s has been sent.\nVerify with \"-l\"\n\n",
			name, cache, source);
	} else
		printf("Error. Unable to create map. Please verify all input values are correct.\n\n");

	return rc;
}

int cache_device_unmap(struct RC_PROFILE *prof, unsigned char *string)
{
	int rc = INVALID_VALUE;
	FILE *fp = NULL;
	unsigned char *buf = NULL;
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
	if ((fp = fopen(ETC_MTAB, "r")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, ETC_MTAB, strerror(errno));
		return -ENOENT;
	}
	fread(buf, BUFSZ, 1, fp);
	fclose(fp);
	if ((strstr(buf, string) != NULL)) {
		printf("%s is currently mounted. Please \"umount\" and retry.\n", string);
		return -EBUSY;
	}

	if (strstr(string, "rc-wb") != NULL) {
		sprintf(cmd, "dmsetup message %s 0 flush\n", string);
		if ((rc = system(cmd)) != SUCCESS)
			printf("Unable to flush dirty cache data to %s\n", string);
	}

	sprintf(cmd, "dmsetup remove %s\n", string);
	if ((rc = system(cmd)) == SUCCESS)
		printf("Command to unmap %s has been sent\nVerify with \"-l\"\n\n", string);
	else {
		printf("Error. Unable to unmap %s. Please check to make sure "
		       "nothing is wrong.n\n", string);
	}
	return rc;
}

int mem_device_flush(struct RD_PROFILE *rd_prof, RC_PROFILE *rc_prof, unsigned char *string)
{
	int fd, rc = INVALID_VALUE;
	unsigned char file[NAMELEN] = {0}, *buf = NULL;
	FILE *fp = NULL;

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
	if ((fp = fopen(ETC_MTAB, "r")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, ETC_MTAB, strerror(errno));
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

int mem_device_lock(struct RD_PROFILE *rd_prof, unsigned char *string, bool lock)
{
	int fd, rc = INVALID_VALUE, state = lock;
	unsigned char file[NAMELEN] = {0};

	while (rd_prof != NULL) {
		if (strcmp(string, rd_prof->device) == SUCCESS) {
			rc = SUCCESS;
		}
		rd_prof = rd_prof->next;
	}

	if (rc != SUCCESS) {
		printf("Error. Device %s does not exist.\n", string);
		return -ENOENT;
	}

	sprintf(file, "/dev/%s", string);

	if ((fd = open(file, O_WRONLY)) < SUCCESS) {
		printf("%s: open: %s\n", __func__, strerror(errno));
		return -ENOENT;
	}

	if((ioctl(fd, BLKROSET, &state)) == INVALID_VALUE){
		printf("%s: ioctl: %s\n", __func__, strerror(errno));
		return -EIO;
	}

	close(fd);
	printf("Device %s is now set to %s.\n", string, ((lock == TRUE) ? "read-only" : "read-write"));

	return SUCCESS;
}

int mem_device_lock_status(unsigned char *string)
{
	int fd, rc = INVALID_VALUE;
	unsigned char file[NAMELEN] = {0};

	sprintf(file, "/dev/%s", string);

	if ((fd = open(file, O_WRONLY)) < SUCCESS)
		return -ENOENT;

	if((ioctl(fd, BLKROGET, &rc)) == INVALID_VALUE)
		return -EIO;

	close(fd);

	return rc;
}

unsigned long long mem_device_get_usage(unsigned char *string)
{
	int fd;
	unsigned long long rc = INVALID_VALUE;
	unsigned char file[NAMELEN] = {0};

	sprintf(file, "/dev/%s", string);

	if ((fd = open(file, O_WRONLY)) < SUCCESS)
		return -ENOENT;

	if((ioctl(fd, RD_GET_USAGE, &rc)) == INVALID_VALUE)
		return -EIO;

	close(fd);

	return (rc * PAGE_SIZE);
}
