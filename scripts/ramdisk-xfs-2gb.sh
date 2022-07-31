#!/bin/bash
# For Debian: v1.01
# XFS is the Only FileSystem that works ok with VeryLargeDrives: 18TB, 20TB.
# forget Ext4 over >2TB, makes weird noises, can damage mechanical drives.
# Linux Free HFSplus, better than NTFS, but does Not work over >2TB.
# forget Linux Free HFSplus Journaled. 
# Paragon HFSplus & APFS paid drivers for Linux is Unknown.
# forget NTFS-3g, files & folders cannot have special characters like: ?
# forget exFAT, FAT32, FAT16, etc... worse than NTFS.
# Btrfs = Ultra safe, but has too much Journaling = 1.7GB Free of 2GB, allows special characters like: ?
# Btrfs is Unknown with very large drives: 18TB, 20TB.
# Cifs = smbfs: Unknown.
# To execute: save and `chmod +x ./ramdisk-xfs-2gb.sh` then `./ramdisk-xfs-2gb.sh`

cd ~
sudo rapiddisk -a 2000
sudo mkfs.xfs -f /dev/rd0
mkdir ramdisk
sudo mount -t xfs /dev/rd0 ramdisk
sudo chown -R $(whoami):$(id -g -n) ramdisk

# chown is needed because sudo creates a sudo:sudo ramdisk 
# and $(whoami):$(id -g -n) does Not have Permissions to Write.
# sudo is required for rapiddisk.
