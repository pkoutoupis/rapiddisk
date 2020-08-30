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
 ** @filename: main.c
 ** @description: This is the main file for the RapidDisk userland tool.
 **
 ** @date: 14Oct10, petros@petroskoutoupis.com
 ********************************************************************************/

#include "common.h"
#include "cli.h"

struct RD_PROFILE *search_targets(void);
struct RC_PROFILE *search_cache(void);
int list_devices(struct RD_PROFILE *, struct RC_PROFILE *);
int short_list_devices(struct RD_PROFILE *, struct RC_PROFILE *);
int detach_device(struct RD_PROFILE *, struct RC_PROFILE *, unsigned char *);
int attach_device(struct RD_PROFILE *, unsigned long);
int resize_device(struct RD_PROFILE *, unsigned char *, unsigned long);
int cache_map(struct RD_PROFILE *, struct RC_PROFILE *, unsigned char *, unsigned char *, int);
int cache_unmap(struct RC_PROFILE *, unsigned char *);
int stat_cache_mapping(struct RC_PROFILE *, unsigned char *);
int rdsk_flush(struct RD_PROFILE *, RC_PROFILE *, unsigned char *);

void online_menu(unsigned char *string)
{
	printf("%s is an administration tool to manage RapidDisk RAM disk devices and\n"
	       "\tRapidDisk-Cache mappings.\n\n", string);
	printf("Usage: %s [ -h | -v ] function [ parameters ]\n\n", string);
	printf("Description:\n\t%s is a RapidDisk module management tool to manage RapidDisk\n"
	       "\tRAM disk devices. Dynamically create, remove, resize RAM volumes and if\n"
	       "\tdesired, map or unmap them as a cache volume to any block device.\n\n", string);
	printf("Functions:\n"
	       "\t-a\t\tAttach RAM disk device (size in MBytes).\n"
	       "\t-b\t\tBackend block device absolute path (for cache mapping).\n"
	       "\t-c\t\tInput capacity for size or resize of RAM disk device (in MBytes).\n"
	       "\t-d\t\tDetach RAM disk device.\n"
	       "\t-f\t\tErase all data to a specified RapidDisk device \033[31;1m(dangerous)\033[0m.\n"
	       "\t-h\t\tDisplay the help menu.\n"
	       "\t-j\t\tEnable JSON formatted output.\n"
	       "\t-l\t\tList all attached RAM disk devices.\n"
	       "\t-m\t\tMap an RapidDisk device as a caching node to another block device.\n"
	       "\t-p\t\tDefine cache policy (default: write-through).\n"
	       "\t-r\t\tDynamically grow the size of an existing RapidDisk device.\n\n"
	       "\t-s\t\tObtain RapidDisk-Cache Mappings statistics.\n"
	       "\t-u\t\tUnmap a RapidDisk device from another block device.\n"
	       "\t-v\t\tEnable verbosity (for debugging purposes).\n"
	       "\t-V\t\tDisplay the utility version string.\n\n");
        printf("Example Usage:\n\trapiddisk -a 64\n"
	       "\trapiddisk -d rd2\n"
	       "\trapiddisk -r rd2 -c 128\n"
	       "\trapiddisk -m rd1 -b /dev/sdb\n"
	       "\trapiddisk -m rd1 -b /dev/sdb -p wt\n"
	       "\trapiddisk -u rc_sdb\n"
	       "\trapiddisk -f rd2\n\n");
}

