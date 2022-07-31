#!/bin/bash
# For Debian: v1.01
# Warning: ECC Memory recommended.
# To execute: save and `chmod +x ./ramdisk-detach.sh` then `./ramdisk-detach.sh`
# Unmap & Unmount first.

cd ~
# $ sudo rapiddisk -l
sudo rapiddisk -d rd0
