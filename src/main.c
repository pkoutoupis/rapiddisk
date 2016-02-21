/*********************************************************************************
 ** Copyright (c) 2011-2016 Petros Koutoupis
 ** All rights reserved.
 **
 ** @project: rapiddisk
 **
 ** @filename: main.c
 ** @description: This is the main file for the rxadm userland tool.
 **
 ** @date: 14Oct10, petros@petroskoutoupis.com
 ********************************************************************************/

#include "common.h"

unsigned char *proc_mod   = "/proc/modules";

int parse_input(int, char **);
struct RxD_PROFILE *search_targets(void);
struct RxC_PROFILE *search_cache(void);
int check_uid(int);
void online_menu(char *);
int list_devices(struct RxD_PROFILE *, struct RxC_PROFILE *);
int short_list_devices(struct RxD_PROFILE *, struct RxC_PROFILE *);
int detach_device(struct RxD_PROFILE *, struct RxC_PROFILE *, unsigned char *);
int attach_device(struct RxD_PROFILE *, unsigned long);
int resize_device(struct RxD_PROFILE *, unsigned char *, unsigned long);
int rxcache_map(struct RxD_PROFILE *, struct RxC_PROFILE *, unsigned char *, unsigned char *);
int rxcache_unmap(struct RxC_PROFILE *, unsigned char *);
int stat_rxcache_mapping(struct RxC_PROFILE *, unsigned char *);
int rxdsk_flush(struct RxD_PROFILE *, RxC_PROFILE *, unsigned char *);
int archive_rxd_volume(struct RxD_PROFILE *, unsigned char *, unsigned char *);
int restore_rxd_volume(struct RxD_PROFILE *, unsigned char *, unsigned char *);
int format_crypt(unsigned char *);
int activate_crypt(unsigned char *);
int deactivate_crypt(unsigned char *);

void online_menu(char *string)
{
	printf("%s is an administration tool to manage the RapidDisk (rxdsk) RAM disk devices and\n"
		"\tRapidCache (rxcache) mappings.\n\n", string);
	printf("Usage: %s [ -h | -v ] function [parameters: unit | size | start & end | src & dest |\n"
		"\t\tcache & source & block size ]\n\n", string);
	printf("Description:\n\t%s is a RapidDisk (rxdsk) module management tool. The tool allows the\n"
		"\tuser to list all attached rxdsk devices, with the ability to dynamically\n"
		"\tattach a new rxdsk block devices and detach existing ones. It also is capable\n"
		"\tof mapping and unmapping a rxd volume to any block device via the RapidCache\n"
		"\t(rxcache) kernel module.\n\n", string);
	printf("Function:\n"
		"\t--attach\t\tAttach RAM disk device.\n"
		"\t--detach\t\tDetach RAM disk device.\n"
		"\t--list\t\t\tList all attached RAM disk devices.\n"
		"\t--short-list\t\tList all attached RAM disk devices in script friendly format.\n"
		"\t--flush\t\t\tErase all data to a specified rxdsk device \033[31;1m(dangerous)\033[0m.\n"
		"\t--resize\t\tDynamically grow the size of an existing rxdsk device.\n\n"
		"\t--archive\t\tUsing zlib, archive an rxdsk device to a data file.\n"
		"\t--restore\t\tUsing zlib, restore an rxdsk device from an archived data file.\n\n"
		"\t--rxc-map\t\tMap an rxdsk device as a caching node to another block device.\n"
		"\t--rxc-unmap\t\tMap an rxdsk device as a caching node to anotther block device.\n"
		"\t--stat-cache\t\tObtain RapidCache Mappings statistics.\n"
		"\t--enable-crypt\t\tInitialize a storage volume for data encryption.\n"
		"\t--activate-crypt\tActivate an encryption volume.\n"
		"\t--deactivate-crypt\tDeactivate an encryption volume.\n\n");
	printf("Parameters:\n\t[size]\t\t\tSpecify desired size of attaching RAM disk device in MBytes.\n"
		"\t[unit]\t\t\tSpecify unit number of RAM disk device to detach.\n"
		"\t[src]\t\t\tSource path for archive/restore options.\n"
		"\t[dest]\t\t\tDestination path for arcive/restore options.\n"
		"\t[cache]\t\t\tSpecify rxd node to use as caching volume.\n"
		"\t[source]\t\tSpecify block device to map cache to.\n"
		"\t[block size]\t\tSpecify cache block size in KBytes (multiples of 4).\n"
		"\n");
	printf("Example:\n\t%s --attach 64\n\t%s --detach rxd2\n\t%s --resize rxd2 128\n"
		"\t%s --archive rxd0 rxd-052911.dat\n\t%s --restore rxd-052911.dat"
		" rxd0\n\t%s --rxc-map rxd1 /dev/sdb\n\t%s --rxc-unmap rxc0\n\t%s --flush rxd2\n"
		"\t%s --enable-crypt /dev/rxd0\n\t%s --activate-crypt /dev/rxd0\n"
		"\t%s --deactivate-crypt /dev/mapper/rxcrypt-rxd0\n\n",
		string, string, string, string, string, string, string, string, string, string, string);
}

