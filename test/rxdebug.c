/* rxdebug.c */

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

#define XFER_SIZE               4096
#define DEBUG_MODE_WRITE_ERR    0x0532
#define DEBUG_MODE_READ_ERR     0x0533

int main () {
	int fd;
	unsigned short rc = -1;
	unsigned long long max_usage;
	unsigned char *buf;

	buf = (char *)malloc(XFER_SIZE);

	printf("Setting debug mode: error on writes.\n");
	if ((fd = open("/dev/rd0", O_WRONLY)) < 0) {
		printf("%s\n", strerror(errno));
		return errno;
	}

	if (ioctl(fd, DEBUG_MODE_WRITE_ERR, &rc) == -1) {
		printf("%s\n", strerror(errno));
		return errno;
	} else {
		printf("debug mode (write) set: %u\n", rc);
	}

	close (fd);

	memset(buf, 0x2F, XFER_SIZE);
	printf("Writing to the device.\n");
	if ((fd = open("/dev/rd0", O_RDWR, O_NONBLOCK)) < 0) {
		printf("%s\n", strerror(errno));
		return errno;
	}

	if ((write (fd, buf, XFER_SIZE)) <= 0) {
		printf("%s\n", strerror(errno));
	}

	close (fd);

	printf("Setting debug mode: error on reads.\n");
	if ((fd = open("/dev/rd0", O_WRONLY)) < 0) {
		printf("%s\n", strerror(errno));
		return errno;
	}

	if (ioctl(fd, DEBUG_MODE_READ_ERR, &rc) == -1) {
		printf("%s\n", strerror(errno));
		return errno;
	} else {
		printf("debug mode (read) set: %u\n", rc);
	}

	close (fd);

	memset(buf, 0x0, XFER_SIZE);
	printf("Reading from the device.\n");
	if ((fd = open("/dev/rd0", O_RDWR, O_NONBLOCK)) < 0) {
		printf("%s\n", strerror(errno));
		return errno;
	}

	if ((read(fd, buf, XFER_SIZE)) <= 0) {
		printf("%s\n", strerror(errno));
	}

	close (fd);

	printf("Unsetting debug mode: error on writes.\n");
	if ((fd = open("/dev/rd0", O_WRONLY)) < 0) {
		printf("%s\n", strerror(errno));
		return errno;
	}

	if (ioctl(fd, DEBUG_MODE_WRITE_ERR, &rc) == -1) {
		printf("%s\n", strerror(errno));
		return errno;
	} else {
		printf("debug mode (write) set: %u\n", rc);
	}

	close (fd);

	memset(buf, 0x2F, XFER_SIZE);
	printf("Writing to the device.\n");
	if ((fd = open("/dev/rd0", O_RDWR, O_NONBLOCK)) < 0) {
		printf("%s\n", strerror(errno));
		return errno;
	}

	if ((write (fd, buf, XFER_SIZE)) <= 0) {
		printf("%s\n", strerror(errno));
	}

	close (fd);

	printf("Unsetting debug mode: error on reads.\n");
	if ((fd = open("/dev/rd0", O_WRONLY)) < 0) {
		printf("%s\n", strerror(errno));
		return errno;
	}

	if (ioctl(fd, DEBUG_MODE_READ_ERR, &rc) == -1) {
		printf("%s\n", strerror(errno));
		return errno;
	} else {
		printf("debug mode (read) set: %u\n", rc);
	}

	close (fd);

	memset(buf, 0x0, XFER_SIZE);
	printf("Reading from the device.\n");
	if ((fd = open("/dev/rd0", O_RDWR, O_NONBLOCK)) < 0) {
		printf("%s\n", strerror(errno));
		return errno;
	}

	if ((read(fd, buf, XFER_SIZE)) <= 0) {
		printf("%s\n", strerror(errno));
	}

	close (fd);

	return 0;
}
