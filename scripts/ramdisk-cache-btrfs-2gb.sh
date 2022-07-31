#!/bin/bash
# For Debian: v1.01
# Btrfs = Ultra safe, but has too much Journaling = 1.7GB Free.
# Better than NTFS, allows special characters like: ?
# Btrfs is Unknown with very large drives: 18TB, 20TB.
# you need to know what is the Btrfs drive & partition you want to add the cache.
# EDIT sd?# BEFORE Running.
# To execute: save and `chmod +x ./ramdisk-cache-ntfs-2gb.sh` then `./ramdisk-cache-ntfs-2gb.sh`

cd ~
# systemctl status rapiddiskd.service
sudo rapiddisk -a 2000
sudo mkfs.btrfs -f /dev/rd0
mkdir ramdisk
sudo rapiddisk -m rd0 -b /dev/sd?#
sudo mount -t btrfs /dev/mapper/rc-wt_sd?# ramdisk
