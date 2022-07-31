#!/bin/bash
# For Debian: v1.01
# Warning: ECC Memory recommended.
# Btrfs = Ultra safe, but has too much Journaling = 1.7GB Free.
# Better than NTFS, allows special characters like: ?
# Btrfs is Unknown with very large drives: 18TB, 20TB.
# To execute: save and `chmod +x ./ramdisk-xfs-2gb.sh` then `./ramdisk-xfs-2gb.sh`

cd ~
sudo rapiddisk -a 2000
sudo mkfs.btrfs -f /dev/rd0
mkdir ramdisk
sudo mount -t btrfs /dev/rd0 ramdisk
sudo chown -R $(whoami):$(id -g -n) ramdisk

# chown is needed because sudo creates a sudo:sudo ramdisk 
# and $(whoami):$(id -g -n) does Not have Permissions to Write.
# sudo is required for rapiddisk.
