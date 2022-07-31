#!/bin/bash
# For Debian: v1.01
# To execute: save and `chmod +x ./ramdisk-cache-xfs-2gb.sh` then `./ramdisk-cache-xfs-2gb.sh`
# you need to know what is the XFS drive & partition you want to add the cache.
# EDIT sd?# BEFORE Running.

cd ~
# systemctl status rapiddiskd.service
sudo rapiddisk -a 2000
sudo mkfs.xfs -f /dev/rd0
mkdir ramdisk
sudo rapiddisk -m rd0 -b /dev/sd?#
sudo mount -t xfs /dev/mapper/rc-wt_sd?# ramdisk
