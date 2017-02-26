/*********************************************************************************
 ** Copyright © 2011-2017 Petros Koutoupis
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
 ** @filename: crypt.c
 ** @description: File for rapiddisk.
 **
 ** @date: 21Nov14, petros@petroskoutoupis.com
 ********************************************************************************/

#include "common.h"
#if !defined NO_CRYPT
#define _GNU_SOURCE
#include <libcryptsetup.h>


int format_crypt(unsigned char *device)
{
	struct crypt_device *cd;
	struct crypt_params_luks1 params;
	char *key;
	FILE *fp;
	int err;
	size_t len;
	ssize_t read;

	if ((err = crypt_init(&cd, device)) < 0) {
		printf("%s: crypt_init: %s\n", __func__, strerror(errno));
		return err;
	}
	printf("Device %s will be formatted to a LUKS device after 5 seconds.\n"
	       "Press CTRL+C now if you want to cancel this operation.\n", device);
	sleep(5);

	params.hash = "sha1";
	params.data_alignment = 0;
#if !defined LEGACY_CRYPT
	params.data_device = NULL;
#endif

	if (access(KEY_FILE, F_OK) == -1) {
		key = strdup(DEFAULT_DES_KEY);
		len = strlen(key);
	} else {
		fp = fopen(KEY_FILE, "r");
		if ((read = getline(&key, &len, fp)) == -1) {
			printf("%s: getline: %s\n", __func__, strerror(errno));
			return errno;
		}
		len = (read - 1);
		fclose(fp);
	}

	if ((err = crypt_format(cd, CRYPT_LUKS1, "aes", "xts-plain64",
	    NULL, NULL, 256 / 8, &params)) < 0) {
		printf("%s: crypt_format: %s\n", __func__, strerror(errno));
		syslog(LOG_ERR, "%s: crypt_format: %s\n",
		       __func__, strerror(errno));
		crypt_free(cd);
		return err;
	}

	if ((err = crypt_keyslot_add_by_volume_key(cd, CRYPT_ANY_SLOT, NULL,
	    0, key, len)) < 0) {
		printf("%s: crypt_keyslot_add_by_volume_key: %s\n",
		       __func__, strerror(errno));
		syslog(LOG_ERR, "%s: crypt_keyslot_add_by_volume_key: %s\n",
		       __func__, strerror(errno));
		crypt_free(cd);
		return err;
	}

	free(key);
	printf("Device %s is now formatted for encrypted access.\n", device);
	syslog(LOG_INFO, "Device %s is now formatted for encrypted access.\n", device);
	crypt_free(cd);
	return 0;
}

int activate_crypt(unsigned char *device)
{
	struct crypt_device *cd;
	struct crypt_active_device cad;
	unsigned char device_name[NAMELEN] = {0};
	char *token, str[64] = {0}, *dup, *key;
	FILE *fp;
	int err;
	size_t len;
	ssize_t read;

	dup = strdup(device);
	token = strtok((char *)dup, "/");
	while (token != NULL) {
		sprintf(str, "%s", token);
		token = strtok(NULL, "/");
	}

	if (access(KEY_FILE, F_OK) == -1) {
		key = strdup(DEFAULT_DES_KEY);
		len = strlen(key);
	} else {
		fp = fopen(KEY_FILE, "r");
		if ((read = getline(&key, &len, fp)) == -1) {
			printf("%s: getline: %s\n", __func__, strerror(errno));
			return errno;
		}
		len = (read - 1);
		fclose(fp);
	}

	sprintf(device_name, "crypt-%s", str);
	if ((err = crypt_init(&cd, device)) < 0) {
		printf("%s: crypt_init: %s\n", __func__, strerror(errno));
		syslog(LOG_ERR, "%s: crypt_init: %s\n", __func__, strerror(errno));
		return err;
	}

	if (crypt_status(cd, device_name) == CRYPT_ACTIVE) {
		printf("Device %s is already active.\n", device_name);
		syslog(LOG_ERR, "Device %s is already active.\n", device_name);
		crypt_free(cd);
		return err;
	}

	if ((err = crypt_load(cd, CRYPT_LUKS1, NULL)) < 0) {
		printf("%s: crypt_load: %s\n", __func__, strerror(errno));
		syslog(LOG_ERR, "%s: crypt_load: %s\n", __func__, strerror(errno));
		crypt_free(cd);
		return err;
	}

	if ((err = crypt_activate_by_passphrase(cd, device_name, CRYPT_ANY_SLOT,
	    key, len, 0)) < 0) {
		if (strcmp(key, DEFAULT_DES_KEY) == 0) {
			err = crypt_activate_by_passphrase(cd, device_name,
							   CRYPT_ANY_SLOT,
							   LEGACY_DES_KEY,
							   strlen(LEGACY_DES_KEY), 0);
			if (err >= 0)
				goto activated;
		}
		printf("%s: crypt_activate_by_passphrase: %s\n", __func__,
		       strerror(errno));
		syslog(LOG_ERR, "%s: crypt_activate_by_passphrase: %s\n",
		       __func__, strerror(errno));
		crypt_free(cd);
		return err;
	}

activated:
	free(key);
	printf("Device %s is now activated as /dev/mapper/%s.\n",
	       device, device_name);
	syslog(LOG_INFO, "Device %s is now activated as /dev/mapper/%s.\n",
	       device, device_name);
	crypt_free(cd);
	return 0;
}

int deactivate_crypt(unsigned char *device_name)
{
	struct crypt_device *cd;
	int err;

	if ((err = crypt_init_by_name(&cd, device_name)) < 0) {
		printf("%s: crypt_init_by_name: %s\n", __func__,
		       strerror(errno));
		syslog(LOG_ERR, "%s: crypt_init_by_name: %s\n",
		       __func__, strerror(errno));
		return err;
	}

	if (crypt_status(cd, device_name) != CRYPT_ACTIVE) {
		printf("Device %s is not active.\n", device_name);
		syslog(LOG_ERR, "Device %s is not active.\n", device_name);
		crypt_free(cd);
		return err;
	}

	if ((err = crypt_deactivate(cd, device_name)) < 0) {
		printf("%s: crypt_deactivate: %s\n", __func__,
		       strerror(errno));
		syslog(LOG_ERR, "%s: crypt_deactivate: %s\n",
		        __func__, strerror(errno));
		crypt_free(cd);
		return err;
	}

	printf("Device %s is now deactivated.\n", device_name);
	syslog(LOG_INFO, "Device %s is now deactivated.\n", device_name);
	crypt_free(cd);
	return 0;
}
#endif
