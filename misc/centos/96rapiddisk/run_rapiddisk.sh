#!/bin/bash

modprobe rapiddisk
modprobe rapiddisk-cache
rapiddisk --attach RAMDISKSIZE
rapiddisk --cache-map rd0 BOOTDEVICE wa

