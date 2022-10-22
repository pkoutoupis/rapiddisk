#!/bin/bash

who="$(whoami)"
if [ "$who" != "root" ] ; then
  echo "Please run as root!"
  exit 0
fi

if [ ! "$BASH_VERSION" ] ; then
	exec /bin/bash "$0" "$@"
fi

PATH=$PATH:$(pwd)
RD=rd0

function cleanup()
{       
	../src/rapiddisk -u rc-wa_loop7
	../src/rapiddisk -d ${RD}
	losetup -d /dev/loop7
	rm -f /tmp/test1
}

function createLoopbackDevices()
{       
	dd if=/dev/zero of=/tmp/test1 bs=1M count=256
	losetup /dev/loop7 /tmp/test1
}

function removeLoopbackDevices()
{       
	losetup -d /dev/loop7
	rm -f /tmp/test1
}

function createCacheVolumes()
{
	RD=`../src/rapiddisk -a 64 -g|cut -d' ' -f3`
	../src/rapiddisk -m ${RD} -b /dev/loop7
	RETVAL=$?
	if [ ${RETVAL} -ne 0 ]; then
		cleanup
		exit ${RETVAL}
	fi
}

function removeCacheVolumes()
{
	../src/rapiddisk -u rc-wt_loop7
	../src/rapiddisk -d ${RD}
	RETVAL=$?
	if [ ${RETVAL} -ne 0 ]; then
		cleanup
		exit ${RETVAL}
	fi
}

function statCacheVolumes()
{       
	../src/rapiddisk -s rc-wt_loop7
	RETVAL=$?
	if [ ${RETVAL} -ne 0 ]; then
		cleanup
		exit ${RETVAL}
	fi
}

#
#
#


COUNT=`lsmod|grep rapiddisk|wc -l`
if [ ${COUNT} -ne 2 ]; then
	insmod ../module/rapiddisk.ko max_sectors=2048 nr_requests=1024 2>&1 >/dev/null
	insmod ../module/rapiddisk-cache.ko 2>&1 >/dev/null
fi

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

rmmod rapiddisk.ko
rmmod rapiddisk-cache.ko

exit 0
