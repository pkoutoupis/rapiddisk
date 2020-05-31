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

unsigned char *proc_mod   = "/proc/modules";

int parse_input(int, char **);
struct RD_PROFILE *search_targets(void);
struct RC_PROFILE *search_cache(void);
int check_uid(int);
void online_menu(char *);
int list_devices(struct RD_PROFILE *, struct RC_PROFILE *);
int list_devices_json(struct RD_PROFILE *, struct RC_PROFILE *);
int short_list_devices(struct RD_PROFILE *, struct RC_PROFILE *);
int detach_device(struct RD_PROFILE *, struct RC_PROFILE *, unsigned char *);
int attach_device(struct RD_PROFILE *, unsigned long);
int resize_device(struct RD_PROFILE *, unsigned char *, unsigned long);
int cache_map(struct RD_PROFILE *, struct RC_PROFILE *, unsigned char *, unsigned char *, int);
int cache_unmap(struct RC_PROFILE *, unsigned char *);
int stat_cache_mapping(struct RC_PROFILE *, unsigned char *);
int rdsk_flush(struct RD_PROFILE *, RC_PROFILE *, unsigned char *);

void online_menu(char *string)
{
	printf("%s is an administration tool to manage the RapidDisk RAM disk devices and\n"
	       "\tRapidDisk-Cache mappings.\n\n", string);
	printf("Usage: %s [ -h | -v ] function [ parameters: unit | size | start & end | "
	       "src & dest |\n\t\tcache & source | mode ]\n\n", string);
	printf("Description:\n\t%s is a RapidDisk module management tool. The tool allows the\n"
	       "\tuser to list all attached RapidDisk devices, with the ability to dynamically\n"
	       "\tattach a new RapidDisk block devices and detach existing ones. It also is\n"
	       "\tcapable of mapping and unmapping a RapidDisk volume to any block device via\n"
	       "\tthe RapidDisk-Cache kernel module.\n\n", string);
	printf("Functions:\n"
	       "\t--attach\t\tAttach RAM disk device.\n"
	       "\t--detach\t\tDetach RAM disk device.\n"
	       "\t--list\t\t\tList all attached RAM disk devices.\n"
	       "\t--short-list\t\tList all attached RAM disk devices in script friendly format.\n"
#if !defined NO_JANSSON
	       "\t--list-json\t\tList all attached RAM disk devices in JSON format.\n"
#endif
	       "\t--flush\t\t\tErase all data to a specified RapidDisk device "
	       "\033[31;1m(dangerous)\033[0m.\n"
	       "\t--resize\t\tDynamically grow the size of an existing RapidDisk device.\n\n"
	       "\t--cache-map\t\tMap an RapidDisk device as a caching node to another block device.\n"
	       "\t--cache-unmap\t\tMap an RapidDisk device as a caching node to anotther block device.\n"
	       "\t--stat-cache\t\tObtain RapidDisk-Cache Mappings statistics.\n\n");
	printf("Parameters:\n"
	       "\t[size]\t\t\tSpecify desired size of attaching RAM disk device in MBytes.\n"
	       "\t[unit]\t\t\tSpecify unit number of RAM disk device to detach.\n"
	       "\t[cache]\t\t\tSpecify RapidDisk node to use as caching volume.\n"
	       "\t[source]\t\tSpecify block device to map cache to.\n"
	       "\t[mode]\t\t\tWrite Through (wt) or Write Around (wa) for cache.\n\n");
	printf("Example Usage:\n\trapiddisk --attach 64\n"
	       "\trapiddisk --detach rd2\n"
	       "\trapiddisk --resize rd2 128\n"
	       "\trapiddisk --cache-map rd1 /dev/sdb\n"
	       "\trapiddisk --cache-map rd1 /dev/sdb wt\n"
	       "\trapiddisk --cache-unmap rc_sdb\n"
	       "\trapiddisk --flush rd2\n\n");
}

int exec_cmdline_arg(int argcin, char *argvin[])
{
	int rc = INVALID_VALUE, mode = WRITETHROUGH;
	struct RD_PROFILE *disk;	/* These are dummy pointers to  */
	struct RC_PROFILE *cache;	/* help create the linked  list */

	printf("%s %s\n%s\n\n", UTIL, VERSION_NUM, COPYRIGHT);
	if ((argcin < 2) || (argcin > 5)) {
		online_menu(argvin[0]);
		return rc;
	}

	if (((strcmp(argvin[1], "-h")) == 0) || ((strcmp(argvin[1],"-H")) == 0) ||
	    ((strcmp(argvin[1], "--help")) == 0)) {
		online_menu(argvin[0]);
		return SUCCESS;
	}
	if (((strcmp(argvin[1], "-v")) == 0) || ((strcmp(argvin[1],"-V")) == 0) ||
	    ((strcmp(argvin[1], "--version")) == 0)) {
		return SUCCESS;
	}

	disk = (struct RD_PROFILE *)search_targets();
	cache = (struct RC_PROFILE *)search_cache();

	if (strcmp(argvin[1], "--list") == 0) {
		if (disk == NULL)
			printf("Unable to locate any RapidDisk devices.\n");
		else
			rc = list_devices(disk, cache);
		goto out;
#if !defined NO_JANSSON
	} else if (strcmp(argvin[1], "--list-json") == 0) {
		rc = list_devices_json(disk, cache);
		goto out;
#endif
	} else if (strcmp(argvin[1], "--short-list") == 0) {
		if (disk == NULL)
			printf("Unable to locate any RapidDisk devices.\n");
		else
			rc = short_list_devices(disk, cache);
		goto out;
	}

	if (strcmp(argvin[1], "--attach") == 0) {
		if (argcin != 3) goto invalid_out;
		rc = attach_device(disk, strtoul(argvin[2], (char **)NULL, 10));
	} else if (strcmp(argvin[1], "--detach") == 0) {
		if (argcin != 3) goto invalid_out;
		if (disk == NULL)
			printf("Unable to locate any RapidDisk devices.\n");
		else
			rc = detach_device(disk, cache, argvin[2]);
	} else if (strcmp(argvin[1], "--resize") == 0) {
		if (argcin != 4) goto invalid_out;
		if (disk == NULL)
			printf("Unable to locate any RapidDisk devices.\n");
		else
			rc = resize_device(disk, argvin[2], strtoul(argvin[3], (char **)NULL, 10));
	} else if (strcmp(argvin[1], "--cache-map") == 0) {
		if ((argcin < 4) || (argcin > 5)) goto invalid_out;
		if (argcin == 5)
			if (strcmp(argvin[4], "wa") == 0)
				mode = WRITEAROUND;
		rc = cache_map(disk, cache, argvin[2], argvin[3], mode);
	} else if (strcmp(argvin[1], "--cache-unmap") == 0) {
		if (argcin != 3) goto invalid_out;
		rc = cache_unmap(cache, argvin[2]);
	} else if (strcmp(argvin[1], "--flush") == 0) {
		if (argcin != 3) goto invalid_out;
		rc = rdsk_flush(disk, cache, argvin[2]);
	} else if (strcmp(argvin[1], "--stat-cache") == 0) {
		if (argcin != 3) goto invalid_out;
		rc = stat_cache_mapping(cache, argvin[2]);
	} else {
		goto invalid_out;
	}

out:
	return rc;
invalid_out:
	printf("Error. Invalid argument(s) entered.\n");
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

	if ((fp = fopen(proc_mod, "r")) == NULL) {
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
