/**
 * @file
 * @brief JSON function declarations
 * @details This header file declares some JSON related functions
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

#ifndef JSON_H
#define JSON_H

#include "common.h"

int json_device_list(struct RD_PROFILE *rd, struct RC_PROFILE *rc, char **list_result, bool wantresult);
int json_resources_list(struct MEM_PROFILE *mem, struct VOLUME_PROFILE *volume, char **list_result, bool wantresult);
int json_cache_statistics(struct RC_STATS *stats, char **stats_result, bool wantresult);
int json_cache_wb_statistics(struct WC_STATS *stats, char **stats_result, bool wantresult);
int json_status_return(int return_value, char *optional_message, char **json_result, bool wantresult);
int json_nvmet_view_exports(struct NVMET_PROFILE *nvmet, struct NVMET_PORTS *ports, char **json_result, bool wantresult);
int json_nvmet_view_ports(struct NVMET_PORTS *ports, char **json_result, bool wantresult);
#ifdef SERVER
int json_status_check(char **json_str);
#endif

#endif //JSON_H
