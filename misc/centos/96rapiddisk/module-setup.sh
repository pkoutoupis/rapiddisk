#!/bin/bash

# called by dracut
check() {
	for i in `ls "$moddir"` ; do
		if [ "$kernel" = "$i" ] ; then
			return 0
		fi
	done
	return 255
}

depends() {
	echo dm
}

installkernel() {

	hostonly='' instmods rapiddisk rapiddisk-cache

	return 0
}

install() {
	inst /sbin/rapiddisk
	inst_hook pre-mount 00 "$moddir/run_rapiddisk.sh"
	inst "$moddir/run_rapiddisk.sh" "/sbin/run_rapiddisk.sh"
	return 0
}
