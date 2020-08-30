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
 ** @filename: sys.c
 ** @description: This file contains the function to build and handle JSON objects.
 **
 ** @date: 1Aug20, petros@petroskoutoupis.com
 ********************************************************************************/

#include "common.h"
#include "daemon.h"
#include <sys/sysinfo.h>
#include <sys/time.h>

#define FILEDATA        0x40

int get_current_time(struct SYS_STATS *stats)
{       
        struct timeval tv; 
        unsigned long long timestamp;
        
        gettimeofday(&tv, NULL);
        timestamp = (((unsigned long long)(tv.tv_sec) * 1000) + \
                     ((unsigned long long)(tv.tv_usec) / 1000));
        sprintf(stats->timestamp, "%llu", timestamp);
        return SUCCESS;
}

int get_memory_usage(struct SYS_STATS *stats)
{       
        struct sysinfo si; 
        if (sysinfo(&si) < 0) { 
                syslog(LOG_ERR, "%s: %s (%d): Unable to retrieve memory usage.\n",
                              DAEMON, __func__, __LINE__);
                return INVALID_VALUE;
        }
        stats->mem_total = si.totalram;
        stats->mem_free = si.freeram;
        return SUCCESS;
}
