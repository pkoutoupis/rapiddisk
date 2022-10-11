/** @file
 * @brief NVME functions declarations
 */
//
// Created by Matteo Tenca on 26/09/2022.
//

#ifndef NVMET_H
#define NVMET_H

#include "common.h"

#define XFER_MODE_TCP			0
#define XFER_MODE_RDMA			1
struct NVMET_PROFILE *nvmet_scan_subsystem(char *return_message);
struct NVMET_PORTS *nvmet_scan_ports(char *return_message);
struct NVMET_PORTS *nvmet_scan_all_ports(char *return_message);
int nvmet_view_exports(bool json_flag);
int nvmet_view_ports(bool json_flag);
int nvmet_enable_port(char *, int, int);
int nvmet_disable_port(int);
int nvmet_export_volume(struct RD_PROFILE *, RC_PROFILE *, char *, char *, int);
int nvmet_revalidate_size(struct RD_PROFILE *, RC_PROFILE *, char *);
int nvmet_unexport_volume(char *, char *, int);
char *nvmet_interface_ip_get(char *interface);
#endif //NVMET_H
