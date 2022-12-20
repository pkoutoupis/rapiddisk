/**
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

#include "nvmet.h"
#include "utils.h"
#include "rdsk.h"
#include "json.h"

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

struct NVMET_PROFILE *nvmet_head = NULL;
struct NVMET_PROFILE *nvmet_end = NULL;

struct NVMET_PORTS *ports_head = NULL;
struct NVMET_PORTS *ports_end = NULL;

/*
 * description: Scan all NVMe Targets NQNs
 */

/**
 * It scans the NVMe Target subsystem for all the subsystems and namespaces and returns a linked list of the subsystems and
 * namespaces.
 *
 * @param return_message This is a pointer to a string that will be used to return error messages.
 *
 * @return A linked list of NVMET_PROFILE structs.
 */
struct NVMET_PROFILE *nvmet_scan_subsystem(char *return_message)
{
	int err, err2, n = 0, i;
	char file[NAMELEN * 2] = {0};
	struct NVMET_PROFILE *nvmet = NULL;
	struct dirent **list, **sublist;
	nvmet_head = NULL;
	nvmet_end = NULL;
	char *msg;

	if (access(SYS_NVMET, F_OK) != SUCCESS) {
		msg = "The NVMe Target subsystem is not loaded. Please load the nvmet and nvmet-tcp kernel modules and ensure that the kernel user configuration filesystem is mounted.";
		print_error("%s", return_message, msg);
		return NULL;
	}

	if ((err = scandir(SYS_NVMET_TGT, &list, NULL, NULL)) < 0) {
		msg = "%s: scandir: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return NULL;
	}
	for (; n < err; n++) {
		if (strncmp(list[n]->d_name, ".", 1) != SUCCESS) {
			sprintf(file, "%s/%s/namespaces/", SYS_NVMET_TGT, list[n]->d_name);
			if ((err2 = scandir(file, &sublist, NULL, NULL)) < 0) {
				msg = "%s: scandir: %s";
				print_error(msg, return_message, __func__, strerror(errno));
				list = clean_scandir(list, err);
				return NULL;
			}
			for (i = 0; i < err2; i++) {
				if (strncmp(sublist[i]->d_name, ".", 1) != SUCCESS) {
					if ((nvmet = (struct NVMET_PROFILE *)calloc(1, sizeof(struct NVMET_PROFILE))) == NULL ) {
						msg = ERR_CALLOC;
						print_error(msg, return_message, __func__, strerror(errno));
						list = clean_scandir(list, err);
						sublist = clean_scandir(sublist, err2);
						return NULL;
					}
					strcpy(nvmet->nqn, (char *)list[n]->d_name);

					sprintf(file, "%s/%s/namespaces/%s", SYS_NVMET_TGT, list[n]->d_name, sublist[i]->d_name);
					if (access(file, F_OK) != INVALID_VALUE) {
						char *info_device = read_info(file, "device_path", return_message);
						if (info_device == NULL) {
							free(nvmet);
							nvmet = NULL;
							list = clean_scandir(list, err);
							sublist = clean_scandir(sublist, err2);
							free_nvmet_linked_lists(NULL, nvmet_head);
							nvmet_head = NULL;
							return NULL;
						}
						sprintf(nvmet->device, "%s", info_device);
						info_device = read_info(file, "enable", return_message);
						if (info_device == NULL) {
							free(nvmet);
							nvmet = NULL;
							list = clean_scandir(list, err);
							sublist = clean_scandir(sublist, err2);
							free_nvmet_linked_lists(NULL, nvmet_head);
							nvmet_head = NULL;
							return NULL;
						}
						nvmet->enabled = atoi(info_device);
						nvmet->namespc = atoi(sublist[i]->d_name);
					}
					if (nvmet_head == NULL)
						nvmet_head = nvmet;
					else
						nvmet_end->next = nvmet;
					nvmet_end = nvmet;
					nvmet->next = NULL;
				}
			}
			sublist = clean_scandir(sublist, err2);
		}
	}
	list = clean_scandir(list, err);
	return nvmet_head;
}

/*
 * description: Scan all exported NVMe Targets ports.
 */

/**
 * It scans the NVMe Target subsystem for all ports and exports and returns a linked list of all ports and exports
 *
 * @param return_message This is a pointer to a string that will be populated with an error message if the function fails.
 *
 * @return A pointer to the head of a linked list of structs.
 */
struct NVMET_PORTS *nvmet_scan_ports(char *return_message)
{
	int err, err2, n = 0, i;
	char file[NAMELEN * 2] = {0};
	struct NVMET_PORTS *nvmet_ports = NULL;
	struct dirent **ports, **exports;
	ports_head = NULL;
	ports_end = NULL;
	char *msg;

	if (access(SYS_NVMET, F_OK) != SUCCESS) {
		msg = "The NVMe Target subsystem is not loaded. Please load the nvmet and nvmet-tcp kernel modules and ensure that the kernel user configuration filesystem is mounted.";
		print_error("%s", return_message, msg);
		return NULL;
	}

