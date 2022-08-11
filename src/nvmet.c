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
 ** @filename: nvmet.c
 ** @description: This file contains the core routines of rapiddisk.
 **
 ** @date: 1Jul21, petros@petroskoutoupis.com
 ********************************************************************************/

#include "common.h"
#include "cli.h"
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SYS_NVMET               "/sys/kernel/config/nvmet"
#define SYS_NVMET_TGT           SYS_NVMET "/subsystems"
#define SYS_NVMET_PORTS         SYS_NVMET "/ports"
#define SYS_NVMET_HOSTS         SYS_NVMET "/hosts"
#define SYS_CLASS_NVME          "/sys/class/nvme"
#define NQN_HDR_STR             "nqn.2021-06.org.rapiddisk:"    /* Followed by <hostname>.<target> */
#define SYS_CLASS_NET           "/sys/class/net"
#define NVME_PORT               "4420"

struct NVMET_PROFILE *nvmet_head =  (struct NVMET_PROFILE *) NULL;
struct NVMET_PROFILE *nvmet_end =   (struct NVMET_PROFILE *) NULL;

struct NVMET_PORTS *ports_head =  (struct NVMET_PORTS *) NULL;
struct NVMET_PORTS *ports_end =   (struct NVMET_PORTS *) NULL;

/*
 * description: Scan all NVMe Targets NQNs
 */
struct NVMET_PROFILE *nvmet_scan_subsystem(void)
{
	int err, err2, n = 0, i;
	unsigned char file[NAMELEN * 2] = {0};
	struct NVMET_PROFILE *nvmet;
	struct dirent **list, **sublist;

	if (access(SYS_NVMET, F_OK) != SUCCESS) {
		printf("The NVMe Target subsystem is not loaded. Please load the nvmet and nvmet-tcp kernel\n"
		       " modules and ensure that the kernel user configuration filesystem is mounted.\n");
		return NULL;
	}

	if ((err = scandir(SYS_NVMET_TGT, &list, NULL, NULL)) < 0) {
		printf("%s: scandir: %s\n", __func__, strerror(errno));
		return NULL;
	}
	for (; n < err; n++) {
		if (strncmp(list[n]->d_name, ".", 1) != SUCCESS) {
			sprintf(file, "%s/%s/namespaces/", SYS_NVMET_TGT, list[n]->d_name);
			if ((err2 = scandir(file, &sublist, NULL, NULL)) < 0) {
				printf("%s: scandir: %s\n", __func__, strerror(errno));
				return NULL;
			}
			for (i = 0; i < err2; i++) {
				if (strncmp(sublist[i]->d_name, ".", 1) != SUCCESS) {
					if ((nvmet = (struct NVMET_PROFILE *)calloc(1, sizeof(struct NVMET_PROFILE))) == NULL ) {
						printf("%s: calloc: %s\n", __func__, strerror(errno));
						return NULL;
					}
					strcpy(nvmet->nqn, (unsigned char *)list[n]->d_name);

					sprintf(file, "%s/%s/namespaces/%s", SYS_NVMET_TGT, list[n]->d_name, sublist[i]->d_name);
					if (access(file, F_OK) != INVALID_VALUE) {
						sprintf(nvmet->device, "%s", read_info(file, "device_path"));
						nvmet->enabled = atoi(read_info(file, "enable"));
						nvmet->namespc = atoi(sublist[i]->d_name);
					}
					if (nvmet_head == NULL)
						nvmet_head = nvmet;
					else
						nvmet_end->next = nvmet;
					nvmet_end = nvmet;
					nvmet->next = NULL;
				}
				if (sublist[i] != NULL) free(sublist[i]);
			}
		}
		if (list[n] != NULL) free(list[n]);
	}
	return nvmet_head;
}

/*
 * description: Scan all exported NVMe Targets ports.
 */
struct NVMET_PORTS *nvmet_scan_ports(void)
{
	int err, err2, n = 0, i;
	unsigned char file[NAMELEN * 2] = {0};
	struct NVMET_PORTS *nvmet_ports;
	struct dirent **ports, **exports;