int parse_input(int argcin, char *argvin[])
{
	int err = 0;
	struct RxD_PROFILE *rxd;	 /* These are dummy pointers to  */
	struct RxC_PROFILE *rxc;	 /* help create the linked  list */

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

	rxd = (struct RxD_PROFILE *)search_targets();
	rxc = (struct RxC_PROFILE *)search_cache();

	if (strcmp(argvin[1], "--list") == 0) {
		if (rxd == NULL) {
			printf("Unable to locate any rxdsk devices.\n");
			return 0;
		}
		err = list_devices(rxd, rxc);
		return err;
	} else if (strcmp(argvin[1], "--short-list") == 0) {
		if (rxd == NULL) {
			printf("Unable to locate any rxdsk devices.\n");
			return 0;
		}
		err = short_list_devices(rxd, rxc);
		return err;
	}

	if (strcmp(argvin[1], "--attach") == 0) {
		if (argcin != 3) goto invalid_out;
		err = attach_device(rxd, strtoul(argvin[2], (char **)NULL, 10));
	} else if (strcmp(argvin[1], "--detach") == 0) {
		if (argcin != 3) goto invalid_out;
		if (rxd == NULL) {
			printf("Unable to locate any rxdsk devices.\n");
			return 0;
		}
		err = detach_device(rxd, rxc, argvin[2]);
	} else if (strcmp(argvin[1], "--resize") == 0) {
		if (argcin != 4) goto invalid_out;
		if (rxd == NULL) {
			printf("Unable to locate any rxdsk devices.\n");
			return 0;
		}
		err = resize_device(rxd, argvin[2], strtoul(argvin[3], (char **)NULL, 10));
	} else if (strcmp(argvin[1], "--archive") == 0) {
		if (argcin != 4) goto invalid_out;
		err = archive_rxd_volume(rxd, argvin[2], argvin[3]);
	} else if (strcmp(argvin[1], "--restore") == 0) {
		if (argcin != 4) goto invalid_out;
		err = restore_rxd_volume(rxd, argvin[2], argvin[3]);
	} else if (strcmp(argvin[1], "--rxc-map") == 0) {
		if (argcin != 4) goto invalid_out;
		err = rxcache_map(rxd, rxc, argvin[2], argvin[3]);
	} else if (strcmp(argvin[1], "--rxc-unmap") == 0) {
		if (argcin != 3) goto invalid_out;
		err = rxcache_unmap(rxc, argvin[2]);
	} else if (strcmp(argvin[1], "--flush") == 0) {
		if (argcin != 3) goto invalid_out;
		err = rxdsk_flush(rxd, rxc, argvin[2]);
	} else if (strcmp(argvin[1], "--stat-cache") == 0) {
		if (argcin != 3) goto invalid_out;
		err = stat_rxcache_mapping(rxc, argvin[2]);
	} else if (strcmp(argvin[1], "--enable-crypt") == 0) {
		if (argcin != 3) goto invalid_out;
		err = format_crypt(argvin[2]);
	} else if (strcmp(argvin[1], "--activate-crypt") == 0) {
		if (argcin != 3) goto invalid_out;
		err = activate_crypt(argvin[2]);
	} else if (strcmp(argvin[1], "--deactivate-crypt") == 0) {
		if (argcin != 3) goto invalid_out;
		err = deactivate_crypt(argvin[2]);
	} else {
		printf("Error. %s is an unsupported option.\n", argvin[1]);
		return -1;
	}

	return err;
invalid_out:
	printf("Error. Invalid number of arguments entered.\n");
	return -1;
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
		return -1;
	}
	if (access(SYS_RDSK, F_OK) == -1) {
		printf("Please ensure that the RapidDisk module is loaded and retry.\n");
		return -1;
	}

	if ((fp = fopen(proc_mod, "r")) == NULL) {
		printf("%s: fopen: %s\n", __func__, strerror(errno));
		return -1;
	}
	fread(string, BUFSZ, 1, fp);
	fclose(fp);
	if ((strstr(string, "rxcache") == NULL) || (strstr(string, "dm_crypt") == NULL)) {
		printf("Please ensure that the RapidCache and dm_crypt modules "
		       "are loaded and retry.\n");
		return -1;
	}

	err = parse_input(argc, argv);
	return err;
}
