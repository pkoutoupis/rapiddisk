/* rxio.c */
  
/** Copyright © 2020 Petros Koutoupis
 ** All rights reserved.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **
 ** SPDX-License-Identifier: GPL-2.0-or-later
 **/

#include "../src/common.h"

int cache_status_mapping(void);

int main(int argc, char *argv[])
{
	int rc = INVALID_VALUE;

	if (getuid() != 0) {
		printf("\nYou must be root or contain sudo permissions\n\n");
		return -EACCES;
	}

	if ((argc < 2) || (argc > 4)) {
		printf("\nInvalid argument.\n\n");
		return INVALID_VALUE;
	}

	if (strcmp(argv[1], "-s") == 0) {
		rc = cache_status_mapping();
	} else if (strcmp(argv[1], "-a") == 0) {
		rc = cache_map(argv[2], 64, WRITEAROUND);
	} else if (strcmp(argv[1], "-u") == 0) {
		rc = cache_unmap();
	} else {
		printf("\nInvalid argument.\n\n");
	}

	return rc;
}
