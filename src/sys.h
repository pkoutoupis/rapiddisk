/**
 * @file
 * @brief System-related function definitions
 * @details This header file defines system-related functions
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
#ifndef SYS_H
#define SYS_H

#include "common.h"

#ifndef SERVER
int resources_list(struct MEM_PROFILE *, struct VOLUME_PROFILE *);
#endif
int get_memory_usage(struct MEM_PROFILE *, char *return_message);
struct VOLUME_PROFILE *search_volumes_targets(char *return_message);

#endif //SYS_H
