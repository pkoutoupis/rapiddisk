/*********************************************************************************
 ** Copyright (c) 2015 Petros Koutoupis
 ** All rights reserved.
 **
 ** @project: rapiddisk
 **
 ** @filename: crypt.c
 ** @description: File for rapiddisk.
 **
 ** @date: 21Nov14, petros@petroskoutoupis.com
 ********************************************************************************/

#include "common.h"
#include <libcryptsetup.h>


int format_crypt(unsigned char *device)
{
	struct crypt_device *cd;
	struct crypt_params_luks1 params;
	int err;

	if ((err = crypt_init(&cd, device)) < 0) {
		printf("%s: crypt_init: %s\n", __func__, strerror(errno));
		return err;
	}
	printf("Device %s will be formatted to LUKS device after 5 seconds.\n"
			"Press CTRL+C now if you want to cancel this operation.\n", device);
	sleep(5);

	params.hash = "sha1";
	params.data_alignment = 0;

	if ((err = crypt_format(cd, CRYPT_LUKS1, "aes", "xts-plain64", NULL, NULL, 256 / 8, &params)) < 0) {
		printf("%s: crypt_format: %s\n", __func__, strerror(errno));
		syslog(LOG_ERR, "%s: crypt_format: %s\n", __func__, strerror(errno));
		crypt_free(cd);
		return err;
	}

	if ((err = crypt_keyslot_add_by_volume_key(cd, CRYPT_ANY_SLOT, NULL, 0, DES_KEY, 8)) < 0) {
		printf("%s: crypt_keyslot_add_by_volume_key: %s\n", __func__, strerror(errno));
		syslog(LOG_ERR, "%s: crypt_keyslot_add_by_volume_key: %s\n", __func__, strerror(errno));
		crypt_free(cd);
		return err;
	}

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
	char *token, str[64] = {0}, *dup;
	int err;

	dup = strdup(device);
	token = strtok((char *)dup, "/");
	while (token != NULL) {
		sprintf(str, "%s", token);
		token = strtok(NULL, "/");
	}

	sprintf(device_name, "rxcrypt-%s", str);
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

	if ((err = crypt_activate_by_passphrase(cd, device_name, CRYPT_ANY_SLOT, DES_KEY, 8, 0)) < 0) {
		printf("%s: crypt_activate_by_passphrase: %s\n", __func__, strerror(errno));
		syslog(LOG_ERR, "%s: crypt_activate_by_passphrase: %s\n", __func__, strerror(errno));
		crypt_free(cd);
		return err;
	}

	printf("Device %s is now activated as /dev/mapper/%s.\n", device, device_name);
	syslog(LOG_INFO, "Device %s is now activated as /dev/mapper/%s.\n", device, device_name);
	crypt_free(cd);
	return 0;
}

int deactivate_crypt(unsigned char *device_name)
{
	struct crypt_device *cd;
	int err;

	if ((err = crypt_init_by_name(&cd, device_name)) < 0) {
		printf("%s: crypt_init_by_name: %s\n", __func__, strerror(errno));
		syslog(LOG_ERR, "%s: crypt_init_by_name: %s\n", __func__, strerror(errno));
		return err;
	}

	if (crypt_status(cd, device_name) != CRYPT_ACTIVE) {
		printf("Device %s is not active.\n", device_name);
		syslog(LOG_ERR, "Device %s is not active.\n", device_name);
		crypt_free(cd);
		return err;
	}

	if ((err = crypt_deactivate(cd, device_name)) < 0) {
		printf("%s: crypt_deactivate: %s\n", __func__, strerror(errno));
		syslog(LOG_ERR, "%s: crypt_deactivate: %s\n", __func__, strerror(errno));
		crypt_free(cd);
		return err;
	}

	printf("Device %s is now deactivated.\n", device_name);
	syslog(LOG_INFO, "Device %s is now deactivated.\n", device_name);
	crypt_free(cd);
	return 0;
}
