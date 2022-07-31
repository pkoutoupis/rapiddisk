#!/bin/bash
# For Debian: v1.01
# Ext4 v2.0 is Not recomended for very large drives >2TB.
# you need to know what is the Ext4 drive & partition you want to add the cache.
# EDIT sd?# BEFORE Running.
# To execute: save and `chmod +x ./ramdisk-cache-ntfs-2gb.sh` then `./ramdisk-cache-ntfs-2gb.sh`

cd ~
# systemctl status rapiddiskd.service
sudo rapiddisk -a 2000
sudo mkfs -t ext4 -F /dev/rd0
mkdir ramdisk
sudo rapiddisk -m rd0 -b /dev/sd?#
sudo mount -t ext4 /dev/mapper/rc-wt_sd?# ramdisk
