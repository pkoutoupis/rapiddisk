#!/bin/bash
# For Debian: v1.01
# Reinstall Part2/3 for Rapiddisk 8.2.0 Linux.
# Everytime there is a Kernet change, Rapiddisk needs to be reinstalled.
# To execute: save and `chmod +x ./reinstall-rapiddisk-b.sh` then `./reinstall-rapiddisk-b.sh`

make
sudo make install
sudo reboot 10
