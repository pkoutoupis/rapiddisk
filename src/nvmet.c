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
#define SYS_NVMET_HOSTS		SYS_NVMET "/hosts"
#define SYS_CLASS_NVME		"/sys/class/nvme"
#define NQN_HDR_STR		"nqn.2021-06.org.rapiddisk:"    /* Followed by <hostname>.<target> */
#define SYS_CLASS_NET		"/sys/class/net"
#define NVME_PORT		"4420"

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
 * description: Scan all NVMe Targets ports.
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

/*
 * description: List NVMe Target exports / ports.
 */
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
			printf("%s: mkdir: %s\n", __func__, strerror(errno));
			return INVALID_VALUE;
		}
	}

	/* Set host NQNs to access target */
	if (strlen(host) == 0) {
		sprintf(path, "%s/%s%s-%s/allowed_hosts", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if ((err = scandir(path, &list, NULL, NULL)) < 0) {
			printf("%s: scandir: %s\n", __func__, strerror(errno));
			return INVALID_VALUE;
		}
		for (n = 0; n < err; n++) if (list[n] != NULL) free(list[n]);    /* clear list */
		if (err > 2) {
			printf("One or more hosts exist. Please remove existing host or define a new one.\n");
			return INVALID_VALUE;
		}

		sprintf(path, "%s/%s%s-%s/attr_allow_any_host", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if((fp = fopen(path, "w")) == NULL){
			printf("%s: fopen: %s\n", __func__, strerror(errno));
			return INVALID_VALUE;
		}
		fprintf(fp, "1");
		fclose(fp);
	} else {
		/* Configure the target to be seen only by the specified host(s) */

		sprintf(path, "%s/%s", SYS_NVMET_HOSTS, host);
		if (access(path, F_OK) != SUCCESS) {
			if ((rc = mkdir(path, mode)) != SUCCESS) {
				printf("%s: mkdir: %s\n", __func__, strerror(errno));
				return INVALID_VALUE;
			}
		}
		sprintf(path, "%s/%s", SYS_NVMET_HOSTS, host);
		sprintf(path2, "%s/%s%s-%s/allowed_hosts/%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device, host);

		if (access(path2, F_OK) != SUCCESS) {
			rc = symlink(path, path2);
			if (rc != SUCCESS) {
				printf("%s: symlink: %s\n", __func__, strerror(errno));
				return rc;
			}
		}

		/* Make sure that no other hosts can access the target */
		sprintf(path, "%s/%s%s-%s/attr_allow_any_host", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if((fp = fopen(path, "w")) == NULL){
			printf("%s: fopen: %s\n", __func__, strerror(errno));
			return INVALID_VALUE;
		}
		fprintf(fp, "0");
		fclose(fp);
	}

	/* Create namespace */
	sprintf(path, "%s/%s%s-%s/namespaces/1", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if (access(path, F_OK) != SUCCESS) {
		if ((rc = mkdir(path, mode)) != SUCCESS) {
			printf("%s: mkdir: %s\n", __func__, strerror(errno));
			return rc;
		}
	}

	/* Set device */
	sprintf(path, "%s/%s%s-%s/namespaces/1/device_path", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
	if((fp = fopen(path, "w")) == NULL){
		printf("%s: fopen: %s\n", __func__, strerror(errno));
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
		printf("%s: fopen: %s\n", __func__, strerror(errno));
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
				printf("%s: symlink: %s\n", __func__, strerror(errno));
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
						printf("%s: symlink: %s\n", __func__, strerror(errno));
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
 * description: Unexport an NVMe Target.
 */
int nvmet_unexport_volume(unsigned char *device, unsigned char *host, int port)
{
	int rc = INVALID_VALUE, n;
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

	// rm -rf /sys/kernel/config/nvmet/subsystems/$NQN/allowed_hosts/$HOST
	// if the last used mapping to host, then...
	// rm -rf /sys/kernel/config/nvmet/hosts/$HOST
	// if host == NULL, then remove all hosts
	// TODO: if a host is defined, just unmap and exit.

	// TODO: if port equal INVALID_VALUE, then iterate through all ports and remove the symlink and if
	if (port != INVALID_VALUE) {
		if (access(path, F_OK) == SUCCESS) {
			sprintf(path, "%s/%d/subsystems/%s%s-%s", SYS_NVMET_PORTS, port, NQN_HDR_STR, hostname, device);
			rc = unlink(path);
			if (rc != SUCCESS) {
				printf("%s: unlink: %s\n", __func__, strerror(errno));
				return rc;
			}
		}
	}
	// TODO: if a port is defined, just unmap and exit.

	if ((strlen(host) == 0) && (port == INVALID_VALUE)) {
		if (access(path, F_OK) == SUCCESS) {
			/* Disable volume */
			sprintf(path, "%s/%s%s-%s/namespaces/1/enable", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
			if((fp = fopen(path, "w")) == NULL){
				printf("%s: fopen: %s\n", __func__, strerror(errno));
				return INVALID_VALUE;
			}
			fprintf(fp, "0");
			fclose(fp);
		}

		//TODO: Check that no other namespace exists
		sprintf(path, "%s/%s%s-%s/namespaces/1", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if (access(path, F_OK) == SUCCESS) {
			if ((rc = rmdir(path)) != SUCCESS) {
				printf("%s: rmdir: %s\n", __func__, strerror(errno));
				return rc;
			}
		}

		/* Remove NQN from NVMeT Subsystem */
		sprintf(path, "%s/%s%s-%s", SYS_NVMET_TGT, NQN_HDR_STR, hostname, device);
		if (access(path, F_OK) == SUCCESS) {
			if ((rc = rmdir(path)) != SUCCESS) {
				printf("%s: rmdir: %s\n", __func__, strerror(errno));
				return rc;
			}
		}
	}

	return SUCCESS;
}
