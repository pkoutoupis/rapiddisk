/*********************************************************************************
 ** Copyright (c) 2011-2015 Petros Koutoupis
 ** All rights reserved.
 **
 ** @project: rapiddisk
 **
 ** @filename: core.c
 ** @description: This file contains the core routines of rxadm.
 **
 ** @date: 15Oct10, petros@petroskoutoupis.com
 ********************************************************************************/

#include "common.h"
#include <dirent.h>
#include <malloc.h>
#include <linux/fs.h>
#include <sys/ioctl.h>

#define NAMELEN		256
#define FILEDATA	64
#define DEFAULT_SIZE	32	/* In MBytes */


struct RxD_PROFILE *head =  (struct RxD_PROFILE *) NULL;
struct RxD_PROFILE *end =   (struct RxD_PROFILE *) NULL;
struct RxC_PROFILE *chead = (struct RxC_PROFILE *) NULL;
struct RxC_PROFILE *cend =  (struct RxC_PROFILE *) NULL;

unsigned char *sys_rdsk	  = "/sys/kernel/rapiddisk/mgmt";
unsigned char *sys_block  = "/sys/block";
unsigned char *etc_mtab   = "/etc/mtab";
unsigned char *dev_mapper = "/dev/mapper";

unsigned char *read_info(unsigned char *, unsigned char *);
struct RxD_PROFILE *search_targets(void);
struct RxC_PROFILE *search_cache(void);

unsigned char *read_info(unsigned char *name, unsigned char *string)
{
	int err, len;
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
	err = fread(buf, FILEDATA, 1, fp);
	len = strlen(buf);
	strncpy(obuf, buf, (len - 1));
	sprintf(obuf, "%s", obuf);
	fclose(fp);

	return obuf;
}

struct RxD_PROFILE *search_targets(void)
{
	int err, n = 0;
	unsigned char file[NAMELEN] = {0};
	struct dirent **list;
	struct RxD_PROFILE *prof;

	if ((err = scandir(sys_block, &list, NULL, NULL)) < 0) {
		if (ENOENT) {
			printf("There are no listed block devices.\n\n");
			exit(0);
		} else {
			printf("%s: scandir: %s\n", __func__, strerror(errno));
			exit(err);
		}
	}
	for (;n < err; n++) {
		if (strncmp(list[n]->d_name, "rxd", 3) == 0) {
			prof = (struct RxD_PROFILE *)calloc(1, sizeof(struct RxD_PROFILE));
			if (prof == NULL) {
				printf("%s: calloc: %s\n", __func__, strerror(errno));
				exit(-1);
			}
			strcpy(prof->device, (unsigned char *)list[n]->d_name);
			sprintf(file, "%s/%s", sys_block, list[n]->d_name);
			prof->size = (BYTES_PER_SECTOR * strtoull(read_info(file, "size"), NULL, 10));

			if (head == NULL) {
				head = prof;
			} else {
				end->next = prof;
			}
			end = prof;
			prof->next = NULL;
		}
		free(list[n]);
	}
	return head;
}

struct RxC_PROFILE *search_cache(void)
{
	int num, num2, num3, n = 0, i, z;
	struct dirent **list, **nodes, **maps;
	unsigned char file[NAMELEN] = {0};
	struct RxC_PROFILE *prof;

	if ((num = scandir(dev_mapper, &list, NULL, NULL)) < 0) {
		if (ENOENT) {
			return NULL;
		} else {
			printf("%s: scandir: %s\n", __func__, strerror(errno));
			exit(num);
		}
	}
	if ((num2 = scandir(sys_block, &nodes, NULL, NULL)) < 0) {
		if (ENOENT) {
			printf("There are no listed block devices.\n\n");
			exit(0);
		} else {
			printf("%s: scandir: %s\n", __func__, strerror(errno));
			exit(num);
		}
	}

