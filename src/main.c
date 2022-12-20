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

#include "main.h"
#include "utils.h"
#include "sys.h"
#include "rdsk.h"
#include "json.h"
#include "nvmet.h"

bool writeback_enabled;

void online_menu(char *string)
{
	printf("%s is an administration tool to manage RapidDisk RAM disk devices and\n"
	       "\tRapidDisk-Cache mappings.\n\n", string);
	printf("Usage: %s [ -h | -v ] function [ parameters ]\n\n", string);
	printf("Description:\n\t%s is a RapidDisk module management tool to manage RapidDisk\n"
	       "\tRAM disk devices. Dynamically create, remove, resize RAM volumes and if\n"
	       "\tdesired, map or unmap them as a cache volume to any block device.\n\n", string);
	printf("Functions:\n"
	       "\t-a\t\tAttach RAM disk device (size in MBytes).\n"
	       "\t-b\t\tBackend block device absolute path (for cache mapping).\n"
	       "\t-c\t\tInput capacity for size or resize of RAM disk device (in MBytes).\n"
	       "\t-d\t\tDetach RAM disk device.\n"
	       "\t-e\t\tExport a RapidDisk block device as an NVMe Target.\n"
	       "\t-f\t\tErase all data to a specified RapidDisk device \033[31;1m(dangerous)\033[0m.\n"
	       "\t-g\t\tDo not print header, useful with -j.\n"
	       "\t-H\t\tThe host to export / unexport the NVMe Target to / from.\n"
	       "\t-h\t\tDisplay the help menu.\n"
	       "\t-i\t\tDefine the network interface to enable for NVMe Target exporting.\n"
	       "\t-j\t\tEnable JSON formatted output.\n"
	       "\t-L\t\tLock a RapidDisk block device (set to read-only).\n"
	       "\t-l\t\tList all attached RAM disk devices.\n"
	       "\t-m\t\tMap an RapidDisk device as a caching node to another block device.\n"
	       "\t-N\t\tList only enabled NVMe Target ports.\n"
	       "\t-n\t\tList RapidDisk enabled NVMe Target exports.\n"
	       "\t-P\t\tThe port to export / unexport the NVMe Target to / from.\n"
	       "\t-p\t\tDefine cache policy: write-through, write-around or writeback \033[31;1m(dangerous)\033[0m\n"
	       "\t\t\t(default: write-through). Writeback caching is supplied by the dm-writecache\n"
	       "\t\t\tkernel module and is not intended for production use as it may result in data\n"
	       "\t\t\tloss on hardware/power failure.\n"
	       "\t-R\t\tRevalidate size of NVMe export using existing RapidDisk device.\n"
	       "\t-r\t\tDynamically grow the size of an existing RapidDisk device.\n"
	       "\t-s\t\tObtain RapidDisk-Cache Mappings statistics.\n"
	       "\t-t\t\tDefine the NVMe Target port's transfer protocol (i.e. tcp or rdma).\n"
	       "\t-U\t\tUnlock a RapidDisk block device (set to read-write).\n"
	       "\t-u\t\tUnmap a RapidDisk device from another block device.\n"
	       "\t-v\t\tDisplay the utility version string.\n"
	       "\t-X\t\tRemove the NVMe Target port (must be unused).\n"
	       "\t-x\t\tUnexport a RapidDisk block device from an NVMe Target.\n\n");
        printf("Example Usage:\n\trapiddisk -a 64\n"
	       "\trapiddisk -d rd2\n"
	       "\trapiddisk -r rd2 -c 128\n"
	       "\trapiddisk -m rd1 -b /dev/sdb\n"
	       "\trapiddisk -m rd1 -b /dev/sdb -p wt\n"
	       "\trapiddisk -m rd3 -b /dev/mapper/rc-wa_sdb -p wb\n"
	       "\trapiddisk -u rc-wt_sdb\n"
	       "\trapiddisk -f rd2\n"
	       "\trapiddisk -L rd2\n"
	       "\trapiddisk -U rd3\n"
	       "\trapiddisk -i eth0 -P 1 -t tcp\n"
	       "\trapiddisk -X -P 1\n"
	       "\trapiddisk -e -b rd3 -P 1 -H nqn.host1\n"
	       "\trapiddisk -R -b rd0\n"
	       "\trapiddisk -x -b rd3 -P 1 -H nqn.host1\n\n");
}

