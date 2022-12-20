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

#include "rdsk.h"
#include "utils.h"

#ifdef SERVER
#include "rapiddiskd.h"
#endif
#include <malloc.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <libdevmapper.h>

#define BYTES_PER_BLOCK		512
#define IOCTL_RD_GET_STATS	0x0529
#define IOCTL_RD_BLKFLSBUF	0x0531

struct RD_PROFILE *rdsk_head = NULL;
struct RD_PROFILE *rdsk_end = NULL;
struct RC_PROFILE *cache_head = NULL;
struct RC_PROFILE *cache_end = NULL;

static const size_t RC_STATS_OFFSETS[] = {
		offsetof(RC_STATS, device),
		offsetof(RC_STATS, reads),
		offsetof(RC_STATS, writes),
		offsetof(RC_STATS, cache_hits),
		offsetof(RC_STATS, replacement),
		offsetof(RC_STATS, write_replacement),
		offsetof(RC_STATS, read_invalidates),
		offsetof(RC_STATS, write_invalidates),
		offsetof(RC_STATS, uncached_reads),
		offsetof(RC_STATS, uncached_writes),
		offsetof(RC_STATS, disk_reads),
		offsetof(RC_STATS, disk_writes),
		offsetof(RC_STATS, cache_reads),
		offsetof(RC_STATS, cache_writes),

		/* Unsupported in this release */
		offsetof(RC_STATS, read_ops),
		offsetof(RC_STATS, write_ops),
};

static const size_t WC_STATS_OFFSETS[] =  {
		offsetof(WC_STATS, device),
		offsetof(WC_STATS, expanded),
		offsetof(WC_STATS, errors),
		offsetof(WC_STATS, num_blocks),
		offsetof(WC_STATS, num_free_blocks),
		offsetof(WC_STATS, num_wb_blocks),
		/* For 5.15 and later */
		offsetof(WC_STATS, num_read_req),
		offsetof(WC_STATS, num_read_cache_hits),
		offsetof(WC_STATS, num_write_req),
		offsetof(WC_STATS, num_write_uncommitted_blk_hits),
		offsetof(WC_STATS, num_write_committed_blk_hits),
		offsetof(WC_STATS, num_write_cache_bypass),
		offsetof(WC_STATS, num_write_cache_alloc),
		offsetof(WC_STATS, num_write_freelist_blocked),
		offsetof(WC_STATS, num_flush_req),
		offsetof(WC_STATS, num_discard_req),
};

/**
 * It's a printf() function that can optionally return the formatted string instead of printing it
 *
 * @param format_string The error format string.
 * @param return_message This is a pointer to a string that will be filled with the error message. If this is NULL, the
 * error message will be printed to stdout.
 * @param ... Values to be inserted.
 */
void print_error(char *format_string, char *return_message, ...) {
	va_list args;
	va_start(args, return_message);
	if (return_message) {
		vsprintf(return_message, format_string, args);
	} else {
		vprintf(format_string, args);
		printf("\n");
	}
	va_end(args);
}

/**
 * It reads a file and returns the contents of the file
 *
 * @param name The name of the directory to read from.
 * @param string The file to read from.
 * @param return_message This is the message that will be returned to the user.
 *
 * @return A pointer to a static buffer.
 */
char *read_info(char *name, char *string, char *return_message)
{
	size_t len;
	char file[NAMELEN] = {0};
	char buf[0xFF] = {0};
	static char obuf[0xFF] = {0};
	FILE *fp = NULL;
	size_t r;
	char *msg;

	memset(&buf, 0, sizeof(buf));
	memset(&obuf, 0, sizeof(obuf));

	sprintf(file, "%s/%s", name, string);
	fp = fopen(file, "r");
	if (fp == NULL) {
		msg = ERR_FOPEN;
		print_error(msg, return_message, __func__, strerror(errno), file);
		return NULL;
	}
	r = fread(buf, FILEDATA, 1, fp);
	if ((r != 1) && (feof(fp) == 0) && (ferror(fp) != 0)) {
		msg = ERR_FREAD;
		print_error(msg, return_message, __func__, "could not read file", file);
		return NULL;
	}
	fclose(fp);
	len = strlen(buf);
	strncpy(obuf, buf, (len - 1));
	sprintf(obuf, "%s", obuf);

	return obuf;
}

/**
 * scandir() filter: "If the first two characters of the file name are 'rd', return TRUE, otherwise return FALSE"
 *
 * @param list This is the directory entry that is being passed to the function.
 *
 * @return the value of the comparison of the first two characters of the file name to the string "rd".
 */
int scandir_filter_rd(const struct dirent *list) {
	if (strncmp(list->d_name, "rd", 2) == SUCCESS) {
		return TRUE;
	}
	return FALSE;
}

/**
 * It searches the /sys/block directory for any device that starts with "rd" and then populates a linked list of struct
 * RD_PROFILE's with the device name, size, lock status, and usage
 *
 * @param return_message This is a pointer to a string that will be used to return error messages.
 *
 * @return A pointer to the first element in the linked list.
 */
struct RD_PROFILE *search_rdsk_targets(char *return_message)
{
	int rc, n = 0;
	char file[NAMELEN] = {0};
	struct dirent **list;
	struct RD_PROFILE *prof = NULL;
	char *msg;

	rdsk_head = NULL;
	rdsk_end = NULL;