	for (;n < num; n++) {
		if ((strncmp(list[n]->d_name, "rxc", 3) == 0) && (strncmp(list[n]->d_name, "rxcrypt", 7) != 0)) {
			prof = (struct RxC_PROFILE *)calloc(1, sizeof(struct RxC_PROFILE));
			if (prof == NULL) {
				printf("%s: calloc: %s\n", __func__, strerror(errno));
				exit(-1);
			}
			strcpy(prof->device, (unsigned char *)list[n]->d_name);
			for (i = 0;i < num2; i++) {
				if (strncmp(nodes[i]->d_name, "dm-", 3) == 0) {
					sprintf(file, "%s/%s", sys_block, nodes[i]->d_name);
					if (strncmp(read_info(file, "dm/name"), prof->device, sizeof(prof->device)) == 0) {
						sprintf(file, "%s/%s/slaves", sys_block, nodes[i]->d_name);
						if ((num3 = scandir(file, &maps, NULL, NULL)) < 0) {
							if (ENOENT) {
								printf("There are no listed block devices.\n\n");
								exit(-1);
							} else {
								printf("%s: scandir: %s\n", __func__, strerror(errno));
								exit(num);
							}
						}
						for (z=0;z < num3; z++) {
							if (strncmp(maps[z]->d_name, ".", 1) != 0) {
								if (strncmp(maps[z]->d_name, "rxd", 3) == 0)
									strcpy(prof->cache, (unsigned char *)maps[z]->d_name);
								else
									strcpy(prof->source, (unsigned char *)maps[z]->d_name);
							}
							free(maps[z]);
						}
					}
				}
				/* free(nodes[i]);*/ //TODO: Figure out while this is faulting.
			}
			if (chead == NULL) {
				chead = prof;
			} else {
				cend->next = prof;
			}
			cend = prof;
			prof->next = NULL;
		}
		free(list[n]);
	}
	return chead;
}

int list_devices(struct RxD_PROFILE *rxd_prof, struct RxC_PROFILE *rxc_prof)
{
	int num = 1;

	printf("List of RapidDisk device(s):\n\n");

	while (rxd_prof != NULL) {
		printf(" RapidDisk Device %d: %s\tSize (KB): %llu\n", num, rxd_prof->device,
			(rxd_prof->size / 1024));
		num++;
		rxd_prof = rxd_prof->next;
	}
	printf("\nList of RapidCache mapping(s):\n\n");
	if (rxc_prof == NULL) {
		printf("  None\n\n");
		return 0;
	}
	num = 1;
	while (rxc_prof != NULL) {
	printf(" RapidCache Target %d: %s\tCache: %s  Target: %s (WRITETHROUGH)\n",
			num, rxc_prof->device, rxc_prof->cache, rxc_prof->source);
		num++;
		rxc_prof = rxc_prof->next;
	}
	printf("\n");
	return 0;
}

int stat_rxcache_mapping(struct RxC_PROFILE *rxc_prof, unsigned char *cache)
{
	unsigned char cmd[32] = {0};

	if (rxc_prof == NULL) {
		printf("  No RapidCache Mappings exist.\n\n");
		return 1;
	}
	sprintf(cmd, "dmsetup status %s", cache);
	system(cmd);
	printf("\n");
	return 0;
}

int short_list_devices(struct RxD_PROFILE *rxd_prof, struct RxC_PROFILE *rxc_prof)
{
	if ((rxd_prof == NULL) && (rxc_prof == NULL)) {
		printf("There are no rxd or rxc devices.\n");
		return 0;
	}

	while (rxd_prof != NULL) {
		printf("%s:%llu\n", rxd_prof->device, rxd_prof->size);
		rxd_prof = rxd_prof->next;
	}
	while (rxc_prof != NULL) {
		printf("%s:%s,%s\n", rxc_prof->device, rxc_prof->cache, rxc_prof->source);
		rxc_prof = rxc_prof->next;
	}
	return 0;
}

