/* rxflush.c */

/** Copyright © 2016 - 2022 Petros Koutoupis
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


#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <errno.h>
#include <string.h>

int main () {
	int fd, rc;

	if ((fd = open("/dev/rd0", O_WRONLY)) < 0) {
		printf("%s\n", strerror(errno));
		return errno;
	}

	rc = ioctl(fd, BLKFLSBUF, 0);
	printf("Sent ioctl() BLKFLSBUF and got return: %d\n", rc);

	close (fd);

	return rc;
}