	if ((err = scandir(SYS_NVMET_PORTS, &ports, NULL, NULL)) < 0) {
		msg = "%s: scandir: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return NULL;
	}
	for (; n < err; n++) {
		if (strncmp(ports[n]->d_name, ".", 1) != SUCCESS) {
			memset(file, 0x0, NAMELEN);
			sprintf(file, "%s/%s", SYS_NVMET_PORTS, ports[n]->d_name);
			if (access(file, F_OK) != INVALID_VALUE) {
				sprintf(file, "%s/%s/subsystems", SYS_NVMET_PORTS, ports[n]->d_name);
				if ((err2 = scandir(file, &exports, NULL, NULL)) < 0) {
					msg = "%s: scandir: %s";
					print_error(msg, return_message, __func__, strerror(errno));
					ports = clean_scandir(ports, err);
					return NULL;
				}
				for (i = 0; i < err2; i++) {
					if (strncmp(exports[i]->d_name, ".", 1) != SUCCESS) {
						if ((nvmet_ports = (struct NVMET_PORTS *)calloc(1, sizeof(struct NVMET_PORTS))) == NULL ) {
							msg = ERR_CALLOC;
							print_error(msg, return_message, __func__, strerror(errno));
							ports = clean_scandir(ports, err);
							exports = clean_scandir(exports, err2);
							return NULL;
						}
						nvmet_ports->port = atoi(ports[n]->d_name);
						sprintf(file, "%s/%s", SYS_NVMET_PORTS, ports[n]->d_name);
						char *info = read_info(file, "addr_traddr", return_message);
						if (info == NULL) {
							free(nvmet_ports);
							nvmet_ports = NULL;
							ports = clean_scandir(ports, err);
							exports = clean_scandir(exports, err2);
							free_nvmet_linked_lists(ports_head, NULL);
							return NULL;
						}
						sprintf(nvmet_ports->addr, "%s", info);
						if (strlen(nvmet_ports->addr) < 1)
							sprintf(nvmet_ports->addr, "UNDEFINED");
						info = read_info(file, "addr_trtype", return_message);
						if (info == NULL) {
							free(nvmet_ports);
							nvmet_ports = NULL;
							ports = clean_scandir(ports, err);
							exports = clean_scandir(exports, err2);
							free_nvmet_linked_lists(ports_head, NULL);
							return NULL;
						}
						sprintf(nvmet_ports->protocol, "%s", info);
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
				}
				exports = clean_scandir(exports, err2);
			}
		}
	}
	ports = clean_scandir(ports, err);
	return ports_head;
}

/*
 * description: Scan all NVMe Targets ports.
 */

/**
 * > This function scans the /sys/kernel/config/nvmet/ports directory for all NVMe Target ports and returns a linked list
 * of all ports found
 *
 * @param return_message This is a pointer to a string that will be populated with an error message if the function fails.
 *
 * @return A pointer to the first element of a linked list of struct NVMET_PORTS.
 */
struct NVMET_PORTS *nvmet_scan_all_ports(char *return_message)
{
	int err, n = 0;
	char file[NAMELEN * 2] = {0};
	struct NVMET_PORTS *nvmet_ports;
	struct dirent **ports;
	ports_head = NULL;
	ports_end = NULL;
	char *msg;

	if (access(SYS_NVMET, F_OK) != SUCCESS) {
		msg = "The NVMe Target subsystem is not loaded. Please load the nvmet and nvmet-tcp kernel modules and ensure that the kernel user configuration filesystem is mounted.";
		print_error("%s", return_message, msg);
		return NULL;
	}

