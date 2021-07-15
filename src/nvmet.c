/*********************************************************************************
 ** Copyright (c) 2015 - 2021 Petros Koutoupis
 ** All rights reserved.
 **
 ** @date: 18Jul17, petros@hyve.io
 ********************************************************************************/

#include "common.h"
#include "cli.h"
#include <net/if.h>


#define SYS_NVMET		"/sys/kernel/config/nvmet"
#define SYS_NVMET_TGT		SYS_NVMET "/subsystems"
#define SYS_NVMET_PORTS		SYS_NVMET "/ports"
#define SYS_CLASS_NVME		"/sys/class/nvme"
#define NQN_HDR_STR		"nqn.2021-11.io.hyve:"    /* Followed by <hostname>.<target> */
#define SYS_CLASS_NET		"/sys/class/net"
#define NVME_PORT		"4420"

struct NVMET_PROFILE *nvmet_head =  (struct NVMET_PROFILE *) NULL;
struct NVMET_PROFILE *nvmet_end =   (struct NVMET_PROFILE *) NULL;

struct NVMET_PORTS *ports_head =  (struct NVMET_PORTS *) NULL;
struct NVMET_PORTS *ports_end =   (struct NVMET_PORTS *) NULL;

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
						sprintf(nvmet_ports->nqn, "%s", exports[i]->d_name);
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

int nvmet_view_exports(bool json_flag)
{
	int i = 1;
	struct NVMET_PROFILE *nvmet, *tmp;
	struct NVMET_PORTS *ports, *tmp_ports;
	unsigned char str[NAMELEN] = {0x0};

	nvmet = (struct NVMET_PROFILE *)nvmet_scan_subsystem();
	ports = (struct NVMET_PORTS *)nvmet_scan_ports();

	if (json_flag == TRUE)
		return json_nvmet_view_exports(nvmet, ports);

	printf("NVMe Target Exports\n\n");
	if (nvmet == NULL) {
		printf("\tNone.\n\n");
		return SUCCESS;
	}
	while (nvmet != NULL) {
		printf("\t%d: NQN: %s \tNamespace: %d\tDevice: %s \tEnabled: %s\n",
		       i, nvmet->nqn, nvmet->namespc, nvmet->device,
		       ((nvmet->enabled == 0) ? "False" : "True"));
		i++;
		tmp = nvmet;
		nvmet = nvmet->next;
		free(tmp);
	}

	i = 1;
	printf("\nNVMe Ports\n\n");
	if (ports == NULL) {
		printf("\tNone.\n\n");
		return SUCCESS;
	}

	while (ports != NULL) {
		printf("\t%d: Port: %d - %s\tNQN: %s\n", i, ports->port, ports->addr, ports->nqn);
		i++;
		tmp_ports = ports;
		ports = ports->next;
		free(tmp_ports);
	}
	return SUCCESS;
}