	if (access(SYS_NVMET, F_OK) != SUCCESS) {
		printf("The NVMe Target subsystem is not loaded. Please load the nvmet and nvmet-tcp kernel\n"
		       " modules and ensure that the kernel user configuration filesystem is mounted.\n");
		return NULL;
	}

	if ((err = scandir(SYS_NVMET_PORTS, &ports, NULL, NULL)) < 0) {
		printf("%s: scandir: %s\n", __func__, strerror(errno));
		return NULL;
	}
	for (; n < err; n++) {
		if (strncmp(ports[n]->d_name, ".", 1) != SUCCESS) {
			memset(file, 0x0, NAMELEN);
			sprintf(file, "%s/%s", SYS_NVMET_PORTS, ports[n]->d_name);
			if (access(file, F_OK) != INVALID_VALUE) {
				sprintf(file, "%s/%s/subsystems", SYS_NVMET_PORTS, ports[n]->d_name);
				if ((err2 = scandir(file, &exports, NULL, NULL)) < 0) {
					printf("%s: scandir: %s\n", __func__, strerror(errno));
					return NULL;
				}
				for (i = 0; i < err2; i++) {
					if (strncmp(exports[i]->d_name, ".", 1) != SUCCESS) {
						if ((nvmet_ports = (struct NVMET_PORTS *)calloc(1, sizeof(struct NVMET_PORTS))) == NULL ) {
							printf("%s: calloc: %s\n", __func__, strerror(errno));
							return NULL;
						}
						nvmet_ports->port = atoi(ports[n]->d_name);
						sprintf(file, "%s/%s", SYS_NVMET_PORTS, ports[n]->d_name);
						sprintf(nvmet_ports->addr, "%s", read_info(file, "addr_traddr"));
						if (strlen(nvmet_ports->addr) < 1)
							sprintf(nvmet_ports->addr, "UNDEFINED");
						sprintf(nvmet_ports->protocol, "%s", read_info(file, "addr_trtype"));
						if (strlen(nvmet_ports->protocol) < 1)
							sprintf(nvmet_ports->protocol, "UNDEFINED");
						sprintf(nvmet_ports->nqn, "%s", exports[i]->d_name);
						if (strlen(nvmet_ports->nqn) < 1)
							sprintf(nvmet_ports->nqn, "UNDEFINED");
						if (ports_head == NULL)
							ports_head = nvmet_ports;
						else
							ports_end->next = nvmet_ports;
						ports_end = nvmet_ports;
						nvmet_ports->next = NULL;
					}
					if (exports[i] != NULL) free(exports[i]);
				}
			}
		}
		if (ports[n] != NULL) free(ports[n]);
	}
	return ports_head;
}

/*
 * description: Scan all NVMe Targets ports.
 */
struct NVMET_PORTS *nvmet_scan_all_ports(void)
{
	int err, n = 0;
	unsigned char file[NAMELEN * 2] = {0};
	struct NVMET_PORTS *nvmet_ports;
	struct dirent **ports;

	if (access(SYS_NVMET, F_OK) != SUCCESS) {
		printf("The NVMe Target subsystem is not loaded. Please load the nvmet and nvmet-tcp kernel\n"
		       " modules and ensure that the kernel user configuration filesystem is mounted.\n");
		return NULL;
	}

