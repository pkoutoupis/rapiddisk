#!/bin/bash

# called by dracut
check() {

	for i in $moddir/*
		do
			if [ "${moddir}/${kernel}" = "$i" ] ; then
				size="$(head -n 1 "$i")"
				device="$(head -n 2 "$i" | tail -n 1)"
				cache_mode="$(tail -n 1 "$i")"
				cp -f "$moddir/run_rapiddisk.sh.orig" "$moddir/run_rapiddisk.sh"
				sed -i 's,RAMDISKSIZE,'"$size"',g' "$moddir/run_rapiddisk.sh"
				sed -i 's,BOOTDEVICE,'"$device"',g' "$moddir/run_rapiddisk.sh"
				sed -i 's,CACHEMODE,'"$cache_mode"',g' "$moddir/run_rapiddisk.sh"
				chmod +x "$moddir/run_rapiddisk.sh"
				return 0
			fi
		done
	return 255

}

depends() {

	echo dm
	return 0

}

installkernel() {

	hostonly='' instmods rapiddisk rapiddisk-cache dm-writecache dm-mod
	return 0

}

install() {

	inst /sbin/rapiddisk
	inst_hook pre-mount 50 "$moddir/run_rapiddisk.sh"
	inst "$moddir/run_rapiddisk.sh" "/sbin/run_rapiddisk.sh"
	rm -f "$moddir/run_rapiddisk.sh"
	return 0

}
