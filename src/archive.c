/*********************************************************************************
 ** Copyright © 2011 - 2018 Petros Koutoupis
 ** All rights reserved.
 **
 ** This file is part of RapidDisk.
 **
 ** RapidDisk is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** RapidDisk is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with RapidDisk.  If not, see <http://www.gnu.org/licenses/>.
 **
 ** @project: rapiddisk
 **
 ** @filename: archive.c
 ** @description: This is the main file for the rapiddisk userland tool.
 **
 ** @date: 15Nov11, petros@petroskoutoupis.com
 ********************************************************************************/

#include "common.h"
#include <assert.h>
#include <sys/ioctl.h>
#include <zlib.h>

#define RD_GET_STATS	 0x0529

int archive_rd_volume(struct RD_PROFILE *prof, unsigned char *src,
		       unsigned char *dest)
{
	int err, flush, fd;
	unsigned long long max_sect, cnt;
	unsigned int have;
	FILE *fin, *fout;
	unsigned char *in, *out, file[16] = {0};
	z_stream strm;

	while (prof != NULL) {
		if (strcmp(src, prof->device) == 0)
			err = 0;
		prof = prof->next;
	}
	if (err != 0) {
		printf("Error. Device %s does not exist.\n", src);
		return -ENOENT;
	}

	sprintf(file, "/dev/%s", src);
	in = (char *)malloc(BUFSZ);
	out = (char *)malloc(BUFSZ);

	if ((in == NULL) || (out == NULL)) {
		printf("Error. Unable to acquire memory space.\n");
		return -ENOMEM;
	}

	if ((fd = open(file, O_RDONLY | O_NONBLOCK)) < 0) {
		printf("fdin: %s\n", strerror(errno));
		return -errno;
	}
	/* This ioctl will obtain the last sector allocate as to not   *
	 * archive the unallocated pages which will just return zeroes.*
	 * This will allow for a restoration utilizing less pages.     */
	if (ioctl(fd, RD_GET_STATS, &max_sect) < 0) {
		printf("ioctl: %s\n", strerror(errno));
		return -errno;
	}
	close(fd);

	if ((fin = fopen((char *)file, "r")) == NULL) {
		printf("%s: fopen: %s\n", __func__, strerror(errno));
		return -errno;
	}

	if ((fout = fopen((char *)dest, "w")) == NULL) {
		printf("%s: fopen: %s\n", __func__, strerror(errno));
		return -errno;
	}

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	if ((err = deflateInit(&strm, Z_DEFAULT_COMPRESSION)) != Z_OK)
		return err;

	printf("Initiating the archival process. Currently deflating...\n");

	do {
		strm.avail_in = fread(in, 1, BUFSZ, fin);
		if (ferror(fin)) {
			(void)deflateEnd(&strm);
			return Z_ERRNO;
		}
		flush = (feof(fin) || cnt >= (max_sect * BYTES_PER_SECTOR)) \
			 ? Z_FINISH : Z_NO_FLUSH;
		strm.next_in = in;

		do {
			strm.avail_out = BUFSZ;
			strm.next_out = out;
			err = deflate(&strm, flush);
			assert(err != Z_STREAM_ERROR);
			have = BUFSZ - strm.avail_out;
			if (fwrite(out, 1, have, fout) != have || ferror(fout)) {
				(void)deflateEnd(&strm);
				return Z_ERRNO;
			}
			cnt += have;
		} while (strm.avail_out == 0);
		assert(strm.avail_in == 0);
	} while (flush != Z_FINISH);
	assert(err == Z_STREAM_END);

	printf("Done.\n");

	(void)deflateEnd(&strm);
	fclose(fin);
	fclose(fout);
	return Z_OK;
}

int restore_rd_volume(struct RD_PROFILE *prof, unsigned char *src,
		       unsigned char *dest)
{
	int err;
	unsigned int have;
	FILE *fin, *fout;
	unsigned char *in, *out, file[16] = {0};
	z_stream strm;

	while (prof != NULL) {
		if (strcmp(dest, prof->device) == 0) {
			err = 0;
		}
		prof = prof->next;
	}
	if (err != 0) {
			printf("Error. Device %s does not exist.\n", dest);
			return -ENOENT;
	}

	sprintf(file, "/dev/%s", dest);
	in = (char *)malloc(BUFSZ);
	out = (char *)malloc(BUFSZ);

	if ((in == NULL) || (out == NULL)) {
		printf("Error. Unable to acquire memory space.\n");
		return -ENOMEM;
	}

	if ((fin = fopen((char *)src, "r")) == NULL) {
		printf("%s: fopen: %s\n", __func__, strerror(errno));
		return -errno;
	}

	if ((fout = fopen((char *)file, "w")) == NULL) {
		printf("%s: fopen: %s\n", __func__, strerror(errno));
		return -errno;
	}

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	if ((err = inflateInit(&strm)) != Z_OK)
		return err;

	printf("Initiating the restoration process. Currently inflating...\n");

	do {
		strm.avail_in = fread(in, 1, BUFSZ, fin);
		if (ferror(fin)) {
			(void)inflateEnd(&strm);
			return Z_ERRNO;
		}
		if (strm.avail_in == 0)
			break;
		strm.next_in = in;

		do {
			strm.avail_out = BUFSZ;
			strm.next_out = out;
			err = inflate(&strm, Z_NO_FLUSH);
			assert(err != Z_STREAM_ERROR);
			switch (err) {
			case Z_NEED_DICT:
				err = Z_DATA_ERROR;
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				(void)inflateEnd(&strm);
				return err;
			}
			have = BUFSZ - strm.avail_out;
			if (fwrite(out, 1, have, fout) != have || ferror(fout)) {
				(void)inflateEnd(&strm);
				return Z_ERRNO;
			}
		} while (strm.avail_out == 0);
	} while (err != Z_STREAM_END);

	printf("Done.\n");

	(void)inflateEnd(&strm);
	fclose(fin);
	fclose(fout);
	return (err == Z_STREAM_END) ? Z_OK : Z_DATA_ERROR;
}