	if ((err = scandir(SYS_NVMET_PORTS, &ports, NULL, NULL)) < 0) {
		printf("%s: scandir: %s\n", __func__, strerror(errno));
		return NULL;
	}
	for (; n < err; n++) {
		if (strncmp(ports[n]->d_name, ".", 1) != SUCCESS) {
			memset(file, 0x0, NAMELEN);
			sprintf(file, "%s/%s", SYS_NVMET_PORTS, ports[n]->d_name);
			if (access(file, F_OK) != INVALID_VALUE) {
				if ((nvmet_ports = (struct NVMET_PORTS *)calloc(1, sizeof(struct NVMET_PORTS))) == NULL ) {
					printf("%s: calloc: %s\n", __func__, strerror(errno));
					return NULL;
				}
				nvmet_ports->port = atoi(ports[n]->d_name);
				sprintf(nvmet_ports->addr, "%s", read_info(file, "addr_traddr"));
				if (strlen(nvmet_ports->addr) < 1)
					sprintf(nvmet_ports->addr, "UNDEFINED");
				sprintf(nvmet_ports->protocol, "%s", read_info(file, "addr_trtype"));
				if (strlen(nvmet_ports->protocol) < 1)
					sprintf(nvmet_ports->protocol, "UNDEFINED");
				if (ports_head == NULL)
					ports_head = nvmet_ports;
				else
					ports_end->next = nvmet_ports;
				ports_end = nvmet_ports;
				nvmet_ports->next = NULL;
			}
		}
		if (ports[n] != NULL) free(ports[n]);
	}
	return ports_head;
}

/*
 * description: List NVMe Target exports / ports.
 */
int nvmet_view_exports(bool json_flag)
{
	int i = 1;
	struct NVMET_PROFILE *nvmet, *tmp;
	struct NVMET_PORTS *ports, *tmp_ports;

	nvmet = (struct NVMET_PROFILE *)nvmet_scan_subsystem();
	ports = (struct NVMET_PORTS *)nvmet_scan_ports();

	if (json_flag == TRUE)
		return json_nvmet_view_exports(nvmet, ports);

	printf("NVMe Target Exports\n\n");
	if (nvmet == NULL) {
		printf("\tNone.\n\n");
	} else {
		while (nvmet != NULL) {
			printf("\t%d: NQN: %s \tNamespace: %d\tDevice: %s \tEnabled: %s\n",
			       i, nvmet->nqn, nvmet->namespc, nvmet->device,
			       ((nvmet->enabled == 0) ? "False" : "True"));
			i++;
			tmp = nvmet;
			nvmet = nvmet->next;
			free(tmp);
		}
	}

	i = 1;
	printf("\nExported NVMe Ports\n\n");
	if (ports == NULL) {
		printf("\tNone.\n\n");
		return SUCCESS;
	}

	while (ports != NULL) {
		printf("\t%d: Port: %d - %s (%s)\tNQN: %s\n", i, ports->port, ports->addr, ports->protocol, ports->nqn);
		i++;
		tmp_ports = ports;
		ports = ports->next;
		free(tmp_ports);
	}
	return SUCCESS;
}

/*
 * description: Export an NVMe Target.
 */