int attach_device(struct RxD_PROFILE *prof, unsigned long size)
{
	int dsk, err = -1;
	FILE *fp;
	unsigned char string[BUFSZ], name[16];

	/* echo "rxdsk attach 1 65536" > /sys/kernel/rapiddisk/mgmt <- in sectors*/
	for (dsk = 0; prof != NULL; dsk++) {
		sprintf(string, "%s,%s", string, prof->device);
		prof = prof->next;
	}

	while (dsk >= 0) {
		sprintf(name, "rxd%d", dsk);
		if (strstr(string, (const char *)name) == NULL) {
			break;
		}
		dsk--;
	}
	if ((fp = fopen(sys_rdsk, "w")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, sys_rdsk, strerror(errno));
		return -1;
	}
	if ((err = fprintf(fp, "rxdsk attach %d %llu\n", dsk,
		(((unsigned long long)size * 1024 * 1024) / BYTES_PER_SECTOR))) < 0) {	/* module supports sector input */
		printf("%s: fprintf: %s\n", __func__, strerror(errno));
		return err;
	}
	printf("Attached device rxd%d of size %lu Mbytes\n", dsk, size);
	fclose(fp);
	return 0;
}

int detach_device(struct RxD_PROFILE *rxd_prof, RxC_PROFILE * rxc_prof, unsigned char *string)
{
	int err = -1;
	FILE *fp;
	unsigned char *buf;

	/* echo "rxdsk detach 1" > /sys/kernel/rapiddisk/mgmt */
	while (rxd_prof != NULL) {
		if (strcmp(string, rxd_prof->device) == 0)
			err = 0;
		rxd_prof = rxd_prof->next;
	}
	if (err != 0) {
		printf("Error. Device %s does not exist.\n", string);
		return -1;
	}
	/* Check to make sure rxdsk device isn't in a mapping */
	while (rxc_prof != NULL) {
		if (strcmp(string, rxc_prof->cache) == 0) {
			printf("Error. Unable to remove %s.\nThis rxdsk device is currently"
				" mapped as a cache drive to %s.\n\n", string, rxc_prof->device);
			return -1;
		}
		rxc_prof = rxc_prof->next;
	}

	if ((buf = (char *)malloc(BUFSZ)) == '\0') {
		printf("%s: malloc: Unable to allocate memory.\n", __func__);
		return -1;
	}

	/* Here we are starting to check to see if the device is mounted */
	if ((fp = fopen(etc_mtab, "r")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, etc_mtab, strerror(errno));
		return -1;
	}
	fread(buf, BUFSZ, 1, fp);
	fclose(fp);
	if ((strstr(buf, string) != NULL)) {
		printf("%s is currently mounted. Please \"umount\" and retry.\n", string);
		return -1;
	}

	/* This is where we begin to detach the block device */
	if ((fp = fopen(sys_rdsk, "w")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, sys_rdsk, strerror(errno));
		return -1;
	}

	if ((err = fprintf(fp, "rxdsk detach %s\n", string + 3)) < 0) {
		printf("%s: fprintf: %s\n", __func__, strerror(errno));
		return err;
	}
	printf("Detached device %s\n", string);
	fclose(fp);

	return 0;
}

int resize_device(struct RxD_PROFILE *prof, unsigned char *string, unsigned long size)
{
	int err = -1;
	FILE *fp;

	/* echo "rxdsk resize 1 128" > /sys/kernel/rapiddisk/mgmt */
	while (prof != NULL) {
		if (strcmp(string, prof->device) == 0) {
			if (((unsigned long long)size * 1024 * 1024) <= prof->size) {
				printf("Error. Please specify a size larger than %llu Mbytes\n", prof->size);
				return -1;
			}
			err = 0;
		}
		prof = prof->next;
	}
	if (err != 0) {
		printf("Error. Device %s does not exist.\n", string);
		return -1;
	}

	/* This is where we begin to detach the block device */
	if ((fp = fopen(sys_rdsk, "w")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, sys_rdsk, strerror(errno));
		return -1;
	}

	if ((err = fprintf(fp, "rxdsk resize %s %llu\n", string + 3,
		(((unsigned long long)size * 1024 * 1024) / BYTES_PER_SECTOR))) < 0) {
		printf("%s: fprintf: %s\n", __func__, strerror(errno));
		return err;
	}
	printf("Resized device %s to %lu Mbytes\n", string, size);
	fclose(fp);

	return 0;
}

