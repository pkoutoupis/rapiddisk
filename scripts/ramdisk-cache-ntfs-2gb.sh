#!/bin/bash
# For Debian: v1.01
# sometimes a necesary evil.
# you need to know what is the NTFS drive & partition you want to add the ram cache.
# EDIT sd?# BEFORE Running.
# To execute: save and `chmod +x ./ramdisk-cache-ntfs-2gb.sh` then `./ramdisk-cache-ntfs-2gb.sh`

cd ~
# systemctl status rapiddiskd.service
sudo rapiddisk -a 2000
sudo mkfs.ntfs -F /dev/rd0
mkdir ramdisk
sudo rapiddisk -m rd0 -b /dev/sd?#
sudo mount -t ntfs /dev/mapper/rc-wt_sd?# ramdisk
