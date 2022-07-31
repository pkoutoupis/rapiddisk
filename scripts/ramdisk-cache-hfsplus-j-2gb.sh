#!/bin/bash
# For Debian: v1.01
# HFSplus Journaled.
# HFSplus Journaled is Read Only = useless for Ramdrive.
# consider Paragon HFSplus & APFS paid drivers for Linux is Unknown.
# Linux Free HFSplus does Not work with large drives >2TB.
# Linux Free HFSplus driver is incomplete, works in OSX, but Not with Bootcamp 5 drives in Windows.
# Linux HFSplus cannot Name the Drive, but OSX can, and Linux detects ok.
# ...
# journaling file system keeps track of changes not yet written to the drive, 
# pre-records the cue list of changes in a circular log known as Journal. 
# In the event of a crash or power failure, Drive is less likely to become corrupted.
# ...
# you need to know what is the HFSplus drive & partition you want to add the cache.
# EDIT sd?# BEFORE Running.
# To execute: save and `chmod +x ./ramdisk-cache-ntfs-2gb.sh` then `./ramdisk-cache-ntfs-2gb.sh`

cd ~
# systemctl status rapiddiskd.service
sudo rapiddisk -a 2000
sudo mkfs.hfsplus -J /dev/rd0
mkdir ramdisk
sudo rapiddisk -m rd0 -b /dev/sd?#
sudo mount -t hfsplus /dev/mapper/rc-wt_sd?# ramdisk
