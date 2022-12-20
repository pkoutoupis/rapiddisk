/**
 * @file
 * @brief Utility functions declarations
 * @details This header file defines some utility functions
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
#ifndef UTILS_H
#define UTILS_H

#include "common.h"

int preg_replace(const char *re, char *replacement, char *subject, char *result, size_t pcre2_result_len);
int split(char* input_string, char** output_arr, char* delim);
void free_linked_lists(RC_PROFILE *rc_head, RD_PROFILE *rd_head, VOLUME_PROFILE *vp_head);
void free_nvmet_linked_lists(struct NVMET_PORTS *ports_head, struct NVMET_PROFILE *nvmet_head);
struct dirent **clean_scandir(struct dirent **scanlist, int num);
int check_loaded_modules(void);
void print_message(int ret_value, char *message, bool json_flag);
char *verbose_msg(char *dest, char *msg);

#endif //UTILS_H