	if ((rc = scandir(SYS_BLOCK, &list, scandir_filter_rd, NULL)) < 0) {
		msg = ERR_SCANDIR;
		print_error(msg, return_message, __func__, strerror(errno));
		return NULL;
	}
	for (;n < rc; n++) {
		prof = calloc(1, sizeof(struct RD_PROFILE));
		if (prof == NULL) {
			msg = ERR_CALLOC;
			print_error(msg, return_message, __func__, strerror(errno));
			list = clean_scandir(list, rc);
			free_linked_lists(NULL, rdsk_head, NULL);
			return NULL;
		}
		strcpy(prof->device, (char *)list[n]->d_name);
		sprintf(file, "%s/%s", SYS_BLOCK, list[n]->d_name);
		char *info = read_info(file, "size", return_message);
		if (info == NULL) {
			free(prof);
			prof = NULL;
			list = clean_scandir(list, rc);
			free_linked_lists(NULL, rdsk_head, NULL);
			return NULL;
		}
		prof->size = (BYTES_PER_SECTOR * strtoull(info, NULL, 10));
		prof->lock_status = mem_device_lock_status(prof->device);
		prof->usage = mem_device_get_usage(prof->device);

		if (rdsk_head == NULL)
			rdsk_head = prof;
		else
			rdsk_end->next = prof;
		rdsk_end = prof;
		prof->next = NULL;
	}
	list = clean_scandir(list, rc);
	return rdsk_head;
}

/**
 * scandir() filter: "If the first two characters of the file name are 'rc', return TRUE, otherwise return FALSE."
 *
 * The function is passed a pointer to a struct dirent, which is defined in the dirent.h header file. The struct dirent
 * contains the following members:
 *
 * @param list This is the directory entry that is being passed to the function.
 *
 * @return TRUE or FALSE
 */
int scandir_filter_rc(const struct dirent *list) {
	if (strncmp(list->d_name, "rc", 2) == SUCCESS) {
		return TRUE;
	}
	return FALSE;
}

/**
 * scandir() filter: "If the name of the directory/file entry begins with 'dm-', return TRUE, otherwise return FALSE"
 *
 * @param list This is the directory entry that is being passed to the function.
 *
 * @return TRUE or FALSE
 */
int scandir_filter_dm(const struct dirent *list) {
	if (strncmp(list->d_name, "dm-", 3) == SUCCESS) {
		return TRUE;
	}
	return FALSE;
}

/**
 * It searches for all the cache targets in the system and returns a linked list of them
 *
 * @param return_message This is a pointer to a string that will be used to return any error messages.
 *
 * @return A pointer to the first element of a linked list of structs.
 */
struct RC_PROFILE *search_cache_targets(char *return_message)
{
	int num, num2, num3, n = 0, i, z;
	struct dirent **list, **nodes, **maps;
	char file[NAMELEN] = {0};
	struct RC_PROFILE *prof = NULL;
	char *msg;

	cache_head = NULL;
	cache_end = NULL;

	if ((num = scandir(DEV_MAPPER, &list, scandir_filter_rc, NULL)) < 0) {
		msg = ERR_SCANDIR;
		print_error(msg, return_message, __func__, strerror(errno));
		return NULL;
	}
	if ((num2 = scandir(SYS_BLOCK, &nodes, scandir_filter_dm, NULL)) < 0) {
		msg = ERR_SCANDIR;
		print_error(msg, return_message, __func__, strerror(errno));
		list = clean_scandir(list, num);
		return NULL;
	}

	for (;n < num; n++) {
		prof = calloc(1, sizeof(struct RC_PROFILE));
		if (prof == NULL) {
			msg = ERR_CALLOC;
			print_error(msg, return_message, __func__, strerror(errno));
			list = clean_scandir(list, num);
			nodes = clean_scandir(nodes, num2);
			free_linked_lists(cache_head, NULL, NULL);
			return NULL;
		}
		strcpy(prof->device, list[n]->d_name);
		for (i = 0; i < num2; i++) {
			sprintf(file, "%s/%s", SYS_BLOCK, nodes[i]->d_name);
			char *info_size = read_info(file, "dm/name", return_message);
			if (info_size == NULL) {
				list = clean_scandir(list, num);
				nodes = clean_scandir(nodes, num2);
				free(prof);
				free_linked_lists(cache_head, NULL, NULL);
				return NULL;
			}
			if (strncmp(info_size, prof->device, sizeof(prof->device)) == 0) {
				sprintf(file, "%s/%s/slaves", SYS_BLOCK, nodes[i]->d_name);
				if ((num3 = scandir(file, &maps, NULL, NULL)) < 0) {
					msg = ERR_SCANDIR;
					print_error(msg, return_message, __func__, strerror(errno));
					list = clean_scandir(list, num);
					nodes = clean_scandir(nodes, num2);
					free(prof);
					free_linked_lists(cache_head, NULL, NULL);
					return NULL;
				}
				for (z=0;z < num3; z++) {
					if (strncmp(maps[z]->d_name, ".", 1) != SUCCESS) {
						if (strncmp(maps[z]->d_name, "rd", 2) == SUCCESS)
							strcpy(prof->cache, (char *)maps[z]->d_name);
						else
							strcpy(prof->source, (char *)maps[z]->d_name);
					}
				}
				maps = clean_scandir(maps, num3);
			}
		}
		if (cache_head == NULL)
			cache_head = prof;
		else
			cache_end->next = prof;
		cache_end = prof;
		prof->next = NULL;
	}
	list = clean_scandir(list, num);
	nodes = clean_scandir(nodes, num2);
	return cache_head;
}

