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
				sed -i 's,RAMDISKSIZE,'"$size"',' "$moddir/run_rapiddisk.sh"
				sed -i 's,BOOTDEVICE,'"$device"',' "$moddir/run_rapiddisk.sh"
				sed -i 's,CACHEMODE,'"$cache_mode"',' "$moddir/run_rapiddisk.sh"
				chmod +x "$moddir/run_rapiddisk.sh"
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
	rm -f "$moddir/run_rapiddisk.sh"
	return 0

}
