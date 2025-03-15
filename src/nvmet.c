/**
 * @file nvmet.c
 * @brief NVME function definitions
 * @details This file defines some NVME related functions
 * @copyright @verbatim
Copyright © 2011 - 2025 Petros Koutoupis

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
* @version 9.2.0
* @date 15 March 2025
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

struct NVMET_ALLOWED_HOST *allowed_host_head = NULL;
struct NVMET_ALLOWED_HOST *allowed_host_end = NULL;

/**
 * The function checks if the NVMe Target subsystem is loaded and returns a message if it is not.
 *
 * @param return_message The parameter "return_message" is a pointer to a character array that will be used to store any
 * error messages or status updates that the function generates during its execution.
 *
 * @return A boolean value (TRUE or FALSE) is being returned.
 */
bool check_nvmet_subsystem(char *return_message) {
	if (access(SYS_NVMET, F_OK) != SUCCESS) {
		char *msg = "The NVMe Target subsystem is not loaded. Please load the nvmet and nvmet-tcp (or nvme-loop) kernel modules and ensure that the kernel user configuration filesystem is mounted.";
		print_error("%s", return_message, msg);
		return FALSE;
	}
	return TRUE;
}

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
struct NVMET_PROFILE *nvmet_scan_subsystem(char *return_message, int *rc)
{
	int err2, n, i, ii, allowed_hosts, subsystems_number, rv = INVALID_VALUE;
	char file[NAMELEN * 2] = {0};
	struct NVMET_PROFILE *nvmet = NULL;
	struct NVMET_ALLOWED_HOST *allowed_host = NULL;
	struct NVMET_PORTS *ports = NULL, *orig_ports = NULL, *new_ports = NULL;
	struct dirent **list, **sublist, **allowed_hosts_list;
	nvmet_head = NULL;
	nvmet_end = NULL;
	allowed_host_head = NULL;
	allowed_host_end = NULL;
	ports_head = NULL;
	ports_end = NULL;
	char *msg;

	if (check_nvmet_subsystem(return_message) == FALSE) return NULL;

	ports = nvmet_scan_ports(return_message, &rv);
	if (rv == INVALID_VALUE) {
		return NULL;
	}
	orig_ports = ports;

	if ((subsystems_number = scandir(SYS_NVMET_TGT, &list, scandir_filter_no_dot, NULL)) < 0) {
		msg = "%s: scandir: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		free_nvmet_linked_lists(orig_ports, NULL);
		return NULL;
	}