/**
 * This function takes a string as an argument and returns the lock status of the device
 *
 * @param string The name of the device to lock.
 * @result The lock status of the device.
 */
int mem_device_lock_status(char *string)
{
	int fd, rc = INVALID_VALUE;
	char file[NAMELEN] = {0};

	sprintf(file, "/dev/%s", string);

	if ((fd = open(file, O_WRONLY)) < SUCCESS)
		return -ENOENT;

	if((ioctl(fd, BLKROGET, &rc)) == INVALID_VALUE) {
		close(fd);
		return -EIO;
	}

	close(fd);

	return rc;
}

/**
 * It opens the device file, calls the ioctl() function to get the usage, and returns the result
 *
 * @param string The name of the device.
 *
 * @return The amount of memory used by the device.
 */
unsigned long long mem_device_get_usage(char *string)
{
	int fd;
	unsigned long long rc = INVALID_VALUE;
	char file[NAMELEN] = {0};

	sprintf(file, "/dev/%s", string);

	if ((fd = open(file, O_WRONLY)) < SUCCESS)
		return -ENOENT;

	if ((ioctl(fd, RD_GET_USAGE, &rc)) == INVALID_VALUE) {
		close(fd);
		return -EIO;
	}
	close(fd);

	return (rc * PAGE_SIZE);
}

/**
 * It sends a flush command to the device mapper
 *
 * @param device the device name, e.g. /dev/mapper/my_device
 *
 * @return The return value is the return value of the dm_task_run function.
 */
int dm_flush_device(char *device) {
	int cmdno = DM_DEVICE_TARGET_MSG;
	struct dm_task *dmt;
	uint64_t sector = 0;
	char *command = "flush";

	if (!(dmt = dm_task_create(cmdno))) {
		return INVALID_VALUE;
	}
	if (!dm_task_set_name(dmt, device)) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}
	if (!dm_task_set_sector(dmt, sector)) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}
	if (!dm_task_set_message(dmt, command)) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}
	if (!dm_task_run(dmt)) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}
	const char *response = dm_task_get_message_response(dmt);
	dm_task_destroy(dmt);
	return SUCCESS;
}

/**
 * It removes a device mapping
 *
 * @param device the name of the device to be removed.
 *
 * @return The return value is the status of the operation.
 */
int dm_remove_mapping(char *device) {
	int cmdno = DM_DEVICE_REMOVE;
	struct dm_task *dmt;
	uint32_t cookie = 0;
	uint16_t udev_flags = 0;

	if (!(dmt = dm_task_create(cmdno))) {
		return INVALID_VALUE;
	}
	if (!dm_task_set_name(dmt, device)) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}
	if (!dm_task_set_add_node(dmt, DM_ADD_NODE_ON_CREATE)) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}
	if (!dm_task_set_cookie(dmt, &cookie, udev_flags)) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}
	if (!dm_task_run(dmt)) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}

	dm_udev_wait(cookie);
	dm_task_destroy(dmt);

	return SUCCESS;
}

/**
 * It takes a device name and a cache type, and returns a pointer to a struct containing the cache statistics
 *
 * @param device the device name, e.g. /dev/mapper/cache0
 * @param cache_type WRITETHROUGH, WRITEAROUND or WRITEBACK
 *
 * @return A pointer to a struct containing the cache statistics.
 */
void *dm_get_status(char* device, enum CACHE_TYPE cache_type) {

	int cmdno = DM_DEVICE_STATUS;
	uint64_t start, length;
	char *target_type = NULL;
	char *params;
	struct dm_task *dmt = NULL;
	RC_STATS *rc_stats = NULL;
	WC_STATS *wc_stats = NULL;
	void *stats = NULL;
	struct dm_info info;

	if (!(dmt = dm_task_create(cmdno))) {
		return NULL;
	}
	if (!dm_task_set_name(dmt, device)) {
		dm_task_destroy(dmt);
		return NULL;
	}
	if (!dm_task_run(dmt)) {
		dm_task_destroy(dmt);
		return NULL;
	}
	if (!dm_task_get_info(dmt, &info)) {
		dm_task_destroy(dmt);
		return NULL;
	}
	if (!info.exists) {
		dm_task_destroy(dmt);
		return NULL;
	}

	dm_get_next_target(dmt, NULL, &start, &length,
								  &target_type, &params);
	if (target_type) {
		size_t result_size = strlen(params) * 2;
		char *result = calloc(1, result_size);
		if (preg_replace("[^\\d ]", "", params, result, result_size) < SUCCESS) {
			if (result) free(result);
			dm_task_destroy(dmt);
			return NULL;
		}
		if (preg_replace("  ", " ", result, result, result_size) < SUCCESS) {
			if (result) free(result);
			dm_task_destroy(dmt);
			return NULL;
		}
		char *split_arr[64] = {NULL};
		split(result, split_arr, " ");
		if (cache_type == WRITETHROUGH || cache_type == WRITEAROUND) {
			rc_stats = calloc(1, sizeof(struct RC_STATS));
			sprintf(rc_stats->device, "%s", device);
			int i = 1;
			for (; split_arr[i - 1] != NULL; i++) {
				unsigned int *pvalue = (unsigned int *) (((char *) rc_stats) + RC_STATS_OFFSETS[i]);
				unsigned int v = strtol(split_arr[i - 1], NULL, 10);
				*pvalue = v;
			}
			stats = (void *) rc_stats;
		} else if (cache_type == WRITEBACK) {
			wc_stats = calloc(1, sizeof(struct WC_STATS));
			sprintf(wc_stats->device, "%s", device);
			wc_stats->expanded = FALSE;
			int i = 2;
			for (; split_arr[i - 2] != NULL; i++) {
				unsigned int *pvalue = (unsigned int *) (((char *) wc_stats) + WC_STATS_OFFSETS[i]);
				unsigned int v = strtol(split_arr[i - 2], NULL, 10);
				*pvalue = v;
			}
			if (i > 5) wc_stats->expanded = TRUE;
			stats = (void *) wc_stats;
		}
		if (result) free(result);
	}
	dm_task_destroy(dmt);
	return stats;
}

