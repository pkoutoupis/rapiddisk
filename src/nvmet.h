/**
 * @file nvmet.h
 * @brief NVME function declarations
 * @details This header file declares some NVME related functions
 * @copyright @verbatim
Copyright © 2011 - 2023 Petros Koutoupis

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
 * @version 9.1.0
 * @date 23 April 2023
 */

#ifndef NVMET_H
#define NVMET_H

#include "common.h"

#define XFER_MODE_TCP			0
#define XFER_MODE_RDMA			1
#define XFER_MODE_LOOP			2

struct NVMET_PROFILE *nvmet_scan_subsystem(char *return_message, int *rc);
struct NVMET_PORTS *nvmet_scan_ports(char *return_message, int *rc);
struct NVMET_PORTS *nvmet_scan_all_ports(char *return_message, int *rc);
char *nvmet_interface_ip_get(char *interface, char *return_message);
bool check_nvmet_subsystem(char *return_message);
#ifndef SERVER
int nvmet_view_exports(bool json_flag, char *error_message);
int nvmet_view_ports(bool json_flag, char *error_message);
int nvmet_enable_port(char *interface, int port, int protocol, char *return_message);
int nvmet_create_port(char *interface, int port, int protocol, char *return_message);
int nvmet_disable_port(int port, char *return_message);
int nvmet_remove_port(int port, char *return_message);
int nvmet_add_host(char *host, char *return_message);
int nvmet_remove_host(char *host, char *return_message);
int nvmet_export_volume(struct RD_PROFILE *rd_prof, RC_PROFILE *rc_prof, char *device, char *host, int port, char *return_message);
int nvmet_revalidate_size(struct RD_PROFILE *rd_prof, RC_PROFILE *rc_prof, char *device, char *return_message);
int nvmet_unexport_volume(char *device, char *host, int port, char *return_message);
int nvmet_link_host_nqn(char *device, char *host, char *return_message);
int nvmet_unlink_host_nqn(char *device, char *host, char *return_message);
int nvmet_add_nqn(struct RD_PROFILE *rd_prof, RC_PROFILE *rc_prof, char *device, char *return_message);
int nvmet_remove_nqn(char *device, char *return_message);
int nvmet_link_port_nqn(char *device, int port, char *return_message);
int nvmet_unlink_port_nqn(char *device, int port, char *return_message);
#else
int nvmet_view_ports_json(char *error_message, char **json_result);
int nvmet_view_exports_json(char *error_message, char **json_result);
#endif
#endif //NVMET_H