int rxcache_map(struct RxD_PROFILE *rxd_prof, struct RxC_PROFILE * rxc_prof, unsigned char *cache,
			unsigned char *source)
{
	int err = -1, node, fd;
	unsigned long long source_sz = 0, cache_sz = 0;
	FILE *fp;
	unsigned char *buf, string[BUFSZ], name[64] = {0}, str[64] = {0}, *dup, *token;

	while (rxd_prof != NULL) {
		if (strcmp(cache, rxd_prof->device) == 0) {
			err = 0;
		}
		rxd_prof = rxd_prof->next;
	}
	if (err != 0) {
		printf("Error. Device %s does not exist.\n", cache);
		return -1;
	}

	/* Check to make sure it is a normal block device found in /dev */
	if (strncmp(source, "/dev/", 5) != 0) {
		printf("Error. Source device does not seem to be a normal block device listed\n"
			"in the /dev directory path.\n\n");
		return -1;
	}
	/* Check to make sure that cache/source devices are not in a mapping already */
	while (rxc_prof != NULL) {
		if ((strcmp(cache, rxc_prof->cache) == 0) || (strcmp(source+5, rxc_prof->source) == 0)) {
			printf("Error. At least one of your cache/source devices is currently mapped to %s.\n\n",
				rxc_prof->device);
			return -1;
		}
		rxc_prof = rxc_prof->next;
	}

	if ((buf = (char *)malloc(BUFSZ)) == '\0') {
		printf("%s: malloc: Unable to allocate memory.\n", __func__);
		return -1;
	}
	if ((fp = fopen(etc_mtab, "r")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, etc_mtab, strerror(errno));
		return -1;
	}
	fread(buf, BUFSZ, 1, fp);
	fclose(fp);
	if ((strstr(buf, cache) != NULL)) {
		printf("%s is currently mounted. Please \"umount\" and retry.\n", string);
		return -1;
	}

	if ((fd = open(source, O_RDONLY)) < 0) {
		printf("%s: open: %s\n", __func__, strerror(errno));
		return errno;
	}

	if (ioctl(fd, BLKGETSIZE, &source_sz) == -1) {
		printf("%s: ioctl: %s\n", __func__, strerror(errno));
		return errno;
	}
	close(fd);

	sprintf(name, "/dev/%s", cache);
	if ((fd = open(name, O_RDONLY)) < 0) {
		printf("%s: open: %s\n", __func__, strerror(errno));
		return errno;
	}

	if (ioctl(fd, BLKGETSIZE, &cache_sz) == -1) {
		printf("%s: ioctl: %s\n", __func__, strerror(errno));
		return errno;
	}
	close(fd);
	memset(name, 0x0, 16);

	dup = strdup(source);
	token = strtok((char *)dup, "/");
	while (token != NULL) {
		sprintf(str, "%s", token);
		token = strtok(NULL, "/");
	}
	sprintf(name, "rxc_%s", str);

	memset(string, 0x0, BUFSZ);
	/* echo 0 4194303 rxcache /dev/sdb /dev/rxd0 0 196608|dmsetup create rxc0 */
	/* Param after echo 0 & 1: starting and ending offsets of source device      *
	 * Param 3: always rxcache; Param 4 & 5: path to source device and rxdsk dev *
	 * Param 6: is the size of the cache  */
	sprintf(string, "echo 0 %llu rxcache %s /dev/%s %llu|dmsetup create %s\n",
		source_sz, source, cache, cache_sz, name);

	if ((err = system(string)) == 0) {
		printf("Command to map %s with %s and %s has been sent.\nVerify with \"--list\"\n\n",
			name, cache, source);
	} else
		printf("Error. Unable to create map. Please verify all input values are correct.\n\n");

	return err;
}