	if ((err = scandir(SYS_NVMET_PORTS, &ports, NULL, NULL)) < 0) {
		msg = "%s: scandir: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return NULL;
	}
	for (; n < err; n++) {
		if (strncmp(ports[n]->d_name, ".", 1) != SUCCESS) {
			memset(file, 0x0, NAMELEN);
			sprintf(file, "%s/%s", SYS_NVMET_PORTS, ports[n]->d_name);
			if (access(file, F_OK) != INVALID_VALUE) {
				if ((nvmet_ports = (struct NVMET_PORTS *)calloc(1, sizeof(struct NVMET_PORTS))) == NULL ) {
					msg = ERR_CALLOC;
					print_error(msg, return_message, __func__, strerror(errno));
					ports = clean_scandir(ports, err);
					return NULL;
				}
				nvmet_ports->port = atoi(ports[n]->d_name);
				char *info = read_info(file, "addr_traddr", return_message);
				if (info == NULL) {
					free(nvmet_ports);
					nvmet_ports = NULL;
					ports = clean_scandir(ports, err);
					free_nvmet_linked_lists(ports_head, NULL);
					ports_head = NULL;
					return NULL;
				}
				sprintf(nvmet_ports->addr, "%s", info);
				if (strlen(nvmet_ports->addr) < 1)
					sprintf(nvmet_ports->addr, "UNDEFINED");
				info = read_info(file, "addr_trtype", return_message);
				if (info == NULL) {
					free(nvmet_ports);
					nvmet_ports = NULL;
					ports = clean_scandir(ports, err);
					free_nvmet_linked_lists(ports_head, NULL);
					ports_head = NULL;
					return NULL;
				}
				sprintf(nvmet_ports->protocol, "%s", info);
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
	}
	ports = clean_scandir(ports, err);
	return ports_head;
}

/**
 * If the string is not a number, return INVALID_VALUE, otherwise return SUCCESS.
 *
 * @param str The string to validate.
 *
 * @return the value of the variable "number_validate".
 */
int number_validate(char *str)
{
	while (*str) {
		if (!isdigit(*str))
			return INVALID_VALUE;
		str++;
	}

	return SUCCESS;
}

/**
 * It takes a string as input and returns SUCCESS if the string is a valid IP address, else it returns INVALID_VALUE
 *
 * @param ip The IP address to validate.
 *
 * @return the value of the variable "dots".
 */
int ip_validate(char *ip)
{
	int num, dots = 0;
	char *ptr, *ip_dup;

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

	if (ip_dup) free(ip_dup);

	return SUCCESS;
}

/**
 * It gets the IP address of a given interface
 *
 * @param interface the interface name (ex: eth0 or eno1)
 * @param return_message a pointer to a string that will be filled with the error message if the function fails.
 *
 * @return The IP address of the interface.
 */
char *nvmet_interface_ip_get(char *interface, char *return_message)
{
	int fd;
	struct ifreq ifr;
	static char ip[0xF] = {0};
	char path[NAMELEN] = {0};
	char *msg;
	struct in_addr addr;

	sprintf(path, "%s/%s", SYS_CLASS_NET, interface);
	if (access(path, F_OK) != SUCCESS) {
		msg = "%s: access: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return NULL;
	}

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < SUCCESS) {
		msg = "%s: open: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return NULL;
	}

	/* I want to get an IPv4 IP address */
	ifr.ifr_addr.sa_family = AF_INET;

	/* I want the IP address attached to the interface (ex: eth0 or eno1) */
	strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);

	if (ioctl(fd, SIOCGIFADDR, &ifr) == INVALID_VALUE) {
		msg = "%s: ioctl: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		close(fd);
		return NULL;
	}
	close(fd);

	/* Some more checks to addr.s_addr may be added here */
	addr = ( (struct sockaddr_in *) &ifr.ifr_addr)->sin_addr;
	sprintf(ip, "%s", inet_ntoa(addr));