/**
 * It creates a device-mapper mapping with the name specified in the first argument, and the table specified in the second
 * argument
 *
 * @param final_map_name the name of the device mapper device to create.
 * @param table a table definition, i.e. "0 452591616 rapiddisk-cache /dev/sdb1 /dev/rd2 20480 0"
 */
int dm_create_mapping(char* final_map_name, char *table) {

	int cmdno = DM_DEVICE_CREATE;
	struct dm_task *dmt = NULL;
	struct dm_info info;
	char ttype[4096];
	unsigned long long size, start;
	int n;
	uint32_t cookie = 0;
	uint16_t udev_flags = 0;
	/*	table example: "0 452591616 rapiddisk-cache /dev/sdb1 /dev/rd2 20480 0" */

	if (!(dmt = dm_task_create(cmdno))) {
		return INVALID_VALUE;
	}
	if (!dm_task_set_name(dmt, final_map_name)) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}

	if (sscanf(table, "%llu %llu %s %n",
			   &start, &size, ttype, &n) < 3) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}

	table += n;

	if (!dm_task_add_target(dmt, start, size, ttype, table)) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}

	if (!dm_task_set_add_node(dmt, DM_ADD_NODE_ON_RESUME)) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}

	if (!dm_task_set_cookie(dmt, &cookie, udev_flags)) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}

	if (!dm_task_run(dmt)) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}

	dm_udev_wait(cookie);

	if (!dm_task_get_info(dmt, &info)) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}

	if (!info.exists) {
		dm_task_destroy(dmt);
		return INVALID_VALUE;
	}

	dm_task_destroy(dmt);
	return SUCCESS;
}

/**
 * It takes a ramdisk device, a block device, and a cache mode, and creates a mapping between the two
 *
 * @param rd_prof pointer to the head of the linked list of RD_PROFILE structures
 * @param rc_prof pointer to the head of the linked list of RC_PROFILE structures
 * @param ramdisk the name of the ramdisk device
 * @param block_dev the block device to be mapped to the ramdisk
 * @param cache_mode WRITETHROUGH = 0 = write-through, WRITEAROUND = 1 = write-around, WRITEBACK = 2 = write-back
 * @param return_message This is a pointer to a buffer that will contain the error message if the function fails.
 */
int cache_device_map(struct RD_PROFILE *rd_prof, struct RC_PROFILE *rc_prof, char *ramdisk, char *block_dev, int cache_mode, char *return_message)
{
	int rc = INVALID_VALUE, fd;
	unsigned long long block_dev_sz = 0, ramdisk_sz = 0;
	FILE *fp = NULL;
	char *buf, table[BUFSZ] = {0}, name[NAMELEN] = {0}, str[NAMELEN - 6] = {0};
	char *token, *dup;
	char *msg;

	while (rd_prof != NULL) {
		if (strcmp(ramdisk, rd_prof->device) == SUCCESS) {
			rc = SUCCESS;
		}
		rd_prof = rd_prof->next;
	}
	if (rc != SUCCESS) {
		msg = "Error. Device %s does not exist";
		print_error(msg, return_message, ramdisk);
		return -ENOENT;
	}

	/* Check to make sure it is a normal block device found in /dev */
	if (strncmp(block_dev, "/dev/", 5) != SUCCESS) {
		msg = "Error. Source device does not seem to be a normal block device listed in the /dev directory path.";
		if (return_message) {
			sprintf(return_message, "%s", msg);
		} else {
			printf("%s\n", msg);
		}
		return INVALID_VALUE;
	}

	/* Check to make sure that ramdisk/block_dev devices are not in a mapping already */
	while (rc_prof != NULL) {
		if ((strcmp(ramdisk, rc_prof->cache) == SUCCESS) || \
		    (strcmp(block_dev + 5, rc_prof->source) == SUCCESS)) {
			msg = "Error. At least one of your cache/source devices is currently mapped to %s.";
			print_error(msg, return_message, rc_prof->device);
			return INVALID_VALUE;
		}
		rc_prof = rc_prof->next;
	}

	if ((buf = calloc(1,BUFSZ)) == NULL) {
		msg = "%s: malloc: Unable to allocate memory.";
		print_error(msg, return_message, __func__);
		return INVALID_VALUE;
	}

	if ((fp = fopen(ETC_MTAB, "r")) == NULL) {
		msg = ERR_FOPEN;
		print_error(msg, return_message, __func__, ETC_MTAB, strerror(errno));
		if (buf) free(buf);
		return INVALID_VALUE;
	}
	fread(buf, BUFSZ, 1, fp);
	fclose(fp);

	if ((strstr(buf, ramdisk) != NULL)) {
		msg = "%s is currently mounted. Please \"umount\" and retry.";
		print_error(msg, return_message, ramdisk);
		if (buf) free(buf);
		return INVALID_VALUE;
	}
	if ((strstr(buf, block_dev) != NULL)) {
		msg = "%s is currently mounted. Please \"umount\" and retry.";
		print_error(msg, return_message, block_dev);
		if (buf) free(buf);
		return INVALID_VALUE;
	}

	if (buf) free(buf);

	if ((fd = open(block_dev, O_RDONLY)) < SUCCESS) {
		msg = "%s: open: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return -ENOENT;
	}

	if (ioctl(fd, BLKGETSIZE, &block_dev_sz) == INVALID_VALUE) {
		msg = "%s: ioctl: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		close(fd);
		return -EIO;
	}
	close(fd);

	sprintf(name, "/dev/%s", ramdisk);
	if ((fd = open(name, O_RDONLY)) < SUCCESS) {
		msg = "%s: open: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return -ENOENT;
	}

	if (ioctl(fd, BLKGETSIZE, &ramdisk_sz) == INVALID_VALUE) {
		msg = "%s: ioctl: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		close(fd);
		return -EIO;
	}
	close(fd);
	memset(name, 0x0, sizeof(name));

	dup = strdup(block_dev);
	token = strtok(dup, "/");
	while (token != NULL) {
		sprintf(str, "%s", token);
		token = strtok(NULL, "/");
	}
	if (cache_mode == WRITETHROUGH)
		sprintf(name, "rc-wt_%s", str);
	else if (cache_mode == WRITEBACK)    /* very dangerous mode */
		sprintf(name, "rc-wb_%s", str);
	else
		sprintf(name, "rc-wa_%s", str);

	memset(table, 0x0, BUFSZ);
	if (cache_mode == WRITEBACK)    /* very dangerous mode */
		sprintf(table, "0 %llu writecache s %s /dev/%s 4096 0",
				block_dev_sz, block_dev, ramdisk);
	else {
		sprintf(table, "0 %llu rapiddisk-cache %s /dev/%s %llu %d",
				block_dev_sz, block_dev, ramdisk, ramdisk_sz, cache_mode);
	}

	rc = dm_create_mapping(name, table);

	if (rc == SUCCESS) {
		msg = "Command to map %s with %s and %s has been sent.";
		print_error(msg, return_message, name, ramdisk, block_dev);
	} else {
		msg = "Error. Unable to create map. Please verify all input values are correct.";
		print_error("%s", return_message, msg);
	}
	if (dup) free(dup);
	return rc;
}

