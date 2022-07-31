#!/bin/bash
# For Debian: v1.01
# Warning: ECC Memory recommended.
# HFSplus Not Journaled = Faster but Not safer.
# HFSplus Journaled is Read Only = useless for Ramdrive.
# Journal is useful for drives that store information after power loss = useless for Ramdrive.
# Linux Free HFSplus does Not work over >2TB. 
# Better than NTFS, allows special characters like: ?
# Paragon HFSplus & APFS paid drivers for Linux is Unknown.
# To execute: save and `chmod +x ./ramdisk-xfs-2gb.sh` then `./ramdisk-xfs-2gb.sh`

cd ~
sudo rapiddisk -a 2000
sudo mkfs.hfsplus -J /dev/rd0
mkdir ramdisk
sudo mount -t hfsplus /dev/rd0 ramdisk
sudo chown -R $(whoami):$(id -g -n) ramdisk

# chown is needed because sudo creates a sudo:sudo ramdisk 
# and $(whoami):$(id -g -n) does Not have Permissions to Write.
# sudo is required for rapiddisk.
