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
 ** @date: 26Aug20, petros@petroskoutoupis.com
 ********************************************************************************/

#ifndef CLI_H
#define CLI_H

#define PROC_MODULES			"/proc/modules"
#define SYS_RDSK			"/sys/kernel/rapiddisk/mgmt"

#define ACTION_NONE			0x0
#define ACTION_ATTACH			0x1
#define ACTION_DETACH			0x2
#define ACTION_FLUSH			0x3
#define ACTION_LIST			0x4
#define ACTION_CACHE_MAP		0x5
#define ACTION_RESIZE			0x6
#define ACTION_CACHE_STATS		0x7
#define ACTION_CACHE_UNMAP		0x8

typedef struct RD_PROFILE{      /* For RapidDisk device list     */
	unsigned char device[0xf];
	unsigned long long size;
	struct RD_PROFILE *next;
} RD_PROFILE;

typedef struct RC_PROFILE{      /* For RapidDisk-Cache node list */
	unsigned char device[NAMELEN];
	unsigned char cache[0x20];
	unsigned char source[NAMELEN];
	struct RC_PROFILE *next;
} RC_PROFILE;

int mem_device_list(struct RD_PROFILE *, struct RC_PROFILE *);
int mem_device_attach(struct RD_PROFILE *, unsigned long);
int mem_device_detach(struct RD_PROFILE *, struct RC_PROFILE *, unsigned char *);
int mem_device_resize(struct RD_PROFILE *, unsigned char *, unsigned long);
int mem_device_flush(struct RD_PROFILE *, RC_PROFILE *, unsigned char *);
int cache_device_map(struct RD_PROFILE *, struct RC_PROFILE *, unsigned char *, unsigned char *, int);
int cache_device_unmap(struct RC_PROFILE *, unsigned char *);
int cache_device_stat(struct RC_PROFILE *, unsigned char *);
int json_device_list(unsigned char *, struct RD_PROFILE *, struct RC_PROFILE *);

#endif
