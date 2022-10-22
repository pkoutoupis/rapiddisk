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

j=tee
jj=/dev/null

if [ "$1" == "json" ] ; then
  json=" -j -g "
  if which jqa; then
    j=jq
    jj=
  elif which json_pp; then
    j=json_pp
    jj=
  else
    json=
  fi
fi

if which >/dev/null 2>&1 valgrind ; then
  cmd="valgrind --sim-hints=lax-ioctls --show-leak-kinds=all --leak-check=full --track-origins=yes -s ../src/rapiddisk_debug"
else
  cmd="../src/rapiddisk_debug"
fi

function cont() {
    echo -e "\npress a key..."
    read -r -n 1 -s q
}

function cleanup()
{
	../src/rapiddisk_debug -u rc-wa_loop7 >/dev/null
	../src/rapiddisk_debug -d ${RD}  >/dev/null
	[ -f /dev/loop7 ] && losetup -d /dev/loop7  >/dev/null
	rm -f /tmp/test1 >/dev/null
	rmmod >/dev/null 2>&1 rapiddisk.ko
  rmmod >/dev/null 2>&1 rapiddisk-cache.ko
}

function createLoopbackDevices()
{
	dd if=/dev/zero of=/tmp/test1 bs=1M count=256 >/dev/null 2>&1
	losetup /dev/loop7 /tmp/test1 >/dev/null 2>&1
}

function removeLoopbackDevices()
{
	losetup -d /dev/loop7 >/dev/null 2>&1
	rm -f /tmp/test1 >/dev/null 2>&1
}

function createCacheVolumes()
{
	O=$($cmd -g -a 64 $json)
	RETVAL=$?
	if [ -n "$json" ] ; then
	  RD=$(echo "$O"|cut -d' ' -f6)
	else
	  RD=$(echo "$O"|cut -d' ' -f3)
  fi
	echo -e >/dev/stderr "\n$O"
	if [ ${RETVAL} -ne 0 ]; then
		cleanup
		exit ${RETVAL}
	fi
	echo "$RD"
}

function mapCacheVolumes() {
  RD="$1"
  o=$($cmd -m "${RD}" -b /dev/loop7 $json)
  echo "$o" | $j $jj
  RETVAL=$?
  	if [ ${RETVAL} -ne 0 ]; then
  		cleanup
  		exit ${RETVAL}
  	fi
}

function unampCacheVolumes()
{
  o=$($cmd -u rc-wt_loop7 $json)
  echo "$o" | $j $jj
  RETVAL=$?
  	if [ ${RETVAL} -ne 0 ]; then
  		cleanup
  		exit ${RETVAL}
  	fi
}


function removeCacheVolumes()
{
	o=$($cmd -d "${RD}" $json)
  echo "$o" | $j $jj
	RETVAL=$?
	if [ ${RETVAL} -ne 0 ]; then
		cleanup
		exit ${RETVAL}
	fi
}

function statCacheVolumes()
{
	o=$($cmd -s rc-wt_loop7 $json)
  echo "$o" | $j $jj
	RETVAL=$?
	if [ ${RETVAL} -ne 0 ]; then
		cleanup
		exit ${RETVAL}
	fi
}

function listVolumes() {
  o=$($cmd -l  $json)
  echo "$o" | $j $jj
  RETVAL=$?
#  if [ ${RETVAL} -ne 0 ]; then
#    cleanup
#    exit ${RETVAL}
#  fi
}

#
#
#


COUNT=$(lsmod|grep -c rapiddisk)
if [ "${COUNT}" -ne 2 ]; then
	insmod ../module/rapiddisk.ko max_sectors=2048 nr_requests=1024 >/dev/null 2>&1
	insmod ../module/rapiddisk-cache.ko >/dev/null 2>&1
fi

echo "Create Loopback Devices..."
createLoopbackDevices

echo -e "\nCreate Ramdisk..."
ramdisk=$(createCacheVolumes)
cont

echo -e "\nMap Ramdisk..."
mapCacheVolumes "$ramdisk"
cont

echo -e "\nRapiddisk -l..."
listVolumes
cont

echo -e "\nListing /dev/mapper..."
ls -l /dev/mapper/*rc-*
cont

echo -e "\nStat Cache Volumes..."
statCacheVolumes
cont

echo -e "\nUnmapCacheVolumes..."
unampCacheVolumes
cont

echo -e "\nRemove Ramdisk..."
removeCacheVolumes
cont

echo -e "\nRapiddisk -l..."
listVolumes
cont

echo -e "\nListing /dev/mapper..."
ls -l /dev/mapper/*rc-*
cont

echo -e "\nRemove Loopback Devices..."
removeLoopbackDevices

cleanup
exit 0