int rxcache_unmap(struct RxC_PROFILE *prof, unsigned char *string)
{
	int err = -1;
	FILE *fp;
	unsigned char *buf;
	unsigned char cmd[32] = {0};

	/* dmsetup remove rxc0 */
	while (prof != NULL) {
		if (strcmp(string, prof->device) == 0) {
			err = 0;
		}
		prof = prof->next;
	}
	if (err != 0) {
		printf("Error. Cache target %s does not exist.\n", string);
		return -1;
	}

	if ((buf = (char *)malloc(BUFSZ)) == '\0') {
		printf("%s: malloc: Unable to allocate memory.\n", __func__);
		return -1;
	}

	/* Here we are starting to check to see if the device is mounted */
	if ((fp = fopen(etc_mtab, "r")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, etc_mtab, strerror(errno));
		return -1;
	}
	fread(buf, BUFSZ, 1, fp);
	fclose(fp);
	if ((strstr(buf, string) != NULL)) {
		printf("%s is currently mounted. Please \"umount\" and retry.\n", string);
		return -1;
	}

	sprintf(cmd, "dmsetup remove %s\n", string);
	if ((err = system(cmd)) == 0)
		printf("Command to unmap %s has been sent\nVerify with \"--list\"\n\n", string);
	else
		printf("Error. Unable to unmap %s. Please check to make sure nothing is wrong.n\n", string);

	return err;
}

int rxdsk_flush(struct RxD_PROFILE *rxd_prof, RxC_PROFILE *rxc_prof, unsigned char *string)
{
	int fd, err = -1;
	unsigned char file[16] = {0}, *buf;
	FILE *fp;

	while (rxd_prof != NULL) {
		if (strcmp(string, rxd_prof->device) == 0)
			err = 0;
		rxd_prof = rxd_prof->next;
	}
	if (err != 0) {
		printf("Error. Device %s does not exist.\n", string);
		return -1;
	}
	/* Check to make sure rxdsk device isn't in a mapping */
	while (rxc_prof != NULL) {
		if (strcmp(string, rxc_prof->cache) == 0) {
			printf("Error. Unable to remove %s.\nThis rxdsk device is currently"
				" mapped as a cache drive to %s.\n\n", string, rxc_prof->device);
			return -1;
		}
		rxc_prof = rxc_prof->next;
	}

	if ((buf = (char *)malloc(BUFSZ)) == '\0') {
		printf("%s: malloc: Unable to allocate memory.\n", __func__);
		return -1;
	}

	/* Here we are starting to check to see if the device is mounted */
	if ((fp = fopen(etc_mtab, "r")) == NULL) {
		printf("%s: fopen: %s: %s\n", __func__, etc_mtab, strerror(errno));
		return -1;
	}
	fread(buf, BUFSZ, 1, fp);
	fclose(fp);
	if ((strstr(buf, string) != NULL)) {
		printf("%s is currently mounted. Please \"umount\" and retry.\n", string);
		return -1;
	}

	sprintf(file, "/dev/%s", string);

	if ((fd = open(file, O_WRONLY)) < 0) {
		printf("%s: open: %s\n", __func__, strerror(errno));
		return errno;
	}

	if (ioctl(fd, BLKFLSBUF, 0) == -1) {
		printf("%s: ioctl: %s\n", __func__, strerror(errno));
		return errno;
	}
	close(fd);
	printf("Flushed all data from device %s\n", string);

	return err;
}