int exec_cmdline_arg(int argcin, char *argvin[])
{
	int rc = INVALID_VALUE, mode = WRITETHROUGH, action = ACTION_NONE, i, port = INVALID_VALUE, xfer = XFER_MODE_TCP;
	unsigned long long size = 0;
	bool json_flag = FALSE;
	bool header_flag = TRUE;
	char device[NAMELEN] = {0}, backing[NAMELEN] = {0}, host[NAMELEN] = {0}, header[NAMELEN] = {0};
	char generic_msg[NAMELEN] = {0};
	struct RD_PROFILE *disk = NULL;
	struct RC_PROFILE *cache = NULL;
	struct MEM_PROFILE *mem = NULL;
	struct VOLUME_PROFILE *volumes = NULL;

	sprintf(header, "%s %s\n%s\n\n", PROCESS, VERSION_NUM, COPYRIGHT);

	while ((i = getopt(argcin, argvin, "?:a:b:c:d:ef:gH:hi:jL:lm:NnP:p:qRr:s:t:U:u:VvXx")) != INVALID_VALUE) {
		switch (i) {
			case 'h':
				printf("%s", header);
				online_menu(PROCESS);
				return SUCCESS;
			case 'a':
				action = ACTION_ATTACH;
				size = strtoul(optarg, (char **)NULL, 10);
				break;
			case 'b':
				sprintf(backing, "%s", optarg);
				break;
			case 'c':
				size = strtoul(optarg, (char **)NULL, 10);
				break;
			case 'd':
				action = ACTION_DETACH;
				sprintf(device, "%s", optarg);
				break;
			case 'e':
				action = ACTION_EXPORT_NVMET;
				break;
			case 'f':
				action = ACTION_FLUSH;
				sprintf(device, "%s", optarg);
				break;
			case 'g':
				header_flag = FALSE;
				break;
			case 'H':
				sprintf(host, "%s", optarg);
				break;
			case 'i':
				action = ACTION_ENABLE_NVMET_PORT;
				sprintf(host, "%s", optarg);
				break;
			case 'j':
				json_flag = TRUE;
				break;
			case 'L':
				action = ACTION_LOCK;
				sprintf(device, "%s", optarg);
				break;
			case 'l':
				action = ACTION_LIST;
				break;
			case 'm':
				action = ACTION_CACHE_MAP;
				sprintf(device, "%s", optarg);
				break;
			case 'N':
				action = ACTION_LIST_NVMET_PORTS;
				break;
			case 'n':
				action = ACTION_LIST_NVMET;
				break;
			case 'P':
				port = atoi(optarg);
				break;
			case 'p':
				if (strcmp(optarg, "wa") == 0)
					mode = WRITEAROUND;
				else if (strcmp(optarg, "wb") == 0) {
					mode = WRITEBACK;
				}
				break;
			case 'q':
				action = ACTION_QUERY_RESOURCES;
				break;
			case 'R':
				action = ACTION_REVALIDATE_NVMET_SIZE;
				break;
			case 'r':
				action = ACTION_RESIZE;
				sprintf(device, "%s", optarg);
				break;
			case 's':
				action = ACTION_CACHE_STATS;
				sprintf(device, "%s", optarg);
				break;
			case 't':
				if (strcmp(optarg, "rdma") == 0)
					xfer = XFER_MODE_RDMA;
				break;
			case 'U':
				action = ACTION_UNLOCK;
				sprintf(device, "%s", optarg);
				break;
			case 'u':
				action = ACTION_CACHE_UNMAP;
				sprintf(device, "%s", optarg);
				break;
			case 'v':
				printf("%s", header);
				return SUCCESS;
			case 'X':
				action = ACTION_DISABLE_NVMET_PORT;
				break;
			case 'x':
				action = ACTION_UNEXPORT_NVMET;
				break;
			default:
			case '?':
				printf("%s", header);
				online_menu(PROCESS);
				return INVALID_VALUE;
		}
	}

	if (header_flag == TRUE) {
		printf("%s", header);
	}

	if ((writeback_enabled == FALSE) && (mode == WRITEBACK)) {
		print_message(-EPERM, ERR_NOWB_MODULE, json_flag);
		return -EPERM;
	}

	disk = search_rdsk_targets(generic_msg);
	if ((disk == NULL) && (strlen(generic_msg) != 0)) {
		print_message(INVALID_VALUE, generic_msg, json_flag);
		return INVALID_VALUE;
	}
	cache = search_cache_targets(generic_msg);
	if ((cache == NULL) && (strlen(generic_msg) != 0)) {
		print_message(INVALID_VALUE, generic_msg, json_flag);
		free_linked_lists(NULL, disk, NULL);
		return INVALID_VALUE;
	}

	switch(action) {
		case ACTION_ATTACH:
			if (size <= 0) {
				rc = -EINVAL;
				print_message(rc, ERR_INVALID_ARG, json_flag);
			} else {
				char mem_device_attach_msg[NAMELEN] = {0};
				rc = mem_device_attach(disk, size, mem_device_attach_msg);
				print_message(rc, mem_device_attach_msg, json_flag);
			}
			break;
		case ACTION_DETACH:
			if (disk == NULL) {
				rc = -EINVAL;
				print_message(rc, ERR_NO_DEVICES, json_flag);
				break;
			} else {
				if (strlen(device) <= 0) {
					rc = -EINVAL;
					print_message(rc, ERR_INVALID_ARG, json_flag);
					break;
				}
				rc = mem_device_detach(disk, cache, device, generic_msg);
			}
			print_message(rc, generic_msg, json_flag);
			break;
		case ACTION_FLUSH:
			rc = mem_device_flush(disk, cache, device, generic_msg);
			print_message(rc, generic_msg, json_flag);
			break;
		case ACTION_LIST:
			if (disk == NULL) {
				rc = -ENOENT;
				print_message(rc, ERR_NO_DEVICES, json_flag);
			} else {
				if (json_flag == TRUE)
					rc = json_device_list(disk, cache, NULL, FALSE);
				else
					rc = mem_device_list(disk, cache);
			}
			break;
		case ACTION_CACHE_MAP:
			if ((strlen(device) <= 0) || (strlen(backing) <= 0)) {
				rc = -EINVAL;
				print_message(rc, ERR_INVALID_ARG, json_flag);
				break;
			}
			rc = cache_device_map(disk, cache, device, backing, mode, generic_msg);
			print_message(rc, generic_msg, json_flag);
			break;
		case ACTION_RESIZE:
			if (disk == NULL) {
				rc = -EINVAL;
				print_message(rc, ERR_NO_DEVICES, json_flag);
				break;
			} else {
				if (size <= 0 || strlen(device) <= 0) {
					rc = -EINVAL;
					print_message(rc, ERR_INVALID_ARG, json_flag);
					break;
				}
				rc = mem_device_resize(disk, device, size, generic_msg);
			}
			print_message(rc, generic_msg, json_flag);
			break;
		case ACTION_CACHE_STATS:
			if (strlen(device) <= 0) {
				rc = -EINVAL;
				print_message(rc, ERR_INVALID_ARG, json_flag);
				break;
			} else {
				if (strstr(device, "rc-wb") != NULL) {
					if (!json_flag)
						rc = cache_wb_device_stat(cache, device);
					else {
						struct WC_STATS *wc_stats = NULL;
						if (cache_wb_device_stat_json(cache, device, &wc_stats) == SUCCESS) {
							rc = json_cache_wb_statistics(wc_stats, NULL, FALSE);
						}
						if (wc_stats) free(wc_stats);
					}
				} else {
					if (!json_flag)
						rc = cache_device_stat(cache, device);
					else {
						struct RC_STATS *rc_stats = NULL;
						if (cache_device_stat_json(cache, device, &rc_stats) == SUCCESS) {
							rc = json_cache_statistics(rc_stats, NULL, FALSE);
						}
						if (rc_stats) free(rc_stats);
					}
				}
			}
			break;
		case ACTION_CACHE_UNMAP:
			rc = cache_device_unmap(cache, device, generic_msg);
			print_message(rc, generic_msg, json_flag);
			break;
		case ACTION_QUERY_RESOURCES:
			mem = (struct MEM_PROFILE *)calloc(1, sizeof(struct MEM_PROFILE));
			if (get_memory_usage(mem, generic_msg) != SUCCESS) {
				if (mem) free(mem);
				mem = NULL;
				rc = -EIO;
				print_message(rc, ERR_NO_MEMUSAGE, json_flag);
				break;
			}
			volumes = search_volumes_targets(generic_msg);
			if ((volumes == NULL) && (strlen(generic_msg) != 0)) {
				if (mem) free(mem);
				mem = NULL;
				rc = INVALID_VALUE;
				print_message(rc, generic_msg, json_flag);
				break;
			}
			if (json_flag == TRUE)
				rc = json_resources_list(mem, volumes, NULL, FALSE);
			else
				rc = resources_list(mem, volumes);
			if (mem) free(mem);
			mem = NULL;
			break;
		case ACTION_LIST_NVMET_PORTS:
			rc = nvmet_view_ports(json_flag, generic_msg);
			if (rc != SUCCESS) {
				print_message(rc, generic_msg, json_flag);
			}
			break;
		case ACTION_LIST_NVMET:
			rc = nvmet_view_exports(json_flag, generic_msg);
			if (rc != SUCCESS) {
				print_message(rc, generic_msg, json_flag);
			}
			break;
		case ACTION_ENABLE_NVMET_PORT:
			if (port == INVALID_VALUE) {
				rc = -EINVAL;
				print_message(rc, ERR_INVALID_PORT, json_flag);
				break;
			}
			if (json_flag) {
				rc = nvmet_enable_port(host, port, xfer, generic_msg);
				print_message(rc, generic_msg, json_flag);
			} else {
				rc = nvmet_enable_port(host, port, xfer, NULL);
			}
			break;
		case ACTION_DISABLE_NVMET_PORT:
			if (port == INVALID_VALUE) {
				rc = -EINVAL;
				print_message(rc, ERR_INVALID_PORT, json_flag);
				break;
			}
			if (json_flag) {
				rc = nvmet_disable_port(port, generic_msg);
				print_message(rc, generic_msg, json_flag);
			} else {
				rc = nvmet_disable_port(port, NULL);
			}
			break;
		case ACTION_EXPORT_NVMET:
			if (strlen(backing) <= 0) {
				rc = -EINVAL;
				print_message(rc, ERR_INVALID_ARG, json_flag);
				break;
			}
			if ((disk == NULL) && (cache == NULL)) {
				rc =  SUCCESS;
				print_message(rc, ERR_NO_DEVICES, json_flag);
				break;
			}
			if (json_flag) {
				rc = nvmet_export_volume(disk, cache, backing, host, port, generic_msg);
				print_message(rc, generic_msg, json_flag);
			} else {
				rc = nvmet_export_volume(disk, cache, backing, host, port, NULL);
			}
			break;
		case ACTION_UNEXPORT_NVMET:
			if (strlen(backing) <= 0){
				rc = -EINVAL;
				print_message(rc, ERR_INVALID_ARG, json_flag);
				break;
			}
			if (json_flag) {
				rc = nvmet_unexport_volume(backing, host, port, generic_msg);
				print_message(rc, generic_msg, json_flag);
			} else {
				rc = nvmet_unexport_volume(backing, host, port, NULL);
			}
			break;
		case ACTION_LOCK:
			if (strlen(device) <= 0){
				rc = -EINVAL;
				print_message(rc, ERR_INVALID_ARG, json_flag);
				break;
			}
			rc = mem_device_lock(disk, device, TRUE, generic_msg);
			print_message(rc, generic_msg, json_flag);
			break;
		case ACTION_UNLOCK:
			if (strlen(device) <= 0){
				rc = -EINVAL;
				print_message(rc, ERR_INVALID_ARG, json_flag);
				break;
			}
			rc = mem_device_lock(disk, device, FALSE, generic_msg);
			print_message(rc, generic_msg, json_flag);
			break;
		case ACTION_REVALIDATE_NVMET_SIZE:
			if (strlen(backing) <= 0){
				rc = -EINVAL;
				print_message(rc, ERR_INVALID_ARG, json_flag);
				break;
			}
			if (json_flag) {
				rc = nvmet_revalidate_size(disk, cache, backing, generic_msg);
				print_message(rc, generic_msg, json_flag);
			} else {
				rc = nvmet_revalidate_size(disk, cache, backing, NULL);
			}
			break;
		default:
		case ACTION_NONE:
			if (header_flag == FALSE) {
				printf("%s", header);
			}
			online_menu(PROCESS);
			break;
	}

	free_linked_lists(cache, disk, volumes);
	cache = NULL;
	disk = NULL;
	volumes = NULL;
	return rc;

}

/**
 * It checks if the user is root, then checks if the required kernel modules are loaded, then executes the command line
 * arguments
 *
 * @param argc The number of arguments passed to the program.
 * @param argv The command line arguments
 *
 * @return The return code of the last executed command.
 */
int main(int argc, char *argv[])
{
	int rc = INVALID_VALUE;

	/* This preprocessor if/endif is useful when the debbugger can't run as root */
#ifndef DEBUG
	if (geteuid() != 0) {
		printf("\nYou must be root or contain sudo permissions to initiate this\n\n");
		return -EACCES;
	}
#endif

	writeback_enabled = FALSE;
	rc = check_loaded_modules();
	if (rc != SUCCESS) {
		if (rc == 1)
			writeback_enabled = TRUE;
		else
			return -EPERM;
	}

	rc = exec_cmdline_arg(argc, argv);
	return rc;
}
