#!/bin/bash

# called by dracut
check() {

	for i in $moddir/*
		do
			if [ "${moddir}/${kernel}" = "$i" ] ; then
				size="$(cat "$i" | head -n 1)"
				device="$(cat "$i" | tail -n 1)"
				cp "$moddir/run_rapiddisk.sh.orig" "$moddir/run_rapiddisk.sh"
				sed -i 's,RAMDISKSIZE,'$size',' "$moddir/run_rapiddisk.sh"
				sed -i 's,BOOTDEVICE,'$device',' "$moddir/run_rapiddisk.sh" 
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