int exec_cmdline_arg(int argcin, char *argvin[])
{
	int rc = INVALID_VALUE, mode = WRITETHROUGH, action = ACTION_NONE, i;
	unsigned long size = 0;
	bool json_flag = FALSE, verbose = FALSE;
	unsigned char device[NAMELEN] = {0}, backing[NAMELEN] = {0};
	struct RD_PROFILE *disk;	/* These are dummy pointers to  */
	struct RC_PROFILE *cache;	/* help create the linked  list */

	printf("%s %s\n%s\n\n", PROCESS, VERSION_NUM, COPYRIGHT);

	while ((i = getopt(argcin, argvin, "a:b:c:d:f:hjlm:p:r:s:u:vV")) != INVALID_VALUE) {
		switch (i) {
		case 'h':
			online_menu(argvin[0]);
			return SUCCESS;
			break;
		case 'a':
			action = ACTION_ATTACH;
			size = strtoul(optarg, (char **)NULL, 10);
			break;
		case 'b':
			sprintf(backing, "%s", optarg);
			break;
		case 'c':
			size = strtoul(optarg, (char **)NULL, 10);
			break;
		case 'd':
			action = ACTION_DETACH;
			sprintf(device, "%s", optarg);
			break;
		case 'f':
			action = ACTION_FLUSH;
			sprintf(device, "%s", optarg);
			break;
		case 'j':
			json_flag = TRUE;
			break;
		case 'l':
			action = ACTION_LIST;
			break;
		case 'm':
			action = ACTION_CACHE_MAP;
			sprintf(device, "%s", optarg);
			break;
		case 'p':
			if (strcmp(optarg, "wa") == 0)
				mode = WRITEAROUND;
			break;
		case 'r':
			action = ACTION_RESIZE;
			sprintf(device, "%s", optarg);
			break;
		case 's':
			action = ACTION_CACHE_STATS;
			sprintf(device, "%s", optarg);
			break;
		case 'u':
			action = ACTION_CACHE_UNMAP;
			sprintf(device, "%s", optarg);
			break;
		case 'v':
			verbose = TRUE;
			break;
		case 'V':
			return SUCCESS;
			break;
		case '?':
			online_menu(argvin[0]);
			return INVALID_VALUE;
			break;
                }
	}

	disk = (struct RD_PROFILE *)search_targets();
	cache = (struct RC_PROFILE *)search_cache();

	switch(action) {
	case ACTION_ATTACH:
		if (size <= 0)
			goto exec_cmdline_arg_out;

		rc = attach_device(disk, size);
		break;
	case ACTION_DETACH:
		if (disk == NULL)
			printf("Unable to locate any RapidDisk devices.\n");
		else {
			if (strlen(device) <= 0)
				goto exec_cmdline_arg_out;

			rc = detach_device(disk, cache, device);
		}
		break;
	case ACTION_FLUSH:
		rc = rdsk_flush(disk, cache, device);
		break;
	case ACTION_LIST:
		if (disk == NULL)
			printf("Unable to locate any RapidDisk devices.\n");
		else {
			if (strlen(device) <= 0)
				goto exec_cmdline_arg_out;

			//TODO: Add JSON flag input
			rc = list_devices(disk, cache);
		}
		break;
	case ACTION_CACHE_MAP:
		if ((strlen(device) <= 0) || (strlen(backing) <= 0))
			goto exec_cmdline_arg_out;

		rc = cache_map(disk, cache, device, backing, mode);
		break;
	case ACTION_RESIZE:
		if (disk == NULL)
			printf("Unable to locate any RapidDisk devices.\n");
		else
			if (size <= 0)
				goto exec_cmdline_arg_out;
			if (strlen(device) <= 0)
				goto exec_cmdline_arg_out;

                        rc = resize_device(disk, device, size);
		break;
	case ACTION_CACHE_STATS:
		if (strlen(device) <= 0)
			goto exec_cmdline_arg_out;
		rc = stat_cache_mapping(cache, device);
		break;
	case ACTION_CACHE_UNMAP:
		 rc = cache_unmap(cache, device);
		break;
	case ACTION_NONE:
		online_menu(argvin[0]);
		break;
	}

	return rc;

exec_cmdline_arg_out:
	printf("Error. Invalid argument(s) or values entered.\n");
	return -EINVAL;
}

int main(int argc, char *argv[])
{
	int rc = INVALID_VALUE;
	FILE *fp;
	unsigned char string[BUFSZ];

	if (getuid() != 0) {
		printf("\nYou must be root or contain sudo permissions to initiate this\n\n");
		return -EACCES;
	}

	if (access(SYS_RDSK, F_OK) == -1) {
		printf("Please ensure that the RapidDisk module is loaded and retry.\n");
		return -EPERM;
	}

	if ((fp = fopen(PROC_MODULES, "r")) == NULL) {
		printf("%s: fopen: %s\n", __func__, strerror(errno));
		return -errno;
	}
	fread(string, BUFSZ, 1, fp);
	fclose(fp);
	if (strstr(string, "rapiddisk_cache") == NULL) {
		printf("Please ensure that the RapidDisk-Cache module "
		       "are loaded and retry.\n");
		return -EPERM;
	}

	rc = exec_cmdline_arg(argc, argv);
	return rc;
}
