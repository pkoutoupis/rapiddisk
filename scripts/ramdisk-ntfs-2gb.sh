#!/bin/bash
# For Debian: v1.01
# Warning: ECC Memory recommended.
# NTFS driver is called NTFS-3g, Not to be confused with 2GB size. 
# sometimes a necesary evil.
# To execute: save and `chmod +x ./ramdisk-ntfs-2gb.sh` then `./ramdisk-ntfs-2gb.sh`

# systemctl status rapiddiskd.service 
cd ~
sudo rapiddisk -a 2000
sudo mkfs.ntfs /dev/rd0
mkdir ramdisk
sudo mount -t ntfs /dev/rd0 ramdisk
sudo chown -R $(whoami):$(id -g -n) ramdisk
