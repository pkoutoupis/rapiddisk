#!/bin/bash
# For Debian: v1.01
# HFSplus Not Journaled = Faster but Not safer.
# HFSplus does Not work with large drives >2TB.
# Linux Free HFSplus driver is incomplete, works in OSX, but Not with Bootcamp 5 drivers in Windows.
# Linux HFSplus cannot Name the Drive, but OSX can, and Linux detects ok.
# you need to know what is the HFSplus drive & partition you want to add the cache.
# EDIT sd?# BEFORE Running.
# To execute: save and `chmod +x ./ramdisk-cache-ntfs-2gb.sh` then `./ramdisk-cache-ntfs-2gb.sh`

cd ~
# systemctl status rapiddiskd.service
sudo rapiddisk -a 2000
sudo mkfs.hfsplus /dev/rd0
mkdir ramdisk
sudo rapiddisk -m rd0 -b /dev/sd?#
sudo mount -t hfsplus /dev/mapper/rc-wt_sd?# ramdisk