	for (n = 0; n < subsystems_number; n++) {
		sprintf(file, "%s/%s/namespaces/", SYS_NVMET_TGT, list[n]->d_name);
		if ((err2 = scandir(file, &sublist, scandir_filter_no_dot, NULL)) < 0) {
			msg = "%s: scandir: %s";
			print_error(msg, return_message, __func__, strerror(errno));
			list = clean_scandir(list, subsystems_number);
			free_nvmet_linked_lists(orig_ports, NULL);
			return NULL;
		}
		for (i = 0; i < err2; i++) {
			if ((nvmet = (struct NVMET_PROFILE *)calloc(1, sizeof(struct NVMET_PROFILE))) == NULL ) {
				msg = ERR_CALLOC;
				print_error(msg, return_message, __func__, strerror(errno));
				list = clean_scandir(list, subsystems_number);
				sublist = clean_scandir(sublist, err2);
				free_nvmet_linked_lists(orig_ports, nvmet_head);
				return NULL;
			}
			strcpy(nvmet->nqn, (char *)list[n]->d_name);
			ports_head = NULL;
			while (ports != NULL) {
				if (strcmp(nvmet->nqn, ports->nqn) == 0) {
					if ((new_ports = (struct NVMET_PORTS *)calloc(1, sizeof(struct NVMET_PORTS))) == NULL ) {
						msg = ERR_CALLOC;
						print_error(msg, return_message, __func__, strerror(errno));
						list = clean_scandir(list, subsystems_number);
						sublist = clean_scandir(sublist, err2);
						free_nvmet_linked_lists(orig_ports, NULL);
						free_nvmet_linked_lists(ports_head, nvmet_head);
						return NULL;
					}
					memcpy(new_ports, ports, sizeof(NVMET_PORTS));
					new_ports->next = NULL;
					if (ports_head == NULL)
						ports_head = new_ports;
					else
						ports_end->next = new_ports;
					ports_end = new_ports;
					new_ports->next = NULL;
				}
				ports = ports->next;
			}
			nvmet->assigned_ports = ports_head;
			ports = orig_ports;

			sprintf(file, "%s/%s/allowed_hosts/", SYS_NVMET_TGT, list[n]->d_name);
			if ((allowed_hosts = scandir(file, &allowed_hosts_list, scandir_filter_no_dot, NULL)) < 0) {
				msg = "%s: scandir: %s";
				print_error(msg, return_message, __func__, strerror(errno));
				list = clean_scandir(list, subsystems_number);
				sublist = clean_scandir(sublist, err2);
				free_nvmet_linked_lists(orig_ports, nvmet_head);
				return NULL;
			}
			allowed_host_head = NULL;
			for (ii = 0; ii < allowed_hosts; ii++) {
				if ((allowed_host = (struct NVMET_ALLOWED_HOST *)calloc(1, sizeof(struct NVMET_ALLOWED_HOST))) == NULL ) {
					msg = ERR_CALLOC;
					print_error(msg, return_message, __func__, strerror(errno));
					list = clean_scandir(list, subsystems_number);
					sublist = clean_scandir(sublist, err2);
					allowed_hosts_list = clean_scandir(allowed_hosts_list, allowed_hosts);
					free_nvmet_linked_lists(orig_ports, nvmet_head);
					return NULL;
				}
				sprintf(allowed_host->allowed_host, "%s", allowed_hosts_list[ii]->d_name);
				if (allowed_host_head == NULL)
					allowed_host_head = allowed_host;
				else
					allowed_host_end->next = allowed_host;
				allowed_host_end = allowed_host;
				allowed_host->next = NULL;
			}
			allowed_hosts_list = clean_scandir(allowed_hosts_list, allowed_hosts);
			nvmet->allowed_hosts = allowed_host_head;
			sprintf(file, "%s/%s/namespaces/%s", SYS_NVMET_TGT, list[n]->d_name, sublist[i]->d_name);
			if (access(file, F_OK) != INVALID_VALUE) {
				char *info_device = read_info(file, "device_path", return_message);
				if (info_device == NULL) {
					free(nvmet);
					nvmet = NULL;
					list = clean_scandir(list, subsystems_number);
					sublist = clean_scandir(sublist, err2);
					free_nvmet_linked_lists(orig_ports, nvmet_head);
					nvmet_head = NULL;
					return NULL;
				}
				sprintf(nvmet->device, "%s", info_device);
				info_device = read_info(file, "enable", return_message);
				if (info_device == NULL) {
					free(nvmet);
					nvmet = NULL;
					list = clean_scandir(list, subsystems_number);
					sublist = clean_scandir(sublist, err2);
					free_nvmet_linked_lists(orig_ports, nvmet_head);
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
		sublist = clean_scandir(sublist, err2);
	}
	list = clean_scandir(list, subsystems_number);
	*rc = SUCCESS;
	free_nvmet_linked_lists(orig_ports, NULL);
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
struct NVMET_PORTS *nvmet_scan_ports(char *return_message, int *rc)
{
	int err, err2, n = 0, i;
	char file[NAMELEN * 2] = {0};
	struct NVMET_PORTS *nvmet_ports = NULL;
	struct dirent **ports, **exports;
	ports_head = NULL;
	ports_end = NULL;
	char *msg;

	if (check_nvmet_subsystem(return_message) == FALSE) return NULL;

	if ((err = scandir(SYS_NVMET_PORTS, &ports, scandir_filter_no_dot, NULL)) < 0) {
		msg = "%s: scandir: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return NULL;
	}
	for (; n < err; n++) {
		memset(file, 0x0, NAMELEN);
		sprintf(file, "%s/%s", SYS_NVMET_PORTS, ports[n]->d_name);
		if (access(file, F_OK) != INVALID_VALUE) {
			sprintf(file, "%s/%s/subsystems", SYS_NVMET_PORTS, ports[n]->d_name);
			if ((err2 = scandir(file, &exports, scandir_filter_no_dot, NULL)) < 0) {
				msg = "%s: scandir: %s";
				print_error(msg, return_message, __func__, strerror(errno));
				ports = clean_scandir(ports, err);
				return NULL;
			}
			for (i = 0; i < err2; i++) {
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
			exports = clean_scandir(exports, err2);
		}
	}
	ports = clean_scandir(ports, err);
	*rc = SUCCESS;
	return ports_head;
}

/*
 * description: Scan all NVMe Targets ports.
 */

/**
 * This function scans the /sys/kernel/config/nvmet/ports directory for all the ports that are currently configured
 *
 * @param return_message This is a pointer to a string that will be populated with an error message if the function fails.
 * @param rc return code
 *
 * @return A pointer to the first element of a linked list of struct NVMET_PORTS.
 */
struct NVMET_PORTS *nvmet_scan_all_ports(char *return_message, int *rc)
{
	int err, n = 0;
	char file[NAMELEN * 2] = {0};
	struct NVMET_PORTS *nvmet_ports;
	struct dirent **ports;
	ports_head = NULL;
	ports_end = NULL;
	char *msg;

	if (check_nvmet_subsystem(return_message) == FALSE) return NULL;

	if ((err = scandir(SYS_NVMET_PORTS, &ports, scandir_filter_no_dot, NULL)) < 0) {
		msg = "%s: scandir: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return NULL;
	}
	for (; n < err; n++) {
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
	ports = clean_scandir(ports, err);
	*rc = SUCCESS;
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
	int rc = SUCCESS, rv = INVALID_VALUE;
	struct NVMET_PROFILE *nvmet, *nvmet_orig;
	struct NVMET_PORTS *ports;
	struct NVMET_ALLOWED_HOST *hosts;
	nvmet = nvmet_scan_subsystem(error_message, &rv);
	if (rv == INVALID_VALUE) {
		return INVALID_VALUE;
	}
	nvmet_orig = nvmet;

	if (json_flag == TRUE) {
		rc = json_nvmet_view_exports(nvmet, NULL, FALSE);
		free_nvmet_linked_lists(NULL, nvmet_orig);
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
			hosts = nvmet->allowed_hosts;
			while (hosts != NULL) {
				printf("\t\tAllowed host: %s\n", hosts->allowed_host);
				hosts = hosts->next;
			}
			ports = nvmet->assigned_ports;
			while (ports != NULL) {
				printf("\t\tLinked port: %d\tIP address: %s (%s)\n", ports->port,
					   ports->addr, ports->protocol);
				ports = ports->next;
			}
			i++;
			nvmet = nvmet->next;
		}
	}
	free_nvmet_linked_lists(NULL, nvmet_orig);
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
	int rc = INVALID_VALUE, n, err, rv = INVALID_VALUE;
	FILE *fp;
	mode_t mode = (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	char hostname[0x40] = {0x0}, path[NAMELEN] = {0x0}, link[NAMELEN] = {0x0};
	struct dirent **list;
	char *msg;
	struct NVMET_PORTS *ports;
	struct NVMET_PORTS *orig_ports;

	/*
	 * We do not care if the device has already been exported. We will continue to go through
	 * the motions to add more host NQNs to export the target to.
	 */

	/* Check to see if NVMe subsystem is loaded */

	if (check_nvmet_subsystem(return_message) == FALSE) return rc;

	/* Check to see if device exists */

	/* Looks for rd* ramdisk */
	while (rd_prof != NULL) {
		if (strcmp(device, rd_prof->device) == SUCCESS) {
			rc = SUCCESS;
			break;
		}
		rd_prof = rd_prof->next;
	}

	/* Looks for rc-* mappings */
	while (rc_prof != NULL) {
		if (strcmp(device, rc_prof->device) == SUCCESS) {
			rc = SUCCESS;
			break;
		}
		rc_prof = rc_prof->next;
	}

	if (rc != SUCCESS) {
		print_error(ERR_DEV_NOEXIST, return_message, device);
		return rc;
	}

	/* Reset rc to INVALID_VALUE after device check */
	rc = INVALID_VALUE;

	if (port != INVALID_VALUE) {
		ports = nvmet_scan_all_ports(return_message, &rv);
		if (rv == INVALID_VALUE) {
			return rc;
		}
		orig_ports = ports;
		while (ports != NULL) {
			if (port == ports->port) {
				rc = SUCCESS;
				break;
			}
			ports = ports->next;
		}
		if (rc != SUCCESS) {
			print_error(ERR_PORT_NOEXIST, return_message, port);
			free_nvmet_linked_lists(orig_ports, NULL);
			return rc;
		}

		/* Reset rc to INVALID_VALUE after port check */
		rc = INVALID_VALUE;

		free_nvmet_linked_lists(orig_ports, NULL);
	}

	/* Grab hostname for NQN */
	if (gethostname(hostname, sizeof(hostname)) < 0) {
		msg = "Error. Unable to obtain hostname. %s: gethostname: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return rc;
	}

	/* Example of this path:
	 * /sys/kernel/config/nvmet/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0
	 */
	sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		if (mkdir(path, mode) != SUCCESS) {
			msg = "Error. Unable to create target directory %s. %s: mkdir: %s";
			print_error(msg, return_message, path, __func__, strerror(errno));
			return rc;
		}
	}

	/* Set host NQNs to access target */

	/* If host is empty, it means we can allow all remotes to connect */
	if (strlen(host) == 0) {
		/* Example of this path:
		 * /sys/kernel/config/nvmet/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0/allowed_hosts
	 	 */
		sprintf(path, "%s/%s%s-%s/allowed_hosts", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if ((err = scandir(path, &list, scandir_filter_no_dot, NULL)) < 0) {
			msg = "Error. Unable to access %s. %s: scandir: %s";
			print_error(msg, return_message, path, __func__, strerror(errno));
			return INVALID_VALUE;
		}

		list = clean_scandir(list, err);

		/* If err > 0 we have already specified some remote hosts which are the only ones
		 * allowed to connect, so it is pointless to go further since we cannot enable
		 * the unrestricted access mode
		 */
		if (err > 0) {
			msg = "You did not specified any allowed host, but one or more already exist. Please remove existing host(s) or specify a new one.";
			print_error("%s", return_message, msg);
			return INVALID_VALUE;
		}

		/* Example of this path:
		 * /sys/kernel/config/nvmet/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0/attr_allow_any_host
	 	 */
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

		/* Make sure that no other hosts can access the target
		 *
		 * Example of this path:
		 * /sys/kernel/config/nvmet/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0/attr_allow_any_host
	 	 */
		sprintf(path, "%s/%s%s-%s/attr_allow_any_host", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if ((fp = fopen(path, "w")) == NULL){
			msg = "Error. Unable to open %s. %s: fopen: %s";
			print_error(msg, return_message, path, __func__, strerror(errno));
			return INVALID_VALUE;
		}
		fprintf(fp, "0");
		fclose(fp);

		/* Example of this path:
		 * /sys/kernel/config/nvmet/hosts/<REMOTE-HOST>
	 	 */
		sprintf(path, "%s/%s", SYS_NVMET_HOSTS, host);
		if (access(path, F_OK) != SUCCESS) {
			if (mkdir(path, mode) != SUCCESS) {
				msg = "Error. Unable to create host directory %s. %s: mkdir: %s";
				print_error(msg, return_message, path, __func__, strerror(errno));
				return rc;
			}
		}

		/* Example of this path:
		 * /sys/kernel/config/nvmet/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0/allowed_hosts/<REMOTE-HOST>
	 	 */
		sprintf(link, "%s/%s%s-%s/allowed_hosts/%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device, host);

		if (access(link, F_OK) != SUCCESS) {
			if (symlink(path, link) != SUCCESS) {
				msg = "Error. Unable to link host to target. %s: symlink: %s";
				print_error(msg, return_message, __func__, strerror(errno));
				return rc;
			}
		} else
			goto nvme_export_enable_volume;
	}

	/* Set model
	 *
	 * Example of this path:
	 * /sys/kernel/config/nvmet/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0/attr_model
	 */
	sprintf(path, "%s/%s%s-%s/attr_model", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) == SUCCESS) {
		if ((fp = fopen(path, "w")) == NULL){
			msg = "Error. Unable to set model string to file %s. %s: fopen: %s";
			print_error(msg, return_message, path, __func__, strerror(errno));
			return rc;
		}
		fprintf(fp, "RapidDisk");
		fclose(fp);
	}

	/* Create namespace
	 *
	 * Example of this path:
	 * /sys/kernel/config/nvmet/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0/namespaces/1
	 */
	sprintf(path, "%s/%s%s-%s/namespaces/1", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		if (mkdir(path, mode) != SUCCESS) {
			msg = "Error. Unable to create namespace directory %s. %s: mkdir: %s";
			print_error(msg, return_message, path, __func__, strerror(errno));
			return rc;
		}
	}

	/* Set device
	 *
	 * Example of this path:
	 * /sys/kernel/config/nvmet/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0/namespaces/1/device_path
	 */
	sprintf(path, "%s/%s%s-%s/namespaces/1/device_path", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if ((fp = fopen(path, "w")) == NULL){
		msg = "Error. Unable to open device_path file: %s. %s: fopen: %s";
		print_error(msg, return_message, path, __func__, strerror(errno));
		return rc;
	}
	if (strncmp(device, "rd", 2) == SUCCESS)
		fprintf(fp, "/dev/%s", device);
	else
		fprintf(fp, "/dev/mapper/%s", device);
	fclose(fp);

nvme_export_enable_volume:
	/* Enable volume
	 *
	 * Example of this path:
	 * /sys/kernel/config/nvmet/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0/namespaces/1/enable
	 */
	sprintf(path, "%s/%s%s-%s/namespaces/1/enable", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if ((fp = fopen(path, "w")) == NULL){
		msg = "Error. Unable to open namespace enable file %s. %s: fopen: %s";
		print_error(msg, return_message, path, __func__, strerror(errno));
		return rc;
	}
	fprintf(fp, "1");
	fclose(fp);

	/* Set to a port */
	if (port != INVALID_VALUE) {
		/* Example of this path:
		 * /sys/kernel/config/nvmet/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0
	 	 */
		sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		/* Example of this path:
		 * /sys/kernel/config/nvmet/ports/<PORT>/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0
	 	 */
		sprintf(link, "%s/%d/subsystems/%s%s-%s", SYS_NVMET_PORTS, port, NQN_HDR_STR, hostname, device);
		if (access(link, F_OK) != SUCCESS) {
			if (symlink(path, link) != SUCCESS) {
				msg = "Error. Unable to create link of NQN to port. %s: symlink: %s";
				print_error(msg, return_message, __func__, strerror(errno));
				return rc;
			}
		}
	} else {
		/* Iterate through all ports and enable target on each */
		if ((err = scandir(SYS_NVMET_PORTS, &list, scandir_filter_no_dot, NULL)) < 0) {
			msg = "%s: scandir: %s";
			print_error(msg, return_message, __func__, strerror(errno));
			return rc;
		}
		for (n = 0; n < err; n++) {
			/* Example of this path:
		 	 * /sys/kernel/config/nvmet/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0
	 	 	 */
			sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
			/* Example of this path:
			 * /sys/kernel/config/nvmet/ports/<PORT_ON_DISK>/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0
			 */
			sprintf(link, "%s/%s/subsystems/%s%s-%s", SYS_NVMET_PORTS, list[n]->d_name, NQN_HDR_STR, hostname, device);
			if (access(link, F_OK) != SUCCESS) {
				if (symlink(path, link) != SUCCESS) {
					msg = "Error. Unable to create link of NQN to port. %s: symlink: %s";
					print_error(msg, return_message, __func__, strerror(errno));
					list = clean_scandir(list, err);
					return rc;
				}
			}
		}
		list = clean_scandir(list, err);
	}

	sprintf(path, "port %d", port);
	msg = "Block device %s has been mapped to %s through %s as %s%s-%s.";
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

	/* Check to see if NVMe subsystem is loaded */
	if (check_nvmet_subsystem(return_message) == FALSE) return rc;

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
		print_error(ERR_DEV_NOEXIST, return_message, device);
		return INVALID_VALUE;
	} else
		rc = INVALID_VALUE;

	gethostname(hostname, sizeof(hostname));
	sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		sprintf(path, "%s%s-%s", NQN_HDR_STR, hostname, device);
		msg = "Error. NQN export: %s does not exist.";
		print_error(msg, return_message, path);
		return rc;
	}

	/* Check if namespace 1 exists. That is the only namespace we define. */
	sprintf(path, "%s/%s%s-%s/namespaces/1", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		msg = "%s: A RapidDisk defined namespace does not exist.";
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
	int rc = INVALID_VALUE, n, allowed_host_number, rv = INVALID_VALUE, port_number;
	int nqn_ok = 0, host_ok = 0, port_ok = 0;
	FILE *fp;
	char hostname[0x40] = {0x0}, path[NAMELEN] = {0x0}, nqn[NAMELEN] = {0x0};
	mode_t mode = (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	struct dirent **list;
	char *msg;
	char target_string[NAMELEN] = {0};
	char target_string_final[NAMELEN] = {0};
	char regexp[NAMELEN] = {0};
	struct NVMET_PROFILE *profile;
	struct NVMET_PROFILE *orig_profile;
	struct NVMET_PORTS *ports;
	struct NVMET_ALLOWED_HOST *allowed_hosts;

	if (gethostname(hostname, sizeof(hostname)) < 0) {
		msg = "Error. Unable to obtain hostname. %s: gethostname: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return rc;
	}

	sprintf(nqn, "%s%s-%s", NQN_HDR_STR, hostname, device);

	profile = nvmet_scan_subsystem(return_message, &rv);
	if (rv == INVALID_VALUE) {
		return rc;
	}
	orig_profile = profile;

	/*
	 * This loop gathers information about the user specified port, the NQN, and the user specified host.
	 * At the end, we will know:
	 * 1) if the NQN specified by the user exists or not
	 * 2) if the host specified by the user is in the list of allowed hosts of the specified NQN
	 * 3) if the host specified by the user is in the list of allowed hosts of another NQN
	 * 4) if the port specfied by the user is linked to the NQN specified by the user
	 * 5) if the port specified by the user is linked to another NQN
	 *
	 */
	while (profile != NULL) {
		/* looks for the NQN in the profile */
		if (strcmp(nqn, profile->nqn) == 0) {
			nqn_ok = TRUE;
		}
		if (port != INVALID_VALUE) {
			ports = profile->assigned_ports;
			while (ports != NULL) {
				if ((port == ports->port) && (strcmp(nqn, ports->nqn) == 0)) {
					/* In the specified port the specified NQN esists */
					port_ok = port_ok | 1 << 0;
				} else if ((port == ports->port) && (strcmp(nqn, ports->nqn) != 0)) {
					/* In the specified port another NQN esists */
					port_ok = port_ok | 1 << 1;
				} else if ((port != ports->port) && (strcmp(nqn, ports->nqn) == 0)) {
					/* The NQN is mapped to other ports than the specified one */
					port_ok = port_ok | 1 << 2;
				}
				ports = ports->next;
			}
		}
		if (strlen(host) > 0) {
			allowed_hosts = profile->allowed_hosts;
			while (allowed_hosts != NULL) {
				if (strcmp(allowed_hosts->allowed_host, host) == 0) {
					if (strcmp(nqn, profile->nqn) != 0) {
						/* the host is present in another NQN, in
						 * /sys/kernel/config/nvmet/subsystems/<OTHER_NQN>/allowed_hosts/
						 */
						host_ok = host_ok | 1 << 0;
					} else {
						/* the host is present in the user-specified NQN, in
						 * /sys/kernel/config/nvmet/subsystems/<NQN>/allowed_hosts/
						 */
						host_ok = host_ok | 1 << 1;
					}
				}
				allowed_hosts = allowed_hosts->next;
			}
		}
		profile = profile->next;
	}
		
	free_nvmet_linked_lists(NULL, orig_profile);

	if (! nqn_ok) {
		print_error("NQN %s does not exists!", return_message, nqn);
		return rc;
	}

	if ((port != INVALID_VALUE) && (! (port_ok & (1<<0)))) {
		print_error("NQN %s on port %d does not exists!", return_message, nqn, port);
		return rc;
	}

	if ((strlen(host) > 0) && (! (host_ok & (1<<1)))) {
		print_error("Error. Host name '%s' is not in the list of allowed hosts for this NQN: '%s'", return_message, host, nqn);
		return rc;
	}

	/* Example of this path:
	 * /sys/kernel/config/nvmet/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0/allowed_hosts
	 */
	sprintf(path, "%s/%s/allowed_hosts", SYS_NVMET_TGT, nqn);
	if ((allowed_host_number = scandir(path, &list, scandir_filter_no_dot, NULL)) < 0) {
		msg = "Error. Unable to access %s. %s: scandir: %s";
		print_error(msg, return_message, path, __func__, strerror(errno));
		return rc;
	}

	if (allowed_host_number > 0) {
		for (n = 0; n < allowed_host_number; n++) {
			strcat(target_string, list[n]->d_name);
			strcat(target_string, " ");
		}
		size_t size = strlen(target_string);
		size = size - 1;
		target_string[size] = '\0';
	}

	list = clean_scandir(list, allowed_host_number);

	if (allowed_host_number > 0 && strlen(host) <= 0) {
		msg = "Error. No host provided, but one or more target(s) is/are still mapped to NQN(s): %s. Please remove them first.";
		print_error(msg, return_message, target_string);
		return rc;
	} else if (allowed_host_number > 0 && strlen(host) > 0) {
		if (! (host_ok & (1<<1))) {
			print_error("Error. Host name '%s' is not in the list of allowed hosts for this NQN: '%s'", return_message, host, nqn);
			return INVALID_VALUE;
		}
		/* Example of this path:
	 	 * /sys/kernel/config/nvmet/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0/allowed_hosts/<HOST>
	 	 */
		sprintf(path, "%s/%s/allowed_hosts/%s", SYS_NVMET_TGT, nqn, host);
		if (access(path, F_OK) == SUCCESS && (host_ok & (1<<1))) {
			if (unlink(path) != SUCCESS) {
				msg = "Error. Unable to remove link %s. %s: unlink: %s";
				print_error(msg, return_message, path, __func__, strerror(errno));
				return rc;
			}
			allowed_host_number--;
		}
		/* Example of this path:
	 	 * /sys/kernel/config/nvmet/hosts/<HOST>
	 	 *
	 	 * The "global" <HOST> directory is removed ONLY if no other NQNs are using it.
	 	 */
		sprintf(path, "%s/%s", SYS_NVMET_HOSTS, host);
		if (access(path, F_OK) == SUCCESS && ! (host_ok & (1<<0))) {
			if (rmdir(path) != SUCCESS) {
				msg = "Error. Unable to remove dir %s. %s: rmdir: %s";
				print_error(msg, return_message, path, __func__, strerror(errno));
				return rc;
			}
		}

		if (allowed_host_number > 0) {
			sprintf(regexp, "%s%s%s", "\\s?", host, "\\s?");
			preg_replace(regexp, "", target_string, target_string_final, sizeof(target_string_final));
			msg = "Warning. One or more target(s) is/are still mapped to NQN(s): %s. Please remove them first.";
			print_error(msg, return_message, target_string_final);
			return SUCCESS;
		}
	}
	if (allowed_host_number == 0) {
		if (port != INVALID_VALUE) {
			/* We remove the NQN from <PORT> ONLY */

			/* Example of this path:
		 	 * /sys/kernel/config/nvmet/ports/<PORT>/subsystems/nqn.2021-06.org.rapiddisk:ubuserver-rd0
	 	 	 */
			sprintf(path, "%s/%d/subsystems/%s", SYS_NVMET_PORTS, port, nqn);
			if (access(path, F_OK) == SUCCESS) {
				if (unlink(path) != SUCCESS) {
					msg = "Error. Unable to remove link of NQN from port %d. %s: unlink: %s";
					print_error(msg, return_message, port, __func__, strerror(errno));
					return rc;
				}
			}
		} else {
			/* We remove the NQN from ALL the <PORT>s */

			/* Example of this path:
		 	 * /sys/kernel/config/nvmet/ports
	 	 	 */
			sprintf(path, "%s", SYS_NVMET_PORTS);
			if ((port_number = scandir(path, &list, scandir_filter_no_dot, NULL)) < 0) {
				msg = "Error. Unable to scan NVMe Target ports directory for %s. %s: scandir: %s";
				print_error(msg, return_message, path, __func__, strerror(errno));
				return rc;
			}
			for (n = 0; n < port_number; n++) {
				/* Example of this path:
		 	 	 * /sys/kernel/config/nvmet/ports/<PORT>
	 	 	 	 */
				sprintf(path, "%s/%s", SYS_NVMET_PORTS, list[n]->d_name);
				if (access(path, F_OK) == SUCCESS) {
					/* Example of this path:
					 * /sys/kernel/config/nvmet/ports/<PORT>/<NQN>
	 	 	 	     */
					sprintf(path, "%s/%s/subsystems/%s", SYS_NVMET_PORTS, list[n]->d_name, nqn);
					if (access(path, F_OK) == SUCCESS) {
						if (unlink(path) != SUCCESS) {
							msg = "Error. Unable to remove NQN %s. %s: rmdir: %s";
							print_error(msg, return_message, path, __func__, strerror(errno));
							list = clean_scandir(list, port_number);
							return rc;
						}
					}
				}
			}
			list = clean_scandir(list, port_number);
		}

		if ((port_ok & (1<<0)) && (port_ok & (1<<2))) {
			/* The NQN is mapped to the user specified port AND some other ports,
			 * so we should not remove it globally
			 */
			msg = "Warning. The NQN was removed from port %d, but is still exported on other ports.";
			print_error(msg, return_message, port);
			return SUCCESS;
		}

		sprintf(path, "%s/%s/attr_allow_any_host", SYS_NVMET_TGT, nqn);
		if ((fp = fopen(path, "w")) == NULL){
			msg = "Error. Unable to open %s. %s: fopen: %s";
			print_error(msg, return_message, path, __func__, strerror(errno));
			return INVALID_VALUE;
		}
		fprintf(fp, "0");
		fclose(fp);

		/* Disable volume */
		sprintf(path, "%s/%s/namespaces/1/enable", SYS_NVMET_TGT, nqn);
		if ((fp = fopen(path, "w")) == NULL){
			msg = "Error. Unable to open namespace enable file %s. %s: fopen: %s";
			print_error(msg, return_message, path, __func__, strerror(errno));
			return rc;
		}
		fprintf(fp, "0");
		fclose(fp);

		/* Remove namespace */
		sprintf(path, "%s/%s/namespaces/1", SYS_NVMET_TGT, nqn);
		if (access(path, F_OK) == SUCCESS) {
			if (rmdir(path) != SUCCESS) {
				msg = "Error. Unable to remove namespace %s. %s: rmdir: %s";
				print_error(msg, return_message, path, __func__, strerror(errno));
				return rc;
			}
		}

		/* Remove NQN from NVMeT Subsystem */

		/*
		 * The check for the rmdir() result and the operations carried on upon failure
		 * are intended to prevent the situation described below:
		 *
		 * if the rmdir() call fails, it means we could not remove the NQN dir:
		 *
		 * "/sys/kernel/config/nvmet/subsystems/<NQN>"
		 *
		 * from the NVMe subsystem.
		 *
		 * So, an additional test is performed to determine wheather the namespace directory:
		 *
		 * "/sys/kernel/config/nvmet/subsystems/<NQN>/namespaces/1"
		 *
		 * still exists or not.
		 * If it was removed, the NVMe's kernel dir structure is corrupt, since the NQN's namespace has been
		 * removed, but the NQN itself was not.
		 *
		 * Since the functions creating some linked lists, nvmet_scan_subsystem(), nvmet_scan_ports() and
		 * nvmet_scan_all_ports(), immediatly check for the namespace to exist and return an error if it does not,
		 * and since at least one of them is called when a NVMe operation is to be performed, any further rapiddisk
		 * command intended to manipulate the NVMe subsystem will fail immediately.
		 *
		 * So, when the removal of:
		 *
		 * "/sys/kernel/config/nvmet/subsystems/<NQN>"
		 *
		 * fails, we check if the dir:
		 *
		 * "/sys/kernel/config/nvmet/subsystems/<NQN>/namespaces/1"
		 *
		 * is still present. If it is not, we recreate it.
		 *
		 * By doing this operation, we ensure that any further invocation of rapiddisk which is aimed to handle
		 * the NVMe subsystem, would not fail immediatly.
		 *
		 */
		sprintf(path, "%s/%s", SYS_NVMET_TGT, nqn);
		if (access(path, F_OK) == SUCCESS) {
			if (rmdir(path) != SUCCESS) {
				msg = "Error. Unable to remove NQN from NVMe Target subsystem %s. %s: rmdir: %s";
				print_error(msg, return_message, path, __func__, strerror(errno));
				sprintf(path, "%s/%s/namespaces/1", SYS_NVMET_TGT, nqn);
				if (access(path, F_OK) != SUCCESS) {
					if (mkdir(path, mode) != SUCCESS) {
						msg = "Error. Unable to restore namespace %s. %s: mkdir: %s";
						print_error(msg, return_message, path, __func__, strerror(errno));
						return rc;
					}
				}
				return rc;
			}
		}
	}

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
	int rc = SUCCESS, rv = INVALID_VALUE;
	struct NVMET_PORTS *ports = NULL, *tmp_ports = NULL;

	ports = nvmet_scan_all_ports(error_message, &rv);
	if (rv == INVALID_VALUE) {
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
 * @param protocol 0 = TCP, 1 = RDMA, 2 = LOOP
 * @param return_message This is a pointer to a buffer that will be filled with the return message.
 *
 * @return The return value is the return code of the function.
 */
int nvmet_enable_port(char *interface, int port, int protocol, char *return_message)
{
	int rc = INVALID_VALUE, rv = INVALID_VALUE;
	FILE *fp;
	char path[NAMELEN] = {0x0}, ip[0xF] = {0x0}, proto[0x5] = {0x0};
	mode_t mode = (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	struct NVMET_PORTS *ports, *tmp_ports;
	char *msg;
	char error_message[NAMELEN] = {0};

	/* Check to see if NVMe subsystem is loaded */
	if (check_nvmet_subsystem(return_message) == FALSE) return rc;

	sprintf(path, "%s/%d", SYS_NVMET_PORTS, port);
	if (access(path, F_OK) == SUCCESS) {
		msg = "Error. NVMe Target Port %d already exists.";
		print_error(msg, return_message, port);
		return rc;
	}

	if (protocol != XFER_MODE_LOOP) {
		ports = nvmet_scan_ports(return_message, &rv);
		if (rv == INVALID_VALUE) {
			return INVALID_VALUE;
		}

		char *ipaddr = nvmet_interface_ip_get(interface, error_message);
		if (ipaddr == NULL) {
			msg = "Cannot find the IP address of interface %s. Error: %s";
			print_error(msg, return_message, interface, error_message);
			free_nvmet_linked_lists(ports, NULL);
			return INVALID_VALUE;
		}
		sprintf(ip, "%s", ipaddr);

		if (ip_validate(ip) != SUCCESS) {
			msg = "Error. IP address %s is invalid.";
			print_error(msg, return_message, ip);
			free_nvmet_linked_lists(ports, NULL);
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
	}

	if ((rc = mkdir(path, mode)) != SUCCESS) {
		msg = "Error. Unable to create port directory. %s: mkdir: %s";
		print_error(msg, return_message, __func__, strerror(errno));
		return rc;
	}

	if (protocol == XFER_MODE_LOOP) {
		sprintf(proto, "loop");
		sprintf(path, "%s/%d/addr_trtype", SYS_NVMET_PORTS, port);
		if ((fp = fopen(path, "w")) == NULL) {
			msg = "Error. Unable to open %s. %s: fopen: %s";
			print_error(msg, return_message, path, __func__, strerror(errno));
			return INVALID_VALUE;
		}
		fprintf(fp, "%s", proto);
		fclose(fp);

		msg = "Successfully created port %d set for %s.";
		print_error(msg, return_message, port, proto);
		return SUCCESS;
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
	int rc = INVALID_VALUE, err;
	char path[NAMELEN] = {0x0};
	struct dirent **list;
	char *msg;

	/* Check to see if NVMe subsystem is loaded */
	if (check_nvmet_subsystem(return_message) == FALSE) return rc;

	sprintf(path, "%s/%d", SYS_NVMET_PORTS, port);
	if (access(path, F_OK) != SUCCESS) {
		msg = "Error. NVMe Target Port %d does not exist.";
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
	int rc = SUCCESS, rv = INVALID_VALUE;
	struct NVMET_PROFILE *nvmet = NULL;

	nvmet = nvmet_scan_subsystem(error_message, &rv);
	if (rv == INVALID_VALUE) {
		return INVALID_VALUE;
	}

	rc = json_nvmet_view_exports(nvmet, json_result, TRUE);
	free_nvmet_linked_lists(NULL, nvmet);
	return rc;
}

/**
 * This function scans the system for all NVMET ports and returns the results in JSON format
 *
 * @param error_message This is a pointer to a string that will be populated with an error message if the function fails.
 * @param json_result This is the JSON string that will be returned to the caller.
 */
int nvmet_view_ports_json(char *error_message, char **json_result) {
	int rc = SUCCESS, rv = INVALID_VALUE;
	struct NVMET_PORTS *ports = NULL;

	ports = nvmet_scan_all_ports(error_message, &rv);
	if (rv == INVALID_VALUE) {
		return INVALID_VALUE;
	}

	rc = json_nvmet_view_ports(ports, json_result, TRUE);
	free_nvmet_linked_lists(ports, NULL);
	ports = NULL;
	return rc;
}

#endif
