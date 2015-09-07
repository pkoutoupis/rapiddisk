/*********************************************************************************
 ** Copyright (c) 2011-2015 Petros Koutoupis
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
#include <dirent.h>

#define UTIL			"rapiddisk"
#define COPYRIGHT		"Copyright 2011-2015 Petros Koutoupis"
#define VERSION_NUM	  	"3.2"
#define BYTES_PER_SECTOR	512	/* Traditional sector size */
#define BUFSZ			65536
#define DES_KEY			"05291983"
#define NAMELEN			256

typedef struct RxD_PROFILE{		/* For rxdsk device list   */
	unsigned char device[16];
	unsigned long long size;
	struct RxD_PROFILE *next;
}RxD_PROFILE;

typedef struct RxC_PROFILE{		/* For rxcache node list   */
	unsigned char device[64];
	unsigned char cache[16];
	unsigned char source[64];
	struct RxC_PROFILE *next;
}RxC_PROFILE;

#define ERROR(fmt, args...) syslog(LOG_ERR, "rapiddisk: %s:", fmt, __FUNCTION__, __LINE__, ##args);
#define INFO(fmt, args...) syslog(LOG_INFO, "rapiddisk: %s:", fmt, __FUNCTION__, __LINE__, ##args);
#endif
