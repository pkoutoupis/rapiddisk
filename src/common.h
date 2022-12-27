/**
 * @file
 * @brief Common structure and value definitions
 * @details This header file defines constants and structures shared by the rapiddisk and the rapiddiskd source
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
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <stddef.h>

/** Rapiddisk Process name */
#define PROCESS			"rapiddisk"
/** Rapiddiskd (daemon) Process name */
#define DAEMON			PROCESS "d"
#define COPYRIGHT		"Copyright 2011 - 2022 Petros Koutoupis"
#define VERSION_NUM	  	"8.2.0"
#define SUCCESS			0
#define INVALID_VALUE		-1
#define NAMELEN			0x200
#define BUFSZ			0x10000
#define PAYLOADSZ		0x80000 /* 512K: this is our max read limit for libcurl */

/** Internal bool like type */
typedef char bool;

#define FALSE			0
#define TRUE			1

#define SYS_RDSK		"/sys/kernel/rapiddisk/mgmt"
#define SYS_MODULE		"/sys/module"

#define FILEDATA			0x40

#define NAMELEN				0x200

#define DISABLED			0
#define ENABLED				1

#define ERR_CALLOC			"%s: calloc: %s"
#define ERR_FLUSHING		"Error flushing file descriptors: %s, %s."
#define ERR_MALFORMED		"Error: wrong number of arguments or malformed URL."
#define ERR_INVALIDURL		"Invalid URL"
#define ERR_INVALIDDEVNAME	"Invalid device name."
#define ERR_DEV_STATUS		"Can't get device status"
#define ERR_UNSUPPORTED		"Unsupported"
#define ERR_INVALID_SIZE	"Invalid size"
#define ERR_NOTANUMBER		"Not a number."
#define ERR_SCANDIR			"%s: scandir: %s"
#define ERR_FOPEN			"%s: fopen: %s, %s"
#define ERR_FREAD			"%s: fread: %s, %s"
#define ERR_MODULES			"%s, The needed modules are not loaded..."
#define ERR_ALREADY_RUNNING	"%s, The daemon is already running..."
#define ERR_NEW_MHD_DAEMON 	"Error creating MHD Daemon: %s, %s."
#define ERR_SIGPIPE_HANDLER	"Failed to install SIGPIPE handler: %s, %s"
#define ERR_INVALID_MODE	"Invalid cache mode in URL."

/**
 * For RapidDisk device list
 */
typedef struct RD_PROFILE {
	/** Device name */
	char device[0xf];
	/** Device size */
	unsigned long long size;
	/** Lock status */
	int lock_status;
	/** Device usage */
	unsigned long long usage;
	/** Pointer to next RapidDisk device */
	struct RD_PROFILE *next;
} RD_PROFILE;

/**
 * For RapidDisk-Cache node list
 */
typedef struct RC_PROFILE {
	/** Device name */
	char device[NAMELEN];
	/** Cache name */
	char cache[0x20];
	/** Cache source */
	char source[NAMELEN];
	/** Pointer to next RapidDisk-Cache node */
	struct RC_PROFILE *next;
} RC_PROFILE;

/**
 * Represents current memory status
 */
typedef struct MEM_PROFILE {
	/** Total memory */
	unsigned long long mem_total;
	/** Free memory */
	unsigned long long mem_free;
} MEM_PROFILE;

/**
 * Represents a volume list
 */
typedef struct VOLUME_PROFILE {
	/** Volume device name */
	char device[0x20];
	/** Volume size */
	unsigned long long size;
	/** Volume vendor */
	char vendor[FILEDATA];
	/** Volume model */
	char model[FILEDATA];
	/** Pointer to next volume */
	struct VOLUME_PROFILE *next;
} VOLUME_PROFILE;

typedef struct RC_STATS {
	char device[NAMELEN];
	unsigned int reads;
	unsigned int writes;
	unsigned int cache_hits;
	unsigned int replacement;
	unsigned int write_replacement;
	unsigned int read_invalidates;
	unsigned int write_invalidates;
	unsigned int uncached_reads;
	unsigned int uncached_writes;
	unsigned int disk_reads;
	unsigned int disk_writes;
	unsigned int cache_reads;
	unsigned int cache_writes;

	/* Unsupported in this release */
	unsigned int read_ops;
	unsigned int write_ops;
} RC_STATS;

typedef struct WC_STATS {
	char device[NAMELEN];
	bool expanded;
	int errors;
	unsigned int num_blocks;
	unsigned int num_free_blocks;
	unsigned int num_wb_blocks;
	/* For 5.15 and later */
	unsigned int num_read_req;
	unsigned int num_read_cache_hits;
	unsigned int num_write_req;
	unsigned int num_write_uncommitted_blk_hits;
	unsigned int num_write_committed_blk_hits;
	unsigned int num_write_cache_bypass;
	unsigned int num_write_cache_alloc;
	unsigned int num_write_freelist_blocked;
	unsigned int num_flush_req;
	unsigned int num_discard_req;
} WC_STATS;

enum CACHE_TYPE {
	WRITETHROUGH,
	WRITEAROUND,
	WRITEBACK
};

typedef struct NVMET_PROFILE {
	char nqn[NAMELEN];
	int namespc;
	char device[0x1F];
	int enabled;
	struct NVMET_PROFILE *next;
} NVMET_PROFILE;

typedef struct NVMET_PORTS {
	int port;
	char addr[0x1F];
	char nqn[NAMELEN];
	char protocol[0xF];
	struct NVMET_PORTS *next;
} NVMET_PORTS;

typedef struct PTHREAD_ARGS {
	bool verbose;
	char port[0xf];
	char path[NAMELEN];
} PTHREAD_ARGS;

#endif
