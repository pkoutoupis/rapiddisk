#!/bin/bash
# For Debian: v1.01
# Warning: ECC Memory recommended.
# To execute: save and `chmod +x ./ramdisk-unmap.sh` then `./ramdisk-unamp.sh`
# you need to know what is the Mapped cache drive.
# EDIT sd?# BEFORE Running.

cd ~
# $ sudo rapiddisk -l
# $ sudo rapiddisk -s
sudo rapiddisk -u rc-wt_sd?#
# $ sudo rapiddisk -d rd0
