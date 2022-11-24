/**
 * @file
 * @brief Disk functions and constants
 * @details This header file defines constants and functions related to disk access
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

#ifndef RDSK_H
#define RDSK_H

#include "common.h"

#define BYTES_PER_SECTOR	0x200
#define SYS_BLOCK			"/sys/block"
#define ETC_MTAB			"/etc/mtab"
#define DEV_MAPPER			"/dev/mapper"
#define RD_GET_USAGE		0x0530
#define PAGE_SIZE			0x1000

void print_error(char *format_string, char *return_message, ...);
char *read_info(char *name, char *string, char *return_message);
unsigned long long mem_device_get_usage(char *);
int mem_device_lock_status(char *);
struct RD_PROFILE *search_rdsk_targets(char *return_message);
struct RC_PROFILE *search_cache_targets(char *return_message);
void *dm_get_status(char *device, enum CACHE_TYPE cache_type);
int dm_create_mapping(char* device, char *table);
int cache_device_map(struct RD_PROFILE *rd_prof, struct RC_PROFILE *rc_prof, char *ramdisk, char *block_dev, int cache_mode, char *return_message);
int mem_device_resize(struct RD_PROFILE *prof, char *string, unsigned long long size, char *return_message);
int mem_device_attach(struct RD_PROFILE *, unsigned long long, char *return_message);
int mem_device_detach(struct RD_PROFILE *, struct RC_PROFILE *, char *, char *return_message);
int mem_device_lock(struct RD_PROFILE *, char *, bool, char *return_message);
int cache_device_unmap(struct RC_PROFILE *, char *, char *return_message);
int dm_remove_mapping(char* device);
int dm_flush_device(char *device);
int mem_device_flush(struct RD_PROFILE *rd_prof, RC_PROFILE *rc_prof, char *string, char *return_message);
#ifndef SERVER
int cache_device_stat(struct RC_PROFILE *rc_profile, char *cache);
int cache_device_stat_json(struct RC_PROFILE *rc_prof, char *cache, RC_STATS **rc_stats);
int cache_wb_device_stat(struct RC_PROFILE *rc_prof, char *cache);
int cache_wb_device_stat_json(struct RC_PROFILE *rc_prof, char *cache, WC_STATS **wc_stats);
int mem_device_list(struct RD_PROFILE *, struct RC_PROFILE *);
#endif

#endif //RDSK_H