	return ip;
}

#ifndef SERVER
/*
 * description: List NVMe Target exports / ports.
 */


/**
 * This function scans the NVMe subsystems and ports and prints the results to the screen
 *
 * @param json_flag This is a boolean value that indicates whether the output should be in JSON format or not.
 * @param error_message This is a pointer to a string that will be populated with an error message if the function fails.
 */
int nvmet_view_exports(bool json_flag, char *error_message)
{
	int i = 1;
	int rc = SUCCESS;
	struct NVMET_PROFILE *nvmet, *tmp;
	struct NVMET_PORTS *ports, *tmp_ports;

	nvmet = nvmet_scan_subsystem(error_message);
	if ((nvmet == NULL) && (strlen(error_message) != 0)) {
		return INVALID_VALUE;
	}

	ports = nvmet_scan_ports(error_message);
	if ((ports == NULL) && (strlen(error_message) != 0)) {
		free_nvmet_linked_lists(ports, NULL);
		ports = NULL;
		return INVALID_VALUE;
	}

	if (json_flag == TRUE) {
		rc = json_nvmet_view_exports(nvmet, ports, NULL, FALSE);
		free_nvmet_linked_lists(ports, nvmet);
		ports = NULL;
		nvmet = NULL;
		return rc;
	}
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
	free_nvmet_linked_lists(ports, nvmet);
	ports = NULL;
	nvmet = NULL;
	return SUCCESS;
}

/*
 * description: Export an NVMe Target.
 */


/**
 * This function exports a block device to the NVMe-oF subsystem
 *
 * @param rd_prof A pointer to the RapidDisk profile linked list.
 * @param rc_prof This is a pointer to the RapidCache profile linked list.
 * @param device The device name of the volume to export.
 * @param host The hostname of the client that will be accessing the volume. If this is set to an empty string, then the
 * volume will be accessible by all hosts.
 * @param port The port number to export the volume to. If this is set to INVALID_VALUE, then the volume will be exported
 * to all ports.
 * @param return_message This is a pointer to a character array that will be populated with the return message.
 *
 * @return The return value is the return code of the function.
 */
int nvmet_export_volume(struct RD_PROFILE *rd_prof, RC_PROFILE *rc_prof, char *device, char *host, int port, char *return_message)
{
	int rc = INVALID_VALUE, n, err;
	FILE *fp;
	mode_t mode = (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	char hostname[0x40] = {0x0}, path[NAMELEN] = {0x0}, path2[NAMELEN] = {0x0};
	struct dirent **list;
	char *msg;
	NVMET_PROFILE *nvmet = NULL;
	NVMET_PROFILE *nvmet_orig = NULL;
	char error_message[NAMELEN] = {0};
	char full_device_name[NAMELEN] = {0};

	nvmet = nvmet_scan_subsystem(error_message);
	if ((nvmet == NULL) && (strlen(error_message) != 0)) {
		msg = "%s";
		print_error(msg, return_message, error_message);
		return INVALID_VALUE;
	}

	nvmet_orig = nvmet;

	sprintf(full_device_name, "/dev/%s", device);

	while (nvmet != NULL) {
		if (strcmp(full_device_name, nvmet->device) == SUCCESS) {
			rc = SUCCESS;
			break;
		}
		nvmet = nvmet->next;
	}

	free_nvmet_linked_lists(NULL, nvmet_orig);

	if (rc == SUCCESS) {
		msg = "Error: device %s already in use.";
		print_error(msg, return_message, device);
		return INVALID_VALUE;
	}

	/* Check to see if device exists */
	while (rd_prof != NULL) {
		if (strcmp(device, rd_prof->device) == SUCCESS) {
			rc = SUCCESS;
			break;
		}
		rd_prof = rd_prof->next;
	}

	while (rc_prof != NULL) {
		if (strcmp(device, rc_prof->device) == SUCCESS) {
			rc = SUCCESS;
			break;
		}
		rc_prof = rc_prof->next;
	}

	if (rc != SUCCESS) {
		msg = "Error. Device %s does not exist";
		print_error(msg, return_message, device);
		return INVALID_VALUE;
	}

	/* Create NQN */
	gethostname(hostname, sizeof(hostname));
	sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		if ((rc = mkdir(path, mode)) != SUCCESS) {
			msg = "Error. Unable to create target directory %s. %s: mkdir: %s";
			print_error(msg, return_message, path, __func__, strerror(errno));
			return rc;
		}
	}

	/* Set host NQNs to access target */
	if (strlen(host) == 0) {
		sprintf(path, "%s/%s%s-%s/allowed_hosts", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if ((err = scandir(path, &list, NULL, NULL)) < 0) {
			msg = "Error. Unable to access %s. %s: scandir: %s";
			print_error(msg, return_message, path, __func__, strerror(errno));
			return INVALID_VALUE;
		}

		list = clean_scandir(list, err);

		if (err > 2) {
			msg = "One or more hosts exist. Please remove existing host or define a new one.";
			print_error("%s", return_message, msg);
			return INVALID_VALUE;
		}

		sprintf(path, "%s/%s%s-%s/attr_allow_any_host", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if ((fp = fopen(path, "w")) == NULL){
			msg = "Error. Unable to open %s. %s: fopen: %s";
			print_error(msg, return_message, path, __func__, strerror(errno));
			return INVALID_VALUE;
		}
		fprintf(fp, "1");
		fclose(fp);
	} else {
		/* Configure the target to be seen only by the specified host(s) */

		sprintf(path, "%s/%s", SYS_NVMET_HOSTS, host);
		if (access(path, F_OK) != SUCCESS) {
			if ((rc = mkdir(path, mode)) != SUCCESS) {
				msg = "Error. Unable to create host directory %s. %s: mkdir: %s";
				print_error(msg, return_message, path, __func__, strerror(errno));
				return rc;
			}
		}
		sprintf(path, "%s/%s", SYS_NVMET_HOSTS, host);
		sprintf(path2, "%s/%s%s-%s/allowed_hosts/%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device, host);

		if (access(path2, F_OK) != SUCCESS) {
			rc = symlink(path, path2);
			if (rc != SUCCESS) {
				msg = "Error. Unable to link host to port. %s: symlink: %s";
				print_error(msg, return_message, __func__, strerror(errno));
				return rc;
			}
		}

		/* Make sure that no other hosts can access the target */
		sprintf(path, "%s/%s%s-%s/attr_allow_any_host", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if ((fp = fopen(path, "w")) == NULL){
			msg = "Error. Unable to open %s. %s: fopen: %s";
			print_error(msg, return_message, path, __func__, strerror(errno));
			return INVALID_VALUE;
		}
		fprintf(fp, "0");
		fclose(fp);
	}

	/* Set model */
	sprintf(path, "%s/%s%s-%s/attr_model", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) == SUCCESS) {
		if ((fp = fopen(path, "w")) == NULL){
			msg = "Error. Unable to set model string to file %s. %s: fopen: %s";
			print_error(msg, return_message, path, __func__, strerror(errno));
			return INVALID_VALUE;
		}
		fprintf(fp, "RapidDisk");
		fclose(fp);
	}

	/* Create namespace */
	sprintf(path, "%s/%s%s-%s/namespaces/1", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		if ((rc = mkdir(path, mode)) != SUCCESS) {
			msg = "Error. Unable to create namespace directory %s. %s: mkdir: %s";
			print_error(msg, return_message, path, __func__, strerror(errno));
			return rc;
		}
	}

	/* Set device */
	sprintf(path, "%s/%s%s-%s/namespaces/1/device_path", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if ((fp = fopen(path, "w")) == NULL){
		msg = "Error. Unable to open device_path file: %s. %s: fopen: %s";
		print_error(msg, return_message, path, __func__, strerror(errno));
		return INVALID_VALUE;
	}
	if (strncmp(device, "rd", 2) == SUCCESS)
		fprintf(fp, "/dev/%s", device);
	else
		fprintf(fp, "/dev/mapper/%s", device);
	fclose(fp);

	/* Enable volume */
	sprintf(path, "%s/%s%s-%s/namespaces/1/enable", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if ((fp = fopen(path, "w")) == NULL){
		msg = "Error. Unable to open namespace enable file %s. %s: fopen: %s";
		print_error(msg, return_message, path, __func__, strerror(errno));
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
				msg = "Error. Unable to create link of NQN to port. %s: symlink: %s";
				print_error(msg, return_message, __func__, strerror(errno));
				return rc;
			}
		}
	} else {
		/* Iterate through all ports and enable target on each */
		if ((err = scandir(SYS_NVMET_PORTS, &list, NULL, NULL)) < 0) {
			msg = "%s: scandir: %s";
			print_error(msg, return_message, __func__, strerror(errno));
			return INVALID_VALUE;
		}
		for (n = 0; n < err; n++) {
			if (strncmp(list[n]->d_name, ".", 1) != SUCCESS) {
				sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
				sprintf(path2, "%s/%s/subsystems/%s%s-%s", SYS_NVMET_PORTS, list[n]->d_name, NQN_HDR_STR, hostname, device);
				if (access(path2, F_OK) != SUCCESS) {
					rc = symlink(path, path2);
					if (rc != SUCCESS) {
						msg = "Error. Unable to create link of NQN to port. %s: symlink: %s";
						print_error(msg, return_message, __func__, strerror(errno));
						list = clean_scandir(list, err);
						return rc;
					}
				}
			}
		}
		list = clean_scandir(list, err);
	}

	sprintf(path, "port %d", port);
	msg = "Block device %s has been mapped to %s through %s as %s%s-%s";
	print_error(msg, return_message, device, ((strlen(host) == 0) ? "all hosts" : (char *)host), ((port == INVALID_VALUE) ? "all ports" : (char *)path), NQN_HDR_STR, hostname, device);
	return SUCCESS;
}

/**
 * It writes a 1 to the revalidate_size attribute of the namespace
 *
 * @param rd_prof A pointer to the RapidDisk profile list.
 * @param rc_prof This is the RC_PROFILE structure that contains the RapidCache device information.
 * @param device The device name.
 * @param return_message This is a pointer to a buffer that will contain the return message.
 *
 * @return The return code is the return code from the function.
 */
int nvmet_revalidate_size(struct RD_PROFILE *rd_prof, RC_PROFILE *rc_prof, char *device, char *return_message)
{
	int rc = INVALID_VALUE;
	FILE *fp;
	char hostname[0x40] = {0x0}, path[NAMELEN] = {0x0};
	char *msg;

	/* Check to see if device exists */
	while (rd_prof != NULL) {
		if (strcmp(device, rd_prof->device) == SUCCESS) {
			rc = SUCCESS;
			break;
		}
		rd_prof = rd_prof->next;
	}
	while (rc_prof != NULL) {
		if (strcmp(device, rc_prof->device) == SUCCESS) {
			rc = SUCCESS;
			break;
		}
		rc_prof = rc_prof->next;
	}
	if (rc != SUCCESS) {
		msg = "Error. Device %s does not exist";
		print_error(msg, return_message, device);
		return INVALID_VALUE;
	} else
		rc = INVALID_VALUE;

	gethostname(hostname, sizeof(hostname));
	sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		sprintf(path, "%s%s-%s", NQN_HDR_STR, hostname, device);
		msg = "Error. NQN export: %s does not exist";
		print_error(msg, return_message, path);
		return rc;
	}

	/* Check if namespace 1 exists. That is the only namespace we define. */
	sprintf(path, "%s/%s%s-%s/namespaces/1", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		msg = "%s: A RapidDisk defined namespace does not exist";
		print_error(msg, return_message, __func__);
		return rc;
	}

	/* Check if the revalidate_size attribute exists. */
	sprintf(path, "%s/%s%s-%s/namespaces/1/revalidate_size", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		msg = "%s: The kernel nvmet module version utilized does not support this function.";
		print_error(msg, return_message, __func__);
		return rc;
	}

	if ((fp = fopen(path, "w")) == NULL){
		msg = "Error. Unable to open %s. %s: fopen: %s";
		print_error(msg, return_message, path, __func__, strerror(errno));
		return rc;
	}
	fprintf(fp, "1");
	fclose(fp);

	msg = "NVMe Target Namespace size for %s revalidated.";
	print_error(msg, return_message, device);

	return SUCCESS;
}

/*
 * description: Unexport an NVMe Target.
 */

/**
 * It removes a block device from the NVMe Target subsystem
 *
 * @param device The block device to be exported.
 * @param host The hostname of the NVMe Target host.
 * @param port The port number to export the volume to.
 * @param return_message If you want to return a message to the caller, pass a pointer to a char array.
 *
 * @return The return value is the return code from the function.
 */
int nvmet_unexport_volume(char *device, char *host, int port, char *return_message)
{
	int rc = INVALID_VALUE, n, err;
	FILE *fp;
	char hostname[0x40] = {0x0}, path[NAMELEN] = {0x0};
	struct dirent **list;
	char *msg;

	gethostname(hostname, sizeof(hostname));
	sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		sprintf(path, "%s%s-%s", NQN_HDR_STR, hostname, device);
		msg = "Error. NQN export: %s does not exist";
		print_error(msg, return_message, path);
		return rc;
	}

	/* Check if namespace 1 exists. That is the only namespace we define. */
	sprintf(path, "%s/%s%s-%s/namespaces/1", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		msg = "%s: A RapidDisk defined namespace does not exist";
		print_error(msg, return_message, __func__);
		return rc;
	}

	/* Make sure that no other namespaces exist. We do not create anything higher than 1. */
	sprintf(path, "%s/%s%s-%s/namespaces/", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if ((err = scandir(path, &list, NULL, NULL)) < 0) {
		msg = "Error. Unable to access %s. %s: scandir: %s";
		print_error(msg, return_message, path, __func__, strerror(errno));
		return INVALID_VALUE;
	}

	list = clean_scandir(list, err);

	if (err > 3) {
		msg = "An invalid number of namespaces not created by RapidDisk exist.";
		print_error("%s", return_message, msg);
		return rc;
	}

	if (strlen(host) != 0) {
		sprintf(path, "%s/%s%s-%s/allowed_hosts/%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device, host);
		if (access(path, F_OK) == SUCCESS) {
			rc = unlink(path);
			if (rc != SUCCESS) {
				msg = "Error. Unable to remove host. %s: unlink: %s";
				print_error(msg, return_message, __func__, strerror(errno));
				return rc;
			}
			if (return_message == NULL) {
				msg = "Block device %s has been unmapped from NVMe Target host %s.";
				print_error(msg, return_message, device, host);
			}
		} else {
			msg = "%s: Host %s does not exist";
			print_error(msg, return_message, __func__, host);
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
						msg = "Error. Unable to remove hosts. %s: unlink: %s";
						print_error(msg, return_message, __func__, strerror(errno));
						list = clean_scandir(list, err);
						return rc;
					}
				}
			}
		}
		list = clean_scandir(list, err);
		if (return_message == NULL) {
			msg = "Block device %s has been unmapped from all NVMe Target hosts.";
			print_error(msg, return_message, device);
		}
	}

host_check_out:

	if (port != INVALID_VALUE) {
		sprintf(path, "%s/%d/subsystems/%s%s-%s", SYS_NVMET_PORTS, port, NQN_HDR_STR, hostname, device);
		if (access(path, F_OK) == SUCCESS) {
			rc = unlink(path);
			if (rc != SUCCESS) {
				msg = "Error. Unable to remove NQN from port. %s: unlink: %s";
				print_error(msg, return_message, __func__, strerror(errno));
				return rc;
			}
			if (return_message == NULL) {
				msg = "Block device %s has been unmapped from NVMe Target port %d.";
				print_error(msg, return_message, device, port);
			}
//		} else {
//			msg = "%s: Port %d and / or export does not exist";
//			print_error(msg, return_message, __func__, port);
//			return rc;
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
						msg = "Error. Unable to remove NQN from ports. %s: unlink: %s";
						print_error(msg, return_message, __func__, strerror(errno));
						list = clean_scandir(list, err);
						return rc;
					}
				}
			}
		}
		list = clean_scandir(list, err);
		if (return_message == NULL) {
			msg = "Block device %s has been unmapped from all NVMe Target ports.";
			print_error(msg, return_message, device);
		}
	}