/**
 * It resizes the device
 *
 * @param prof This is a pointer to the linked list of RD_PROFILE structures.
 * @param string The device name
 * @param size The size of the device in Mbytes
 * @param return_message This is a pointer to a buffer that will contain the return message.
 *
 * @return The return value is SUCCESS upon result
 */
int mem_device_resize(struct RD_PROFILE *prof, char *string, unsigned long long size, char *return_message)
{
	int rc = INVALID_VALUE, fd;
	FILE *fp = NULL;
	char file[NAMELEN] = {0};
	unsigned long long max_sectors = 0, rd_size = 0;
	char *msg;

	/* echo "rapiddisk resize 1 131072 " > /sys/kernel/rapiddisk/mgmt */
	while (prof != NULL) {
		if (strcmp(string, prof->device) == SUCCESS) {
			rd_size = prof->size;
			rc = SUCCESS;
		}
		prof = prof->next;
	}
	if (rc != SUCCESS) {
		msg = "Error. Device %s does not exist";
		print_error(msg, return_message, string);
		return -ENOENT;
	}

	sprintf(file, "/dev/%s", string);

	if ((fd = open(file, O_WRONLY)) < SUCCESS) {
		msg = "%s: open: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return -ENOENT;
	}

	/**
	 * TODO: check why max_sectors is 0 in some cases
	 */
	if ((ioctl(fd, IOCTL_RD_GET_STATS, &max_sectors)) == INVALID_VALUE) {
		msg = "%s: ioctl: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		close(fd);
		return -EIO;
	}

	close(fd);

	if ((((size * 1024 * 1024) / BYTES_PER_BLOCK) <= (max_sectors)) || ((size * 1024) == (rd_size / 1024))) {
		msg = "Error. Please specify a size larger than %llu Mbytes";
		print_error(msg, return_message, (((max_sectors * BYTES_PER_BLOCK) / 1024) / 1024));
		return -EINVAL;
	}

	/* This is where we begin to detach the block device */
	if ((fp = fopen(SYS_RDSK, "w")) == NULL) {
		msg = "%s: fopen: %s: %s";
		print_error(msg, return_message, __func__, SYS_RDSK, strerror(errno));
		return -ENOENT;
	}

	if (fprintf(fp, "rapiddisk resize %s %llu\n", string + 2, (size * 1024 * 1024)) < 0) {
		msg = "%s: fprintf: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		fclose(fp);
		return -EIO;
	}
	fclose(fp);
	print_error("Resized device %s to %llu Mbytes", return_message, string, size);
	return SUCCESS;
}

/**
 * It attaches a new device to the kernel
 *
 * @param prof This is a pointer to the linked list of RD_PROFILE structures.
 * @param size size of the device in Mbytes
 * @param return_message This is a pointer to a buffer that will contain the error message if the function fails.
 *
 * @return The return value is SUCCESS upon result
 */
