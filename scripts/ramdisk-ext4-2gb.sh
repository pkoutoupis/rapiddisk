#!/bin/bash
# For Debian: v1.01
# Warning: ECC Memory recommended.
# Ext4 was invented in 1996 
# since K/Ubuntu 17 there is Ext4 v2 called v1 that should be called Ext5
# Ext4 v2 has 64-Bit Headers, Magic Number, etc...
# Problem: Not backward compatible with older: Ext2fsd v0.69.exe, Osx MacFuse, etc...
# R&W will damage the Ext4 v2.0 hhd or ssd, 
# damaged hdd/sdd can be Read in Windows with Ext2fsd v0.69 but Not in Linux >20.04 LTS.
# there is a Fork v0.70.exe but needs work.
# to consider: Paragon Linux drivers for Windows.
# To execute: save and `chmod +x ./ramdisk-ext4-2gb.sh` then `./ramdisk-ext4-2gb.sh`

cd ~
# systemctl start rapiddiskd.service
sudo rapiddisk -a 2000
sudo mkfs -t ext4 -F /dev/rd0
mkdir ramdisk
sudo mount -t ext4 /dev/rd0 ramdisk
sudo chown -R $(whoami):$(id -g -n) ramdisk
