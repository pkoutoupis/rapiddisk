#!/bin/bash
  
if [ ! "$BASH_VERSION" ] ; then
        exec /bin/bash "$0" "$@"
fi

[ $# -ne "1" ] && echo "Error. Please input a RapidDisk or RapidDisk-Cache device."

fio --bs=4k --ioengine=libaio --iodepth=32 --size=1g --direct=1 --runtime=60 --filename=$1 --rw=randread --name=fio-rapiddisk-randread-test --numjobs=8 --group_reporting

exit $?