int mem_device_attach(struct RD_PROFILE *prof, unsigned long long size, char *return_message)
{
	int dsk;
	FILE *fp = NULL;
	char string[BUFSZ] = {0}, name[16] = {0};
	char *msg;

	/* echo "rapiddisk attach 65536" > /sys/kernel/rapiddisk/mgmt <- in bytes */
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
		msg = "%s: fopen: %s: %s";
		print_error(msg, return_message, __func__, SYS_RDSK, strerror(errno));
		return -ENOENT;
	}
	if (fprintf(fp, "rapiddisk attach %d %llu\n", dsk,
				(size * 1024 * 1024)) < 0) {
		msg = "%s: fprintf: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		fclose(fp);
		return -EIO;
	}

	fclose(fp);
	print_error("Attached device rd%d of size %llu Mbytes", return_message, dsk, size);
	return SUCCESS;
}

/**
 * It detaches a RapidDisk device from the system
 *
 * @param rd_prof This is a pointer to the first element in the linked list of RapidDisk devices.
 * @param rc_prof This is a pointer to the first element in the linked list of RapidCache devices.
 * @param string The device name to be detached.
 * @param return_message This is a pointer to a char array that will be filled with the return message.
 *
 * @return The return value is the return code of the function.
 */
int mem_device_detach(struct RD_PROFILE *rd_prof, RC_PROFILE *rc_prof, char *string, char *return_message)
{
	int rc = INVALID_VALUE;
	FILE *fp = NULL;
	char *buf = NULL;
	char *msg;

	/* echo "rapiddisk detach 1" > /sys/kernel/rapiddisk/mgmt */
	while (rd_prof != NULL) {
		if (strcmp(string, rd_prof->device) == SUCCESS)
			rc = SUCCESS;
		rd_prof = rd_prof->next;
	}
	if (rc != SUCCESS) {
		msg = "Error. Device %s does not exist";
		print_error(msg, return_message, string);
		return INVALID_VALUE;
	}
	/* Check to make sure RapidDisk device isn't in a mapping */
	while (rc_prof != NULL) {
		if (strcmp(string, rc_prof->cache) == SUCCESS) {
			msg = "Error. Unable to remove %s. This RapidDisk device is currently"
						" mapped as a cache drive to %s.";
			print_error(msg, return_message, string, rc_prof->device);
			return INVALID_VALUE;
		}
		rc_prof = rc_prof->next;
	}

	if ((buf = (char *)calloc(1, BUFSZ)) == NULL) {
		msg = "%s: malloc: Unable to allocate memory.";
		print_error(msg, return_message, __func__);
		return INVALID_VALUE;
	}

	/* Here we are starting to check to see if the device is mounted */
	if ((fp = fopen(ETC_MTAB, "r")) == NULL) {
		msg = "%s: fopen: %s: %s";
		print_error(msg, return_message, __func__, ETC_MTAB, strerror(errno));
		if (buf) free(buf);
		return -ENOENT;
	}
	fread(buf, BUFSZ, 1, fp);
	fclose(fp);
	if ((strstr(buf, string) != NULL)) {
		msg = "%s is currently mounted. Please \"umount\" and retry.";
		print_error(msg, return_message, string);
		if (buf) free(buf);
		return INVALID_VALUE;
	}

	/* This is where we begin to detach the block device */
	if ((fp = fopen(SYS_RDSK, "w")) == NULL) {
		msg = "%s: fopen: %s: %s";
		print_error(msg, return_message, __func__, SYS_RDSK, strerror(errno));
		if (buf) free(buf);
		return -ENOENT;
	}

	if (fprintf(fp, "rapiddisk detach %s\n", string + 2) < 0) {
		msg = "%s: fprintf: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		if (buf) free(buf);
		fclose(fp);
		return -EIO;
	}

	fclose(fp);
	print_error("Detached device %s", return_message, string);
	if (buf) free(buf);

	return SUCCESS;
}

/**
 * It takes a device name and a boolean value, and sets the device to read-only or read-write
 *
 * @param rd_prof This is a pointer to the first element of the linked list of devices.
 * @param string The device name to lock/unlock
 * @param lock TRUE or FALSE
 * @param return_message This is a pointer to a buffer that will contain the error message if the function fails.
 *
 * @return The return value is the return code of the function.
 */
int mem_device_lock(struct RD_PROFILE *rd_prof, char *string, bool lock, char *return_message)
{
	int fd, rc = INVALID_VALUE, state = (unsigned char)lock;
	char file[NAMELEN] = {0};
	char *msg;

	while (rd_prof != NULL) {
		if (strcmp(string, rd_prof->device) == SUCCESS) {
			rc = SUCCESS;
		}
		rd_prof = rd_prof->next;
	}

	if (rc != SUCCESS) {
		msg = "Error. Device %s does not exist";
		print_error(msg, return_message, string);
		return -ENOENT;
	}

	sprintf(file, "/dev/%s", string);

	if ((fd = open(file, O_WRONLY)) < SUCCESS) {
		msg = "%s: open: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return -ENOENT;
	}

	if ((ioctl(fd, BLKROSET, &state)) == INVALID_VALUE) {
		msg = "%s: ioctl: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		close(fd);
		return -EIO;
	}

	close(fd);
	print_error("Device %s is now set to %s", return_message, string, ((lock == TRUE) ? "read-only" : "read-write"));

	return SUCCESS;
}

/**
 * This function is used to unmap a cache device
 *
 * @param prof the head of the profile list
 * @param string the name of the cache device to be unmapped
 * @param return_message This is a pointer to a buffer that will be filled with the return message.
 *
 * @return The return code of the system call.
 */
