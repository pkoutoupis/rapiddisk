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
 ** @filename: daemon.h
 ** @description: A header file containing all commonly used variables
 **	 and such.
 **
 ** @date: 29Jul20, petros@petroskoutoupis.com
 ********************************************************************************/
#ifndef DAEMON_H
#define DAEMON_H

typedef struct PTHREAD_ARGS {
        bool verbose;
        unsigned char port[0xf];
} PTHREAD_ARGS;

void *mgmt_thread(void *);
int json_check_status(unsigned char *);
int json_return_status(unsigned char *, int);
#endif
