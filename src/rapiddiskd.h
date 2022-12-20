/**
 * @file
 * @brief Daemon defines
 * @details This header file defines some constants used by the daemon
 * @copyright @verbatim
Copyright © 2011 - 2022 Petros Koutoupis

All rights reserved.

This file is part of RapidDisk.

RapidDisk is free software: you can redistribute it and/or modify@n
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

RapidDisk is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RapidDisk.  If not, see <http://www.gnu.org/licenses/>.

SPDX-License-Identifier: GPL-2.0-or-later
@endverbatim
 * @author Petros Koutoupis \<petros\@petroskoutoupis.com\>
 * @author Matteo Tenca \<matteo.tenca\@gmail.com\>
 * @version 8.2.0
 * @date 26 September 2022
 */

#ifndef DAEMON_H
#define DAEMON_H

#include <syslog.h>

#define DEFAULT_MGMT_PORT       "9118"

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

#define PID_FILE			"/run/rapiddiskd.pid"
#define D_STDERR_LOG		"/tmp/rapiddiskd_err.log"
#define D_STDOUT_LOG		"/tmp/rapiddiskd_out.log"
#define D_EXITING			"Daemon exiting."
#define D_STARTING			"Starting daemon..."
#define D_RECV_REQ			"Recevied request '%s'."
#define D_LOOP_EXITING		"Daemon loop function exiting: %s."
#define D_SIGNAL_RECEIVED	"Signal_handler function, SIGNAL received: %s."

int mgmt_thread(void *arg);
#endif