int cache_device_unmap(struct RC_PROFILE *prof, char *string, char *return_message)
{
	int rc = INVALID_VALUE;
	FILE *fp = NULL;
	char *buf = NULL;
	char *msg;

	/* dmsetup remove rc-wt_sdb */
	while (prof != NULL) {
		if (strcmp(string, prof->device) == SUCCESS)
			rc = SUCCESS;
		prof = prof->next;
	}
	if (rc != SUCCESS) {
		msg = "Error. Cache target %s does not exist";
		print_error(msg, return_message, string);
		return -ENOENT;
	}

	if ((buf = calloc(1,BUFSZ)) == NULL) {
		msg = "%s: malloc: Unable to allocate memory.";
		print_error(msg, return_message, __func__);
		return -ENOMEM;
	}

	/* Here we are starting to check to see if the device is mounted */
	if ((fp = fopen(ETC_MTAB, "r")) == NULL) {
		msg = "%s: fopen: %s: %s";
		print_error(msg, return_message,  __func__, ETC_MTAB, strerror(errno));
		if (buf) free(buf);
		return -ENOENT;
	}
	fread(buf, BUFSZ, 1, fp);
	fclose(fp);
	if ((strstr(buf, string) != NULL)) {
		msg = "%s is currently mounted. Please \"umount\" and retry.";
		print_error(msg, return_message, string);
		if (buf) free(buf);
		return -EBUSY;
	}
	if (buf) free(buf);

	/* if the mapping is a write-back one, flush before remove */
	if (strstr(string, "rc-wb") != NULL) {
		if ((rc = dm_flush_device(string) != SUCCESS)) {
			msg = "Unable to flush dirty cache data to %s";
			print_error(msg, return_message, string);
			return rc;
		}
	}

	if ((rc = dm_remove_mapping(string)) == SUCCESS) {
		msg = "Command to unmap %s has been sent.";
	} else {
		msg = "Error. Unable to unmap %s. Please check to make sure nothing is wrong.";
	}
	print_error(msg, return_message, string);
	return rc;
}

/**
 * It flushes all data from a RapidDisk device
 *
 * @param rd_prof This is a pointer to the RapidDisk profile list.
 * @param rc_prof This is a pointer to the RC_PROFILE structure list that contains the RapidCache mapping information.
 * @param string The device name
 * @param return_message This is a pointer to a buffer that will contain the return message.
 *
 * @return The return value is the return code of the function.
 */
int mem_device_flush(struct RD_PROFILE *rd_prof, RC_PROFILE *rc_prof, char *string, char *return_message)
{
	int fd, rc = INVALID_VALUE;
	char file[NAMELEN] = {0}, *buf = NULL;
	FILE *fp = NULL;
	char *msg;

	while (rd_prof != NULL) {
		if (strcmp(string, rd_prof->device) == SUCCESS)
			rc = SUCCESS;
		rd_prof = rd_prof->next;
	}
	if (rc != SUCCESS) {
		msg = "Error. Device %s does not exist";
		print_error(msg, return_message, string);
		return -ENOENT;
	}
	/* Check to make sure RapidDisk device isn't in a mapping */
	while (rc_prof != NULL) {
		if (strcmp(string, rc_prof->cache) == SUCCESS) {
			msg = "Error. Unable to remove %s. This RapidDisk device is currently mapped as a cache drive to %s";
			print_error(msg, return_message, string, rc_prof->device);
			return -EBUSY;
		}
		rc_prof = rc_prof->next;
	}

	if ((buf = calloc(1, BUFSZ)) == NULL) {
		msg = "%s: malloc: Unable to allocate memory";
		print_error(msg, return_message, __func__);
		return -ENOMEM;
	}

	/* Here we are starting to check to see if the device is mounted */
	if ((fp = fopen(ETC_MTAB, "r")) == NULL) {
		msg = "%s: fopen: %s: %s";
		print_error(msg, return_message, __func__, ETC_MTAB, strerror(errno));
		if (buf) free(buf);
		return -ENOENT;
	}
	fread(buf, BUFSZ, 1, fp);
	fclose(fp);
	if ((strstr(buf, string) != NULL)) {
		msg = "%s is currently mounted. Please \"umount\" and retry";
		print_error(msg, return_message, string);
		if (buf) free(buf);
		return -EBUSY;
	}
	if (buf) free(buf);
	sprintf(file, "/dev/%s", string);

	if ((fd = open(file, O_WRONLY)) < SUCCESS) {
		msg = "%s: open: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return -ENOENT;
	}

	if (ioctl(fd, IOCTL_RD_BLKFLSBUF, 0) == INVALID_VALUE) {
		msg = "%s: ioctl: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		close(fd);
		return -EIO;
	}
	close(fd);
	print_error("Flushed all data from device %s", return_message, string);

	return SUCCESS;
}

/**
 *  These functions are used by rapiddisk only
 */
#ifndef SERVER

/**
 * It prints out the list of RapidDisk devices and RapidDisk-Cache mappings
 *
 * @param rd_prof This is a pointer to the first element in the linked list of RD_PROFILE structures.
 * @param rc_prof This is a pointer to the first element in the linked list of RC_PROFILE structures.
 */
