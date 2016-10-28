/*********************************************************************************
 ** Copyright (c) 2011-2016 Petros Koutoupis
 ** All rights reserved.
 **
 ** @project: rapiddisk
 **
 ** @filename: common.h
 ** @description: A header file containing all commonly used variables
 **	 and such.
 **
 ** @date: 14Oct10, petros@petroskoutoupis.com
 ********************************************************************************/
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>

#define UTIL			"rapiddisk"
#define COPYRIGHT		"Copyright 2011-2016 Petros Koutoupis"
#define VERSION_NUM	  	"4.4"
#define SUCCESS			0x0
#define NAMELEN			0x100
#define BYTES_PER_SECTOR	0x200
#define BUFSZ			0x10000
#define DEFAULT_DES_KEY		"0529198310301978"
#define LEGACY_DES_KEY		"05291983"
#define SYS_RDSK		"/sys/kernel/rapiddisk/mgmt"
#define KEY_FILE		"/etc/rapiddisk/key"
#define WRITETHROUGH		0
#define WRITEAROUND		1

typedef struct RD_PROFILE{	/* For RapidDisk device list     */
	unsigned char device[16];
	unsigned long long size;
	struct RD_PROFILE *next;
} RD_PROFILE;

typedef struct RC_PROFILE{	/* For RapidDisk-Cache node list */
	unsigned char device[NAMELEN];
	unsigned char cache[16];
	unsigned char source[NAMELEN];
	struct RC_PROFILE *next;
} RC_PROFILE;

#define ERROR(fmt, args...) syslog(LOG_ERR, "rapiddisk: %s:", fmt, __FUNCTION__, __LINE__, ##args);
#define INFO(fmt, args...) syslog(LOG_INFO, "rapiddisk: %s:", fmt, __FUNCTION__, __LINE__, ##args);
#endif
