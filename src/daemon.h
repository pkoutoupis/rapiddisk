/*********************************************************************************
 ** Copyright © 2011 - 2022 Petros Koutoupis
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
 ** @filename: daemon.h
 ** @description: A header file containing all commonly used variables
 **	 and such.
 **
 ** @date: 29Jul20, petros@petroskoutoupis.com
 ********************************************************************************/
#ifndef DAEMON_H
#define DAEMON_H

#include <syslog.h>

#define DEFAULT_MGMT_PORT       "9118"

typedef struct PTHREAD_ARGS {
	bool verbose;
	unsigned char port[0xf];
	unsigned char path[NAMELEN];
} PTHREAD_ARGS;

#define CMD_PING_DAEMON		"/v1/checkServiceStatus"
#define CMD_LIST_RESOURCES	"/v1/listAllResources"
#define CMD_LIST_RD_VOLUMES	"/v1/listRapidDiskVolumes"
#define CMD_RDSK_CREATE		"/v1/createRapidDisk"
#define CMD_RDSK_REMOVE		"/v1/removeRapidDisk"
#define CMD_RDSK_RESIZE		"/v1/resizeRapidDisk"
#define CMD_RDSK_FLUSH		"/v1/flushRapidDisk"
#define CMD_RDSK_LOCK		"/v1/lockRapidDisk"
#define CMD_RDSK_UNLOCK		"/v1/unlockRapidDisk"
#define CMD_RCACHE_CREATE	"/v1/createRapidDiskCache"
#define CMD_RCACHE_REMOVE	"/v1/removeRapidDiskCache"
#define CMD_RCACHE_STATS	"/v1/showRapidDiskCacheStats"
#define CMD_LIST_NVMET		"/v1/listAllNVMeTargets"
#define CMD_LIST_NVMET_PORTS	"/v1/listAllNVMePorts"

void *mgmt_thread(void *);
int json_status_check(unsigned char *);
int json_status_unsupported(unsigned char *);
#endif