int mem_device_list(struct RD_PROFILE *rd_prof, struct RC_PROFILE *rc_prof)
{
	int num = 1;
	char status[0xf] = {0};

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

/**
 * It prints out the statistics of the wb cache device
 *
 * @param rc_prof the RC_PROFILE structure that contains the cache device name and the cache device size.
 * @param cache the name of the cache device
 *
 * @return The function status result
 */
int cache_wb_device_stat(struct RC_PROFILE *rc_prof, char *cache)
{

	int rc = INVALID_VALUE;
	char *msg;

	if (rc_prof == NULL) {
		msg = "No RapidDisk-Cache Mappings exist.";
		print_error("%s", NULL, msg);
		return rc;
	}
	while (rc_prof != NULL) {
		if (strcmp(cache, rc_prof->device) == SUCCESS)
			rc = SUCCESS;
		rc_prof = rc_prof->next;
	}
	if (rc != SUCCESS) {
		msg = "Error. Cache target %s does not exist";
		print_error(msg, NULL, cache);
		return -ENOENT;
	}
	struct WC_STATS *wc_stats = dm_get_status(cache, WRITEBACK);
	int i = 2;
	unsigned int *pvalue = NULL;
	for (; i < (((sizeof(WC_STATS_OFFSETS) / sizeof(size_t)) - 1)); i++) {
		pvalue = (unsigned int *) (((char *) wc_stats) + WC_STATS_OFFSETS[i]);
		printf("%d ", *pvalue);
	}
	pvalue = (unsigned int *) (((char *) wc_stats) + WC_STATS_OFFSETS[i]);
	printf("%d\n", *pvalue);
	free(wc_stats);
	return SUCCESS;
}

/**
 * It prints the statistics of a cache device
 *
 * @param rc_prof This is the list of RapidDisk-Cache mappings.
 * @param cache The cache device name.
 *
 * @return The function status result
 */
int cache_device_stat(struct RC_PROFILE *rc_prof, char *cache)
{
	int rc = INVALID_VALUE;
	char *msg;

	if (rc_prof == NULL) {
		msg = "No RapidDisk-Cache Mappings exist.";
		print_error("%s", NULL, msg);
		return rc;
	}
	while (rc_prof != NULL) {
		if (strcmp(cache, rc_prof->device) == SUCCESS)
			rc = SUCCESS;
		rc_prof = rc_prof->next;
	}
	if (rc != SUCCESS) {
		msg = "Error. Cache target %s does not exist";
		print_error(msg, NULL, cache);
		return -ENOENT;
	}
	struct RC_STATS *rc_stats = dm_get_status(cache, WRITETHROUGH);
	int i = 1;
	unsigned int *pvalue = NULL;
	for (; i < (((sizeof(RC_STATS_OFFSETS) / sizeof(size_t)) - 1)); i++) {
		pvalue = (unsigned int *) (((char *) rc_stats) + RC_STATS_OFFSETS[i]);
		printf("%d ", *pvalue);
	}
	pvalue = (unsigned int *) (((char *) rc_stats) + RC_STATS_OFFSETS[i]);
	printf("%d\n", *pvalue);
	free(rc_stats);
	return SUCCESS;
}

/**
 * This function is used to get the statistics of a cache device
 *
 * @param rc_prof This is the list of cache mappings.
 * @param cache The name of the cache device.
 * @param rc_stats This is a pointer to a pointer to a RC_STATS structure.  This is the structure that will be filled with
 * the statistics.
 *
 * @return The function status result
 */
int cache_device_stat_json(struct RC_PROFILE *rc_prof, char *cache, RC_STATS **rc_stats)
{
	int rc = INVALID_VALUE;
	char *msg;
	char status_message[NAMELEN] = {0};

	if (rc_prof == NULL) {
		msg = "No RapidDisk-Cache Mappings exist.";
		print_message(rc, msg, TRUE);
		return rc;
	}
	while (rc_prof != NULL) {
		if (strcmp(cache, rc_prof->device) == SUCCESS)
			rc = SUCCESS;
		rc_prof = rc_prof->next;
	}
	if (rc != SUCCESS) {
		msg = "Error. Cache target %s does not exist";
		print_error(msg, status_message, cache);
		print_message(rc, status_message, TRUE);
		return -ENOENT;
	}
	*rc_stats = dm_get_status(cache, WRITETHROUGH);

	return SUCCESS;
}

/**
 * This function is used to get the writeback cache statistics for a given cache target
 *
 * @param rc_prof This is the linked list of RapidDisk cache mappings.
 * @param cache The cache device name
 * @param wc_stats This is a pointer to a pointer to a WC_STATS structure.  This is the structure that will be filled with
 * the statistics.
 *
 * @return The function status result
 */
int cache_wb_device_stat_json(struct RC_PROFILE *rc_prof, char *cache, WC_STATS **wc_stats)
{
	int rc = INVALID_VALUE;
	char *msg;
	char status_message[NAMELEN] = {0};

	if (rc_prof == NULL) {
		msg = "No RapidDisk-Cache Mappings exist.";
		print_message(rc, msg, TRUE);
		return rc;
	}
	while (rc_prof != NULL) {
		if (strcmp(cache, rc_prof->device) == SUCCESS)
			rc = SUCCESS;
		rc_prof = rc_prof->next;
	}
	if (rc != SUCCESS) {
		msg = "Error. Cache target %s does not exist";
		print_error(msg, status_message, cache);
		print_message(rc, status_message, TRUE);
		return -ENOENT;
	}
	*wc_stats = dm_get_status(cache, WRITEBACK);

	return SUCCESS;
}
#endif
