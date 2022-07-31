#!/bin/bash
# For Debian: v1.01
# Uninstall script for Rapiddisk 8.2.0 Linux.
# To execute: save and `chmod +x ./uninstall-linux.sh` then `./uninstall-rapiddisk.sh`

sudo systemctl stop rapiddisk.service
sudo systemctl disable rapiddisk.service
sudo modprobe -r rapiddisk
sudo modprobe -r rapiddisk-cache
whereis rapiddisk
# rapiddisk: /usr/sbin/rapiddisk /etc/rapiddisk /usr/share/man/man1/rapiddisk.1
sudo rm /usr/sbin/rapiddisk
sudo rm /etc/rapiddisk
sudo rm /usr/share/man/man1/rapiddisk.1
sudo systemctl daemon-reload
sudo systemctl reset-failed
sudo make uninstall

sudo reboot 10
