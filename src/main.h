/**
 * @file
 * @brief rapiddisk tool definitions
 * @details This header file defines some constants used by the rapiddisk tool
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

#ifndef MAIN_H
#define MAIN_H

#define ACTION_NONE					0x0
#define ACTION_ATTACH				0x1
#define ACTION_DETACH				0x2
#define ACTION_FLUSH				0x3
#define ACTION_LIST					0x4
#define ACTION_CACHE_MAP			0x5
#define ACTION_RESIZE				0x6
#define ACTION_CACHE_STATS			0x7
#define ACTION_CACHE_UNMAP			0x8
#define ACTION_QUERY_RESOURCES		0x9
#define ACTION_LIST_NVMET_PORTS		0xa
#define ACTION_LIST_NVMET			0xb
#define ACTION_ENABLE_NVMET_PORT	0xc
#define ACTION_DISABLE_NVMET_PORT	0xd
#define ACTION_EXPORT_NVMET			0xe
#define ACTION_UNEXPORT_NVMET		0xf
#define ACTION_LOCK					0x10
#define ACTION_UNLOCK				0x11
#define ACTION_REVALIDATE_NVMET_SIZE	0x12

#define ERR_INVALID_ARG		"Error. Invalid argument(s) or values entered."
#define ERR_NOWB_MODULE		"Please ensure that the dm-writecache module is loaded and retry."
#define ERR_NO_DEVICES		"Unable to locate any RapidDisk devices."
#define ERR_NO_MEMUSAGE		"Error. Unable to retrieve memory usage data."
#define ERR_INVALID_PORT	"Error. Invalid port number."

#endif
