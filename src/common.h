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

#define PROCESS			"rapiddisk"
#define DAEMON			PROCESS "d"
#define COPYRIGHT		"Copyright 2011 - 2020 Petros Koutoupis"
#define VERSION_NUM	  	"7.0.0"
#define SUCCESS			0
#define INVALID_VALUE		-1
#define NAMELEN			0x200
#define BUFSZ			0x10000
#define PAYLOADSZ		0x80000 /* 512K: this is our max read limit for libcurl */
#define SYS_RDSK		"/sys/kernel/rapiddisk/mgmt"
#define WRITETHROUGH		0
#define WRITEAROUND		1
#define DEFAULT_MGMT_PORT	"9118"

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

typedef char bool;

#define FALSE			0
#define TRUE			1

#define CMD_PING_DAEMON		"/v1/checkServiceStatus"
#define CMD_LIST_VOLUMES	"/v1/listAllVolumes"
#define CMD_LIST_RD_VOLUMES	"/v1/listRapidDiskVolumes"
#define CMD_RDSK_CREATE		"/v1/createRapidDisk"
#define CMD_RDSK_REMOVE		"/v1/removeRapidDisk"
#define CMD_RDSK_RESIZE		"/v1/resizeRapidDisk"
#define CMD_RDSK_FLUSH		"/v1/flushRapidDisk"
#define CMD_RCACHE_CREATE	"/v1/createRapidDiskCache"
#define CMD_RCACHE_REMOVE	"/v1/removeRapidDiskCache"
#define CMD_SHOW_STATS		"/v1/showStatistics"

#endif
