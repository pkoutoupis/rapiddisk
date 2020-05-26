#!/bin/sh

function ihelp () {

	# echo "$(basename $0) - installs hook and boot script to enable rapiddisk cache on boot volume."
	# echo
	echo "Usage:"
	echo "$(basename $0) --boot=<boot_partition> --size=<ramdisk_size> --kernel=<kernel_version> [--force]"
	echo "$(basename $0) --kernel=<kernel_version> --uninstall"
	echo ""
	echo "Where:"
	echo ""
	echo "boot_partition: the device mounted as '/' as in /etc/fstab"
	echo "ramdisk_size: the size in MB of the ramdisk to be used as cache"
	echo "kernel_version: the initrd file version you want to alter"
	echo "--force: forces replacing already installed scripts"
	echo ""
	echo "--uninstall: removes scripts and revert systems changes. Needs the --kernel option."
	echo ""
	echo "Note: kernel_version is really important, if you end up with a system that"
	echo "cannot boot, you can choose another kernel version from grub menu"
	echo "and use the --uninstall command with the kernel_version not booting anymore"
	echo "You can usually use 'uname -r' to obtain the current kernel version."
	echo ""
}

# Credits: https://stackoverflow.com/questions/806906/how-do-i-test-if-a-variable-is-a-number-in-bash
function is_num () {
       [ "$1" ] && [ -z "${1//[0-9]}" ]
}


function myerror () {
	echo ""
	echo "$1"
	echo ""
	ihelp
	exit 1
}

function finalstuff () {
	echo " - Updating initramfs for kernel version ${kernel_version}..."
	echo ""
	update-initramfs -u -k "$kernel_version"
	echo ""
	echo " - Updating grub..."
	echo ""
	update-grub
	echo ""
	echo "Done."
}

for i in "$@"
do
case $i in
	--kernel=*)
		kernel_version="${i#*=}"
		shift # past argument=value
	;;
	--uninstall)
		uninstall=1
		shift # past argument with no value
	;;
	--boot=*)
		boot_device="${i#*=}"
		shift # past argument=value
	;;
	--size=*)
		ramdisk_size="${i#*=}"
		shift # past argument=value
	;;
	--force)
		force=1
		shift # past argument with no value
	;;
	*)
	        myerror "Error: Unknown argument."
		# unknown option
	;;
esac
done

if ! whoami | grep root 2>/dev/null 1>/dev/null ; then
	myerror "Sorry, this must be run as root."
fi

cwd="$(pwd)"

if [ -d /etc/initramfs-tools/hooks ] && [ -d /etc/initramfs-tools/scripts/init-premount ] ; then
	hook_dest=/etc/initramfs-tools/hooks/rapiddisk_hook
	bootscript_dest=/etc/initramfs-tools/scripts/init-premount/rapiddisk
elif [ -d /usr/share/initramfs-tools/hooks ] && [ -d /usr/share/initramfs-tools/scripts/init-premount ] ; then
	hook_dest=/usr/share/initramfs-tools/hooks/rapiddisk_hook
	bootscript_dest=/usr/share/initramfs-tools/scripts/init-premount/rapiddisk
else
	myerror "Error: I can't find any suitable place for initramfs scripts."
fi


if [ ! -z $uninstall ] ; then
	echo " - Starting uninstall process..."
	if [ -z $kernel_version ] ; then
	         myerror "Error: missing argument '--kernel'."
	fi
	if [ -x "$hook_dest" ] || [ -x "$bootscript_dest" ] ; then
		echo " - Uninstalling scripts..."
		rm -f "$hook_dest/rapiddisk_hook" 2>/dev/null
		rm -f "$bootscript_dest/rapiddisk" 2>/dev/null
	fi
	#if lsinitramfs /boot/initrd.img-"$kernel_version" | grep rapiddisk >/dev/null 2>/dev/null ; then
	#	echo ""
	#	echo "Error: /boot/initrd.img-$kernel_version still contains rapiddisk stuff."
	#	echo
	#	exit 1
	#fi
	finalstuff
	exit 0	

fi

if [ -z "$boot_device" ] || [ -z "$ramdisk_size" ] || [ -z $kernel_version ]  ; then
	myerror "Error: missing arguments."
fi

# TODO this check must be improved
if ! echo "$boot_device" | grep -E '^/dev/[[:alpha:]]{1,4}[[:digit:]]{0,99}$' >/dev/null 2>/dev/null ; then
        myerror "Error: boot_device must be in the form '/dev/xxx' or '/dev/xxxn with n as a positive integer. Exiting..."
fi

if ! is_num "$ramdisk_size" ; then
        myerror "Error: The ramdisk size must be a positive integer. Exiting..."
fi

if [ ! -x "${cwd}/hooks/rapiddisk_hook" ] || [ ! -x "${cwd}/scripts/init-premount/rapiddisk" ] ; then
	myerror "Error: I can't find the scriptsi to be installed. Exiting..."
fi

##############################################################################
# TODO The following piece of code was there to check if the user specified
# the same device present in /etc/fstab - but if in fstab UUIDs are used, or
# the LABEL= syntax, it looses meaning.
##############################################################################

##current_boot_device="$(grep -vE '^#' /etc/fstab|grep -m 1 -Eo '.*\s+/\s+'|awk 'BEGIN { OFS="\\s" } { print $1 }')"
##if ! echo "$current_boot_device" | grep -E '^'"$boot_device"'$' 2>/dev/null >/dev/null ; then
##	echo ""
##	echo "ERROR: The specified boot device is different from the one in /etc/fstab ('${current_boot_device}'). Exiting..."
##	echo ""
##	ihelp
##	exit 1
##fi

##############################################################################

if [ -x "$hook_dest" ] || [ -x "$bootscript_dest" ] && [ -z $force ] ; then
	myerror "Error: Initrd hook or boot scripts already installed. You may use the '--force' option. Exiting..."
else
	echo " - Copying ${cwd}/hooks/rapiddisk_hook to ${hook_dest}..."
	if ! cp "${cwd}/hooks/rapiddisk_hook" "${hook_dest}" ; then
		myerror "Error: Could not copy rapiddisk_hook to ${hook_dest}. Exiting..."
	fi
	chmod +x "${hook_dest}"	
	echo " - Copying ${cwd}/scripts/init-premount/rapiddisk to ${bootscript_dest}..."

	# TODO there are many better ways, I suppose...
	rapiddisk_boot_script="$(cat ${cwd}/scripts/init-premount/rapiddisk | sed -r 's/RAMDISKSIZE/'${ramdisk_size}'/' | \
		sed -r 's,BOOTDEVICE,'${boot_device}',')"

	if ! echo "$rapiddisk_boot_script" | tee "${bootscript_dest}" >/dev/null ; then
		myerror "Error: Could not copy rapiddisk to ${bootscript_dest}. Exiting..."
	fi
	chmod +x "${bootscript_dest}"

fi

finalstuff

exit 0

