/*********************************************************************************
 ** Copyright © 2011 - 2018 Petros Koutoupis
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
int archive_rd_volume(struct RD_PROFILE *, unsigned char *, unsigned char *);
int restore_rd_volume(struct RD_PROFILE *, unsigned char *, unsigned char *);
#if !defined NO_CRYPT
int format_crypt(unsigned char *);
int activate_crypt(unsigned char *);
int deactivate_crypt(unsigned char *);
#endif

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
	       "\t--archive\t\tUsing zlib, archive an RapidDisk device to a data file.\n"
	       "\t--restore\t\tUsing zlib, restore an RapidDisk device from an archived data file.\n\n"
	       "\t--cache-map\t\tMap an RapidDisk device as a caching node to another block device.\n"
	       "\t--cache-unmap\t\tMap an RapidDisk device as a caching node to anotther block device.\n"
	       "\t--stat-cache\t\tObtain RapidDisk-Cache Mappings statistics.\n"
#if !defined NO_CRYPT
	       "\t--enable-crypt\t\tInitialize a storage volume for data encryption.\n"
	       "\t--activate-crypt\tActivate an encryption volume.\n"
	       "\t--deactivate-crypt\tDeactivate an encryption volume.\n\n");
#else
	       "\n");
#endif
	printf("Parameters:\n"
	       "\t[size]\t\t\tSpecify desired size of attaching RAM disk device in MBytes.\n"
	       "\t[unit]\t\t\tSpecify unit number of RAM disk device to detach.\n"
	       "\t[src]\t\t\tSource path for archive/restore options.\n"
	       "\t[dest]\t\t\tDestination path for arcive/restore options.\n"
	       "\t[cache]\t\t\tSpecify RapidDisk node to use as caching volume.\n"
	       "\t[source]\t\tSpecify block device to map cache to.\n"
	       "\t[mode]\t\t\tWrite Through (wt) or Write Around (wa) for cache.\n\n");
	printf("Example Usage:\n\trapiddisk --attach 64\n"
	       "\trapiddisk --detach rd2\n"
	       "\trapiddisk --resize rd2 128\n"
	       "\trapiddisk --archive rd0 rd-052911.dat\n"
	       "\trapiddisk --restore rd-052911.dat rd0\n"
	       "\trapiddisk --cache-map rd1 /dev/sdb\n"
	       "\trapiddisk --cache-map rd1 /dev/sdb wt\n"
	       "\trapiddisk --cache-unmap rc_sdb\n"
	       "\trapiddisk --flush rd2\n"
#if !defined NO_CRYPT
	       "\trapiddisk --enable-crypt /dev/rd0\n"
	       "\trapiddisk --activate-crypt /dev/rd0\n"
	       "\trapiddisk --deactivate-crypt /dev/mapper/crypt-rd0\n\n");
#else
	       "\n");
#endif
}

int parse_input(int argcin, char *argvin[])
{
	int err = 0, mode = WRITETHROUGH;
	struct RD_PROFILE *rd;	 /* These are dummy pointers to  */
	struct RC_PROFILE *rc;	 /* help create the linked  list */

	printf("%s %s\n%s\n\n", UTIL, VERSION_NUM, COPYRIGHT);
	if ((argcin < 2) || (argcin > 5)) {
		online_menu(argvin[0]);
		return err;
	}

	if (((strcmp(argvin[1], "-h")) == 0) || ((strcmp(argvin[1],"-H")) == 0) ||
	    ((strcmp(argvin[1], "--help")) == 0)) {
		online_menu(argvin[0]);
		return err;
	}
	if (((strcmp(argvin[1], "-v")) == 0) || ((strcmp(argvin[1],"-V")) == 0) ||
	    ((strcmp(argvin[1], "--version")) == 0)) {
		return err;
	}

	rd = (struct RD_PROFILE *)search_targets();
	rc = (struct RC_PROFILE *)search_cache();

	if (strcmp(argvin[1], "--list") == 0) {
		if (rd == NULL)
			printf("Unable to locate any RapidDisk devices.\n");
		else
			err = list_devices(rd, rc);
		goto out;
#if !defined NO_JANSSON
	} else if (strcmp(argvin[1], "--list-json") == 0) {
		err = list_devices_json(rd, rc);
		goto out;
#endif
	} else if (strcmp(argvin[1], "--short-list") == 0) {
		if (rd == NULL)
			printf("Unable to locate any RapidDisk devices.\n");
		else
			err = short_list_devices(rd, rc);
		goto out;
	}

	if (strcmp(argvin[1], "--attach") == 0) {
		if (argcin != 3) goto invalid_out;
		err = attach_device(rd, strtoul(argvin[2], (char **)NULL, 10));
	} else if (strcmp(argvin[1], "--detach") == 0) {
		if (argcin != 3) goto invalid_out;
		if (rd == NULL)
			printf("Unable to locate any RapidDisk devices.\n");
		else
			err = detach_device(rd, rc, argvin[2]);
	} else if (strcmp(argvin[1], "--resize") == 0) {
		if (argcin != 4) goto invalid_out;
		if (rd == NULL)
			printf("Unable to locate any RapidDisk devices.\n");
		else
			err = resize_device(rd, argvin[2], strtoul(argvin[3], (char **)NULL, 10));
	} else if (strcmp(argvin[1], "--archive") == 0) {
		if (argcin != 4) goto invalid_out;
		err = archive_rd_volume(rd, argvin[2], argvin[3]);
	} else if (strcmp(argvin[1], "--restore") == 0) {
		if (argcin != 4) goto invalid_out;
		err = restore_rd_volume(rd, argvin[2], argvin[3]);
	} else if (strcmp(argvin[1], "--cache-map") == 0) {
		if ((argcin < 4) || (argcin > 5)) goto invalid_out;
		if (argcin == 5)
			if (strcmp(argvin[4], "wa") == 0)
				mode = WRITEAROUND;
		err = cache_map(rd, rc, argvin[2], argvin[3], mode);
	} else if (strcmp(argvin[1], "--cache-unmap") == 0) {
		if (argcin != 3) goto invalid_out;
		err = cache_unmap(rc, argvin[2]);
	} else if (strcmp(argvin[1], "--flush") == 0) {
		if (argcin != 3) goto invalid_out;
		err = rdsk_flush(rd, rc, argvin[2]);
	} else if (strcmp(argvin[1], "--stat-cache") == 0) {
		if (argcin != 3) goto invalid_out;
		err = stat_cache_mapping(rc, argvin[2]);
#if !defined NO_CRYPT
	} else if (strcmp(argvin[1], "--enable-crypt") == 0) {
		if (argcin != 3) goto invalid_out;
		err = format_crypt(argvin[2]);
	} else if (strcmp(argvin[1], "--activate-crypt") == 0) {
		if (argcin != 3) goto invalid_out;
		err = activate_crypt(argvin[2]);
	} else if (strcmp(argvin[1], "--deactivate-crypt") == 0) {
		if (argcin != 3) goto invalid_out;
		err = deactivate_crypt(argvin[2]);
#endif
	} else {
		goto invalid_out;
	}

out:
	return err;
invalid_out:
	printf("Error. Invalid argument(s) entered.\n");
	return -EINVAL;
}

int main(int argc, char *argv[])
{
	int err = 0;
	FILE *fp;
	unsigned char string[BUFSZ];

	if (getuid() != 0) {
		printf("\nYou must be root or contain sudo permissions to "
		       "initiate this\napplication. technically you shouldn't "
		       "be running as root anyway.\n\n");
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
#if !defined NO_CRYPT
	if ((strstr(string, "rapiddisk_cache") == NULL) || \
	    (strstr(string, "dm_crypt") == NULL)) {
		printf("Please ensure that the RapidDisk-Cache and dm_crypt modules "
#else
	if (strstr(string, "rapiddisk_cache") == NULL) {
		printf("Please ensure that the RapidDisk-Cache module "
#endif
		       "are loaded and retry.\n");
		return -EPERM;
	}

	err = parse_input(argc, argv);
	return err;
}