int nvmet_export_volume(struct RD_PROFILE *rd_prof, RC_PROFILE *rc_prof, unsigned char *device, unsigned char *host, int port)
{
	int rc = INVALID_VALUE, n, err;
	FILE *fp;
	mode_t mode = (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	unsigned char hostname[0x40] = {0x0}, path[NAMELEN] = {0x0}, path2[NAMELEN] = {0x0};
	struct dirent **list;

	/* Check to see if device exists */
	while (rd_prof != NULL) {
		if (strcmp(device, rd_prof->device) == SUCCESS)
			rc = SUCCESS;
		rd_prof = rd_prof->next;
	}
	while (rc_prof != NULL) {
		if (strcmp(device, rc_prof->device) == SUCCESS)
			rc = SUCCESS;
		rc_prof = rc_prof->next;
	}
        if (rc != SUCCESS) {
                printf("Error. Device %s does not exist.\n", device);
                return INVALID_VALUE;
        }

	/* Create NQN */
	gethostname(hostname, sizeof(hostname));
	sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		if ((rc = mkdir(path, mode)) != SUCCESS) {
			printf("Error. Unable to create target directory %s.\n%s: mkdir: %s\n", path, __func__, strerror(errno));
			return INVALID_VALUE;
		}
	}

	/* Set host NQNs to access target */
	if (strlen(host) == 0) {
		sprintf(path, "%s/%s%s-%s/allowed_hosts", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if ((err = scandir(path, &list, NULL, NULL)) < 0) {
			printf("Error. Unable to access %s.\n%s: scandir: %s\n", path, __func__, strerror(errno));
			return INVALID_VALUE;
		}
		for (n = 0; n < err; n++) if (list[n] != NULL) free(list[n]);    /* clear list */
		if (err > 2) {
			printf("One or more hosts exist. Please remove existing host or define a new one.\n");
			return INVALID_VALUE;
		}

		sprintf(path, "%s/%s%s-%s/attr_allow_any_host", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if((fp = fopen(path, "w")) == NULL){
			printf("Error. Unable to open %s.\n%s: fopen: %s\n", path, __func__, strerror(errno));
			return INVALID_VALUE;
		}
		fprintf(fp, "1");
		fclose(fp);
	} else {
		/* Configure the target to be seen only by the specified host(s) */

		sprintf(path, "%s/%s", SYS_NVMET_HOSTS, host);
		if (access(path, F_OK) != SUCCESS) {
			if ((rc = mkdir(path, mode)) != SUCCESS) {
				printf("Error. Unable to create host directory %s.\n%s: mkdir: %s\n", path, __func__, strerror(errno));
				return INVALID_VALUE;
			}
		}
		sprintf(path, "%s/%s", SYS_NVMET_HOSTS, host);
		sprintf(path2, "%s/%s%s-%s/allowed_hosts/%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device, host);

		if (access(path2, F_OK) != SUCCESS) {
			rc = symlink(path, path2);
			if (rc != SUCCESS) {
				printf("Error. Unable to link host to port.\n%s: symlink: %s\n", __func__, strerror(errno));
				return rc;
			}
		}

		/* Make sure that no other hosts can access the target */
		sprintf(path, "%s/%s%s-%s/attr_allow_any_host", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if((fp = fopen(path, "w")) == NULL){
			printf("Error. Unable to open %s.\n%s: fopen: %s\n", path, __func__, strerror(errno));
			return INVALID_VALUE;
		}
		fprintf(fp, "0");
		fclose(fp);
	}

	/* Set model */
	sprintf(path, "%s/%s%s-%s/attr_model", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) == SUCCESS) {
		if((fp = fopen(path, "w")) == NULL){
			printf("Error. Unable to set model string to file %s.\n%s: fopen: %s\n", path, __func__, strerror(errno));
			return INVALID_VALUE;
		}
		fprintf(fp, "RapidDisk");
		fclose(fp);
	}

	/* Create namespace */
	sprintf(path, "%s/%s%s-%s/namespaces/1", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		if ((rc = mkdir(path, mode)) != SUCCESS) {
			printf("Error. Unable to create namespace directory %s\n.%s: mkdir: %s\n", path, __func__, strerror(errno));
			return rc;
		}
	}

	/* Set device */
	sprintf(path, "%s/%s%s-%s/namespaces/1/device_path", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if((fp = fopen(path, "w")) == NULL){
		printf("Error. Unable to open device_path file: %s\n%s: fopen: %s\n", path, __func__, strerror(errno));
		return INVALID_VALUE;
	}
	if (strncmp(device, "rd", 2) == SUCCESS)
		fprintf(fp, "/dev/%s", device);
	else
		fprintf(fp, "/dev/mapper/%s", device);
	fclose(fp);

	/* Enable volume */
	sprintf(path, "%s/%s%s-%s/namespaces/1/enable", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if((fp = fopen(path, "w")) == NULL){
		printf("Error. Unable to open namespace enable file %s.\n%s: fopen: %s\n", path, __func__, strerror(errno));
		return INVALID_VALUE;
	}
	fprintf(fp, "1");
	fclose(fp);

	/* Set to a port */
	if (port != INVALID_VALUE) {
		sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		sprintf(path2, "%s/%d/subsystems/%s%s-%s", SYS_NVMET_PORTS, port, NQN_HDR_STR, hostname, device);
		if (access(path2, F_OK) != SUCCESS) {
			rc = symlink(path, path2);
			if (rc != SUCCESS) {
				printf("Error. Unable to create link of NQN to port.\n%s: symlink: %s\n", __func__, strerror(errno));
				return rc;
			}
		}
	} else {
		/* Iterate through all ports and enable target on each */
		if ((err = scandir(SYS_NVMET_PORTS, &list, NULL, NULL)) < 0) {
			printf("%s: scandir: %s\n", __func__, strerror(errno));
			return INVALID_VALUE;
		}
		for (n = 0; n < err; n++) {
			if (strncmp(list[n]->d_name, ".", 1) != SUCCESS) {
				sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
				sprintf(path2, "%s/%s/subsystems/%s%s-%s", SYS_NVMET_PORTS, list[n]->d_name, NQN_HDR_STR, hostname, device);
				if (access(path2, F_OK) != SUCCESS) {
					rc = symlink(path, path2);
					if (rc != SUCCESS) {
						printf("Error. Unable to create link of NQN to port.\n%s: symlink: %s\n", __func__, strerror(errno));
						return rc;
					}
				}
			}
			if (list[n] != NULL) free(list[n]);
		}
	}

	sprintf(path, "port %d", port);
	printf("Block device %s has been mapped to %s through %s as %s%s-%s\n", device, ((strlen(host) == 0) ? "all hosts" : (char *)host),
	       ((port == INVALID_VALUE) ? "all ports" : (char *)path), NQN_HDR_STR, hostname, device);

	return SUCCESS;
}

/*
 *
 */
int nvmet_revalidate_size(struct RD_PROFILE *rd_prof, RC_PROFILE *rc_prof, unsigned char *device)
{
	int rc = INVALID_VALUE, n, err;
	FILE *fp;
	unsigned char hostname[0x40] = {0x0}, path[NAMELEN] = {0x0};

	/* Check to see if device exists */
	while (rd_prof != NULL) {
		if (strcmp(device, rd_prof->device) == SUCCESS)
			rc = SUCCESS;
		rd_prof = rd_prof->next;
	}
	while (rc_prof != NULL) {
		if (strcmp(device, rc_prof->device) == SUCCESS)
			rc = SUCCESS;
		rc_prof = rc_prof->next;
	}
	if (rc != SUCCESS) {
		printf("Error. Device %s does not exist.\n", device);
		return INVALID_VALUE;
	} else
		rc = INVALID_VALUE;

	gethostname(hostname, sizeof(hostname));
	sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		sprintf(path, "%s%s-%s", NQN_HDR_STR, hostname, device);
		printf("Error. NQN export: %s does not exist.\n", path);
		return rc;
	}

	/* Check if namespace 1 exists. That is the only namespace we define. */
	sprintf(path, "%s/%s%s-%s/namespaces/1", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		printf("%s: A RapidDisk defined namespace does not exist.\n", __func__);
		return rc;
	}

	/* Check if the revalidate_size attribute exists. */
	sprintf(path, "%s/%s%s-%s/namespaces/1/revalidate_size", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		printf("%s: The kernel nvmet module version utilized does not support this function.\n", __func__);
		return rc;
	}

	if((fp = fopen(path, "w")) == NULL){
		printf("Error. Unable to open %s.\n%s: fopen: %s\n", path, __func__, strerror(errno));
		return rc;
	}
	fprintf(fp, "1");
	fclose(fp);

	printf("NVMe Target Namespace size for %s revalidated.\n", device);

	return SUCCESS;
}

/*
 * description: Unexport an NVMe Target.
 */
int nvmet_unexport_volume(unsigned char *device, unsigned char *host, int port)
{
	int rc = INVALID_VALUE, n, err;
	FILE *fp;
	unsigned char hostname[0x40] = {0x0}, path[NAMELEN] = {0x0}, path2[NAMELEN] = {0x0};
	struct dirent **list, **sublist;

	gethostname(hostname, sizeof(hostname));
	sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		sprintf(path, "%s%s-%s", NQN_HDR_STR, hostname, device);
		printf("Error. NQN export: %s does not exist.\n", path);
		return rc;
	}

	/* Check if namespace 1 exists. That is the only namespace we define. */
	sprintf(path, "%s/%s%s-%s/namespaces/1", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		printf("%s: A RapidDisk defined namespace does not exist.\n", __func__);
		return rc;
	}

	/* Make sure that no other namespaces exist. We do not create anything higher than 1. */
	sprintf(path, "%s/%s%s-%s/namespaces/", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if ((err = scandir(path, &list, NULL, NULL)) < 0) {
		printf("Error. Unable to access %s.\n%s: scandir: %s\n", path, __func__, strerror(errno));
		return INVALID_VALUE;
	}
	for (n = 0; n < err; n++) if (list[n] != NULL) free(list[n]);    /* clear list */
	if (err > 3) {
		printf("An invalid number of namespaces not created by RapidDisk exist.\n");
		return rc;
	}

	if (strlen(host) != 0) {
		sprintf(path, "%s/%s%s-%s/allowed_hosts/%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device, host);
		if (access(path, F_OK) == SUCCESS) {
			rc = unlink(path);
			if (rc != SUCCESS) {
				printf("Error. Unable to remove host.\n%s: unlink: %s\n", __func__, strerror(errno));
				return rc;
			}
        		printf("Block device %s has been unmapped from NVMe Target host %s.\n", device, host);
		} else {
			printf("%s: Host %s does not exist.\n", __func__, host);
			return rc;
		}
		/* If a host is defined without any port number, just remove and exit. */
		if (port == INVALID_VALUE)
			return SUCCESS;
	} else {
		/* If no host is defined, remove all hosts */
		sprintf(path, "%s/%s%s-%s/allowed_hosts/", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if ((err = scandir(path, &list, NULL, NULL)) < 0) {
			goto host_check_out;
		}
		for (n = 0; n < err; n++) {
			if (strncmp(list[n]->d_name, ".", 1) != SUCCESS) {
				sprintf(path, "%s/%s%s-%s/allowed_hosts/%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device, list[n]->d_name);
				if (access(path, F_OK) == SUCCESS) {
					rc = unlink(path);
					if (rc != SUCCESS) {
						printf("Error. Unable to remove hosts.\n%s: unlink: %s\n", __func__, strerror(errno));
						return rc;
					}
				}
			}
			if (list[n] != NULL) free(list[n]);
		}
        	printf("Block device %s has been unmapped from all NVMe Target hosts.\n", device);
	}

host_check_out:

	if (port != INVALID_VALUE) {
		sprintf(path, "%s/%d/subsystems/%s%s-%s", SYS_NVMET_PORTS, port, NQN_HDR_STR, hostname, device);
		if (access(path, F_OK) == SUCCESS) {
			rc = unlink(path);
			if (rc != SUCCESS) {
				printf("Error. Unable to remove NQN from port.\n%s: unlink: %s\n", __func__, strerror(errno));
				return rc;
			}
        		printf("Block device %s has been unmapped from NVMe Target port %d.\n", device, port);
		} else {
			printf("%s: Port %d and / or export does not exist.\n", __func__, port);
			return rc;
		}
	} else {
		/* If no port is defined, unmap from all ports */
		sprintf(path, "%s", SYS_NVMET_PORTS);
		if ((err = scandir(path, &list, NULL, NULL)) < 0)
			goto port_check_out;
		for (n = 0; n < err; n++) {
			if (strncmp(list[n]->d_name, ".", 1) != SUCCESS) {
				sprintf(path, "%s/%s/subsystems/%s%s-%s", SYS_NVMET_PORTS, list[n]->d_name, NQN_HDR_STR, hostname, device);
				if (access(path, F_OK) == SUCCESS) {
					rc = unlink(path);
					if (rc != SUCCESS) {
						printf("Error. Unable to remove NQN from ports.\n%s: unlink: %s\n", __func__, strerror(errno));
						return rc;
					}
				}
			}
			if (list[n] != NULL) free(list[n]);
		}
        	printf("Block device %s has been unmapped from all NVMe Target ports.\n", device);
	}

port_check_out:

	/* If a port and / or host are defined, just remove / unmap and exit. */
	if ((strlen(host) == 0) && (port == INVALID_VALUE)) {
		if (access(path, F_OK) == SUCCESS) {
			/* Disable volume */
			sprintf(path, "%s/%s%s-%s/namespaces/1/enable", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
			if((fp = fopen(path, "w")) == NULL){
				printf("Error. Unable to open namespace enable file %s.\n%s: fopen: %s\n", path, __func__, strerror(errno));
				return INVALID_VALUE;
			}
			fprintf(fp, "0");
			fclose(fp);
		}

		sprintf(path, "%s/%s%s-%s/namespaces/1", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if (access(path, F_OK) == SUCCESS) {
			if ((rc = rmdir(path)) != SUCCESS) {
				printf("Error. Unable to remove namespace %s.\n%s: rmdir: %s\n", path, __func__, strerror(errno));
				return rc;
			}
		}

		/* Remove NQN from NVMeT Subsystem */
		sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if (access(path, F_OK) == SUCCESS) {
			if ((rc = rmdir(path)) != SUCCESS) {
				printf("Error. Unable to remove NQN from NVMe Target subsystem %s.\n%s: rmdir: %s\n", path, __func__, strerror(errno));
				return rc;
			}
		}
	}

        printf("Block device %s has been removed from the NVMe Target subsystem.\n", device);

	return SUCCESS;
}

int number_validate(unsigned char *str)
{
	while (*str) {
		if (!isdigit(*str))
			return INVALID_VALUE;
		str++;
	}

	return SUCCESS;
}

int ip_validate(unsigned char *ip)
{
	int i, num, dots = 0;
	unsigned char *ptr, *ip_dup;

	ip_dup = strdup(ip);

	if (ip_dup == NULL)
		return INVALID_VALUE;
	ptr = strtok(ip_dup, ".");
	if (ptr == NULL)
		return INVALID_VALUE;

	while (ptr) {
		if (number_validate(ptr) != SUCCESS)
			return INVALID_VALUE;
		num = atoi(ptr);
		if (num >= 0 && num <= 255) {
			ptr = strtok(NULL, ".");
			if (ptr != NULL)
				dots++;
		} else
			return INVALID_VALUE;
	}
	if (dots != 3)
		return INVALID_VALUE;

	return SUCCESS;
}

unsigned char *nvmet_interface_ip_get(unsigned char *interface)
{
	int fd;
	struct ifreq ifr;
	static unsigned char ip[0xF] = {0};

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < SUCCESS) {
		printf("%s: open: %s\n", __func__, strerror(errno));
		return NULL;
	}

	/* I want to get an IPv4 IP address */
	ifr.ifr_addr.sa_family = AF_INET;

	/* I want the IP address attached to the interface (ex: eth0 or eno1) */
	strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);

	if (ioctl(fd, SIOCGIFADDR, &ifr) == INVALID_VALUE) {
		printf("%s: ioctl: %s\n", __func__, strerror(errno));
		return NULL;
	}
	close(fd);

	sprintf(ip, "%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

	return ip;
}


/*
 * description: List all enabled NVMe Target ports.
 */
int nvmet_view_ports(bool json_flag)
{
        int i = 1;
	struct NVMET_PORTS *ports, *tmp_ports;

	ports = (struct NVMET_PORTS *)nvmet_scan_all_ports();

	if (json_flag == TRUE)
		return json_nvmet_view_ports(ports);

	printf("Exported NVMe Ports\n\n");
	if (ports == NULL) {
		printf("\tNone.\n\n");
		return SUCCESS;
	}

	while (ports != NULL) {
		printf("\t%d: Port: %d - %s (%s)\n", i, ports->port, ports->addr, ports->protocol);
		i++;
		tmp_ports = ports;
		ports = ports->next;
		free(tmp_ports);
	}

	return SUCCESS;
}

int nvmet_enable_port(unsigned char *interface, int port, int protocol)
{
        int rc = INVALID_VALUE;
	FILE *fp;
        unsigned char path[NAMELEN] = {0x0}, ip[0xF] = {0x0}, proto[0x5] = {0x0};
	mode_t mode = (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	struct NVMET_PORTS *ports, *tmp_ports;
        
	ports = (struct NVMET_PORTS *)nvmet_scan_all_ports();

	sprintf(path, "%s/%d", SYS_NVMET_PORTS, port);
	if (access(path, F_OK) == SUCCESS) {
		printf("Error. NVMe Target Port %d already exists.\n", port);
		return rc;
	}

	sprintf(ip, "%s", nvmet_interface_ip_get(interface));

	if (ip_validate(ip) != SUCCESS) {
		printf("Error. IP address %s is invalid.\n", ip);
		return rc;
	}
	
        while (ports != NULL) {
		if (strcmp(ports->addr, ip) == SUCCESS) {
			printf("Error. Interface %s with IP address %s is already in use on port %d.\n", interface, ip, ports->port);
			return rc;
		}
                tmp_ports = ports;
                ports = ports->next;
                free(tmp_ports);
        }

	if ((rc = mkdir(path, mode)) != SUCCESS) {
		printf("Error. Unable to create port directory.\n%s: mkdir: %s\n", __func__, strerror(errno));
		return INVALID_VALUE;
	}

	sprintf(path, "%s/%d/addr_trsvcid", SYS_NVMET_PORTS, port);
	if((fp = fopen(path, "w")) == NULL){
		printf("Error. Unable to open %s.\n%s: fopen: %s\n", path, __func__, strerror(errno));
		return INVALID_VALUE;
	}
	fprintf(fp, "4420");
	fclose(fp);

	sprintf(path, "%s/%d/addr_adrfam", SYS_NVMET_PORTS, port);
	if((fp = fopen(path, "w")) == NULL){
		printf("Error. Unable to open %s.\n%s: fopen: %s\n", path, __func__, strerror(errno));
		return INVALID_VALUE;
	}
	fprintf(fp, "ipv4");
	fclose(fp);

	if (protocol == XFER_MODE_RDMA)
		sprintf(proto, "rdma");
	else
		sprintf(proto, "tcp");
	sprintf(path, "%s/%d/addr_trtype", SYS_NVMET_PORTS, port);
	if((fp = fopen(path, "w")) == NULL){
		printf("Error. Unable to open %s.\n%s: fopen: %s\n", path, __func__, strerror(errno));
		return INVALID_VALUE;
	}
	fprintf(fp, "%s", proto);
	fclose(fp);

	sprintf(path, "%s/%d/addr_traddr", SYS_NVMET_PORTS, port);
	if((fp = fopen(path, "w")) == NULL){
		printf("Error. Unable to open %s.\n%s: fopen: %s\n", path, __func__, strerror(errno));
		return INVALID_VALUE;
	}
	fprintf(fp, "%s", ip);
	fclose(fp);

	printf("Successfully created port %d set to %s for interface %s (with IP address %s).\n", port, proto, interface, ip); 

	return SUCCESS;
}

int nvmet_disable_port(int port)
{
	int rc = INVALID_VALUE, n, err;
	unsigned char path[NAMELEN] = {0x0};
	struct dirent **list;

	sprintf(path, "%s/%d", SYS_NVMET_PORTS, port);
	if (access(path, F_OK) != SUCCESS) {
		printf("Error. NVMe Target Port %d does not exist.\n", port);
		return rc;
	}

	/* Make sure that no subsystems are mapped from this port. */
        sprintf(path, "%s/%d/subsystems/", SYS_NVMET_PORTS, port);
        if ((err = scandir(path, &list, NULL, NULL)) < 0) {
                printf("Error. Unable to access %s.\n%s: scandir: %s\n", path, __func__, strerror(errno));
                return rc;
        }
        for (n = 0; n < err; n++) if (list[n] != NULL) free(list[n]);    /* clear list */
        if (err > 2) {
                printf("This port is currently in use.\n");
                return rc;
        }

	/* Remove the port */
	sprintf(path, "%s/%d", SYS_NVMET_PORTS, port);
	if ((rc = rmdir(path)) != SUCCESS) {
		printf("Error. Unable to remove port.\n%s: rmdir: %s\n", __func__, strerror(errno));
		return rc;
	}

	printf("NVMe Target port %d has been removed.\n", port);

	return SUCCESS;
}
