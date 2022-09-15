/* rxio.c */

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
#include <malloc.h>

#define XFER_SIZE         4096
#define BYTES_PER_BLOCK   512

int main () {
	int fd;
	unsigned long long size;
	char *buf;
	off_t offset = 0;

	buf = (char *)malloc(XFER_SIZE);
	memset(buf, 0x2F, XFER_SIZE);

	if ((fd = open("/dev/rd0", O_RDWR, O_NONBLOCK)) < 0) {
		printf("%s\n", strerror(errno));
		return errno;
	}

	if ((ioctl(fd, BLKGETSIZE, &size)) == -1) {
		printf("%s\n", strerror(errno));
		return errno;
	} else {
		printf("total block count: %llu\n", size);
		printf("total bytes count: %llu\n", (size * BYTES_PER_BLOCK));
	}

	if ((write (fd, buf, XFER_SIZE)) <= 0) {
		printf("%s\n", strerror(errno));
		return errno;
	}
	printf ("wrote %d bytes at offset %lu\n", XFER_SIZE, offset);

	offset = (offset + 65536);
	if ((lseek (fd, offset, SEEK_SET)) != offset) {
		printf("%s\n", strerror(errno));
		return errno;
	}
	printf ("seeked to offset %lu\n", offset);

	if ((read(fd, buf, XFER_SIZE)) <= 0) {
		printf("%s\n", strerror(errno));
		return errno;
	}
	printf ("read %d bytes at offset %lu\n", XFER_SIZE, offset);

	close (fd);

	return 0;
}
