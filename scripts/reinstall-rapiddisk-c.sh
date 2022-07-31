#!/bin/bash
# For Debian: v1.01
# Reinstall Part3/3 for Rapiddisk 8.2.0 Linux.
# Everytime there is a Kernet change, Rapiddisk needs to be reinstalled.
# To execute: save and `chmod +x ./reinstall-linux-c.sh` then `./reinstall-rapiddisk-c.sh`

# modprobe rapiddisk
# modprobe rapiddisk-cache

# make tools-install
# make tools-uninstall

# make dkms-install
# make dkms-uninstall

systemctl enable rapiddiskd.service
# systemctl status rapiddiskd.service
