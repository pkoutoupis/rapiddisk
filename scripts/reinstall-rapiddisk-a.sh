#!/bin/bash
# For Debian: v1.01
# Warning: ECC Memory recommended.
# Reinstall script Part 1/3 for Rapiddisk 8.2.0 Linux.
# Everytime there is a Kernet change, Rapiddisk needs to be reinstalled.
# To execute: save and `chmod +x ./reinstall-rapiddisk-a.sh` then `./reinstall-rapiddisk-a.sh`

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