port_check_out:

	/* If a port and / or host are defined, just remove / unmap and exit. */

	/* I changed this test from "((strlen(host) == 0) && (port == INVALID_VALUE))"
	 * because prevented the complete removal of the mapping
	 */

//	if ((strlen(host) != 0) && (port != INVALID_VALUE)) {
		if (access(path, F_OK) == SUCCESS) {
			/* Disable volume */
			sprintf(path, "%s/%s%s-%s/namespaces/1/enable", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
			if ((fp = fopen(path, "w")) == NULL){
				msg = "Error. Unable to open namespace enable file %s. %s: fopen: %s";
				print_error(msg, return_message, path, __func__, strerror(errno));
				return INVALID_VALUE;
			}
			fprintf(fp, "0");
			fclose(fp);
		}

		sprintf(path, "%s/%s%s-%s/namespaces/1", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if (access(path, F_OK) == SUCCESS) {
			if ((rc = rmdir(path)) != SUCCESS) {
				msg = "Error. Unable to remove namespace %s. %s: rmdir: %s";
				print_error(msg, return_message, path, __func__, strerror(errno));
				return rc;
			}
		}

		/* Remove NQN from NVMeT Subsystem */
		sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if (access(path, F_OK) == SUCCESS) {
			if ((rc = rmdir(path)) != SUCCESS) {
				msg = "Error. Unable to remove NQN from NVMe Target subsystem %s. %s: rmdir: %s";
				print_error(msg, return_message, path, __func__, strerror(errno));
				return rc;
			}
		}
//	}

	msg = "Block device %s has been removed from the NVMe Target subsystem.";
	print_error(msg, return_message, device);

	return SUCCESS;
}

/*
 * description: List all enabled NVMe Target ports.
 */

/**
 * It scans the system for all NVMe ports and returns a linked list of all the ports
 *
 * @param json_flag If true, then the output will be in JSON format.
 * @param error_message This is a pointer to a string that will be populated with an error message if the function fails.
 *
 * @return The return value is the return code from the function.
 */
int nvmet_view_ports(bool json_flag, char *error_message)
{
	int i = 1;
	int rc = SUCCESS;
	struct NVMET_PORTS *ports = NULL, *tmp_ports = NULL;

	ports = nvmet_scan_all_ports(error_message);
	if ((ports == NULL) && (strlen(error_message) != 0)) {
		return INVALID_VALUE;
	}

	if (json_flag == TRUE) {
		rc = json_nvmet_view_ports(ports, NULL, FALSE);
		free_nvmet_linked_lists(ports, NULL);
		ports = NULL;
		return rc;
	}
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
	free_nvmet_linked_lists(ports, NULL);
	ports = NULL;
	return SUCCESS;
}

/**
 * It creates a new port.
 *
 * @param interface The interface to use for the port.
 * @param port The port number to create.
 * @param protocol 0 = TCP, 1 = RDMA
 * @param return_message This is a pointer to a buffer that will be filled with the return message.
 *
 * @return The return value is the return code of the function.
 */
int nvmet_enable_port(char *interface, int port, int protocol, char *return_message)
{
	int rc = INVALID_VALUE;
	FILE *fp;
	char path[NAMELEN] = {0x0}, ip[0xF] = {0x0}, proto[0x5] = {0x0};
	mode_t mode = (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	struct NVMET_PORTS *ports, *tmp_ports;
	char *msg;
	char error_message[NAMELEN] = {0};

	ports = nvmet_scan_ports(error_message);
	if ((ports == NULL) && (strlen(error_message) != 0)) {
		msg = "%s";
		print_error(msg, return_message, error_message);
		return INVALID_VALUE;
	}

	sprintf(path, "%s/%d", SYS_NVMET_PORTS, port);
	if (access(path, F_OK) == SUCCESS) {
		msg = "Error. NVMe Target Port %d already exists.";
		print_error(msg, return_message, port);
		return rc;
	}

	char *ipaddr = nvmet_interface_ip_get(interface, error_message);
	if (ipaddr == NULL) {
		msg = "Cannot find the IP address of interface %s. Error: %s";
		print_error(msg, return_message, interface, error_message);
		return INVALID_VALUE;
	}
	sprintf(ip, "%s", ipaddr);

	if (ip_validate(ip) != SUCCESS) {
		msg = "Error. IP address %s is invalid.";
		print_error(msg, return_message, ip);
		return rc;
	}

	tmp_ports = ports;

	while (ports != NULL) {
		if (strcmp(ports->addr, ip) == SUCCESS) {
			msg = "Error. Interface %s with IP address %s is already in use on port %d.";
			print_error(msg, return_message, interface, ip, ports->port);
			free_nvmet_linked_lists(tmp_ports, NULL);
			return rc;
		}
		ports = ports->next;
	}

	free_nvmet_linked_lists(tmp_ports, NULL);

	if ((rc = mkdir(path, mode)) != SUCCESS) {
		msg = "Error. Unable to create port directory. %s: mkdir: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return rc;
	}

	sprintf(path, "%s/%d/addr_trsvcid", SYS_NVMET_PORTS, port);
	if ((fp = fopen(path, "w")) == NULL) {
		msg = "Error. Unable to open %s. %s: fopen: %s";
		print_error(msg, return_message, path, __func__, strerror(errno));
		return INVALID_VALUE;
	}
	fprintf(fp, "4420");
	fclose(fp);

	sprintf(path, "%s/%d/addr_adrfam", SYS_NVMET_PORTS, port);
	if ((fp = fopen(path, "w")) == NULL) {
		msg = "Error. Unable to open %s. %s: fopen: %s";
		print_error(msg, return_message, path, __func__, strerror(errno));
		return INVALID_VALUE;
	}
	fprintf(fp, "ipv4");
	fclose(fp);

	if (protocol == XFER_MODE_RDMA)
		sprintf(proto, "rdma");
	else
		sprintf(proto, "tcp");
	sprintf(path, "%s/%d/addr_trtype", SYS_NVMET_PORTS, port);
	if ((fp = fopen(path, "w")) == NULL) {
		msg = "Error. Unable to open %s. %s: fopen: %s";
		print_error(msg, return_message, path, __func__, strerror(errno));
		return INVALID_VALUE;
	}
	fprintf(fp, "%s", proto);
	fclose(fp);

	sprintf(path, "%s/%d/addr_traddr", SYS_NVMET_PORTS, port);
	if ((fp = fopen(path, "w")) == NULL) {
		msg = "Error. Unable to open %s. %s: fopen: %s";
		print_error(msg, return_message, path, __func__, strerror(errno));
		return INVALID_VALUE;
	}
	fprintf(fp, "%s", ip);
	fclose(fp);

	msg = "Successfully created port %d set to %s for interface %s (with IP address %s).";
	print_error(msg, return_message, port, proto, interface, ip);

	return SUCCESS;
}

/**
 * This function removes the specified NVMe Target port
 *
 * @param port The port number to be disabled.
 * @param return_message This is a pointer to a buffer that will be filled with a message
 *
 * @return The return code of the function.
 */
int nvmet_disable_port(int port, char *return_message)
{
	int rc = INVALID_VALUE, n, err;
	char path[NAMELEN] = {0x0};
	struct dirent **list;
	char *msg;

	sprintf(path, "%s/%d", SYS_NVMET_PORTS, port);
	if (access(path, F_OK) != SUCCESS) {
		msg = "Error. NVMe Target Port %d does not exist";
		print_error(msg, return_message, port);
		return rc;
	}

	/* Make sure that no subsystems are mapped from this port. */
	sprintf(path, "%s/%d/subsystems/", SYS_NVMET_PORTS, port);
	if ((err = scandir(path, &list, NULL, NULL)) < 0) {
		msg = "Error. Unable to access %s. %s: scandir: %s";
		print_error(msg, return_message, path, __func__, strerror(errno));
		return rc;
	}
	list = clean_scandir(list, err);
	if (err > 2) {
		msg = "%s";
		print_error(msg, return_message, "This port is currently in use.");
		return rc;
	}

	/* Remove the port */
	sprintf(path, "%s/%d", SYS_NVMET_PORTS, port);
	if ((rc = rmdir(path)) != SUCCESS) {
		msg = "Error. Unable to remove port. %s: rmdir: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return rc;
	}
	msg = "NVMe Target port %d has been removed.";
	print_error(msg, return_message, port);

	return SUCCESS;
}

#else

/**
 * This function scans the system for NVMe-oF subsystems and ports, and then creates a JSON string that contains the
 * information
 *
 * @param error_message This is a pointer to a character array that will contain the error message if the function fails.
 * @param json_result This is the JSON string that will be returned to the caller.
 */
int nvmet_view_exports_json(char *error_message, char **json_result) {
	int rc = SUCCESS;
	struct NVMET_PROFILE *nvmet = NULL;
	struct NVMET_PORTS *ports = NULL;

	nvmet = nvmet_scan_subsystem(error_message);
	if ((nvmet == NULL) && (strlen(error_message) != 0)) {
		return INVALID_VALUE;
	}

	ports = nvmet_scan_ports(error_message);
	if ((ports == NULL) && (strlen(error_message) != 0)) {
		free_nvmet_linked_lists(NULL, nvmet);
		nvmet = NULL;
		return INVALID_VALUE;
	}

	rc = json_nvmet_view_exports(nvmet, ports, json_result, TRUE);
	free_nvmet_linked_lists(ports, nvmet);
	ports = NULL;
	nvmet = NULL;
	return rc;
}

/**
 * This function scans the system for all NVMET ports and returns the results in JSON format
 *
 * @param error_message This is a pointer to a string that will be populated with an error message if the function fails.
 * @param json_result This is the JSON string that will be returned to the caller.
 */
int nvmet_view_ports_json(char *error_message, char **json_result) {
	int rc = SUCCESS;
	struct NVMET_PORTS *ports = NULL;

	ports = nvmet_scan_all_ports(error_message);
	if ((ports == NULL) && (strlen(error_message) != 0)) {
		return INVALID_VALUE;
	}

	rc = json_nvmet_view_ports(ports, json_result, TRUE);
	free_nvmet_linked_lists(ports, NULL);
	ports = NULL;
	return rc;
}

#endif