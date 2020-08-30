#!/bin/bash

if [ ! "$BASH_VERSION" ] ; then
	exec /bin/bash "$0" "$@"
fi

PATH=$PATH:$(pwd)

function cleanup()
{       
	cache_test -u
	losetup -d /dev/loop5
	rm -f /tmp/test1
}

function createLoopbackDevices()
{       
	dd if=/dev/zero of=/tmp/test1 bs=1M count=256
	losetup /dev/loop5 /tmp/test1
}

function removeLoopbackDevices()
{       
	losetup -d /dev/loop5
	rm -f /tmp/test1
}

function createCacheVolumes()
{
	cache_test -a /dev/loop5
	RETVAL=$?
	if [ $RETVAL -ne 0 ]; then
		cleanup
		exit $RETVAL
	fi
}

function removeCacheVolumes()
{
	cache_test -u
	RETVAL=$?
	if [ $RETVAL -ne 0 ]; then
		cleanup
		exit $RETVAL
	fi
}

function statCacheVolumes()
{       
	cache_test -s rc-wa_loop5
	RETVAL=$?
	if [ $RETVAL -ne 0 ]; then
		cleanup
		exit $RETVAL
	fi
}

#
#
#

modprobe rapiddisk max_sectors=2048 nr_requests=1024 2>&1 >/dev/null
modprobe rapiddisk-cache 2>&1 >/dev/null

echo "Create Loopback Devices..."
createLoopbackDevices
echo "Create Cache Volumes..."
createCacheVolumes
ls -l /dev/mapper|grep "rc-wa_"
echo "Stat Cache Volumes..."
statCacheVolumes
echo "Remove Cache Volumes..."
removeCacheVolumes
ls -l /dev/mapper|grep "rc-wa_"
echo "Remove Loopback Devices..."
removeLoopbackDevices

exit 0
