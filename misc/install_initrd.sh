#!/bin/bash

ihelp()  {

	# echo "$(basename $0) - installs hook and boot script to enable rapiddisk cache on root volume."
	echo "Usage:"
	echo "$(basename "$0") --root=<root_partition> --size=<ramdisk_size> --kernel=<kernel_version> [--force]"
	echo "$(basename "$0") --kernel=<kernel_version> --uninstall [--force]"
	echo ""
	echo "Where:"
	echo ""
	echo "<root_partition> is the device mounted as '/' as in /etc/fstab in the /dev/xxxn format"
	echo "<ramdisk_size> is the size in MB of the ramdisk to be used as cache"
	echo "<kernel_version> is needed to determine which initrd file to alter"
	echo "--force forces replacing already installed scripts"
	echo ""
	echo "--uninstall removes scripts and revert systems changes. Needs the '--kernel' option"
	echo "			and make the script ignore all the other options."
	echo "--force issue the rm command even if the install dir seems not to exists"
	echo ""
	echo "Note: kernel_version is really important: if you end up with a system that"
	echo "cannot boot, you can choose another kernel version from the grub menu,"
	echo "boot successfully, and use the --uninstall command with the kernel_version of"
	echo "the non-booting kernel to restore it."
	echo ""
	echo "You can usually use 'uname -r' to obtain the current kernel version."
	echo ""
	echo "Warning: CentOS support is experimental!"
	echo ""

}

is_num()  {

	[ "$1" ] && [ -z "${1//[0-9]/}" ]

} # Credits: https://stackoverflow.com/questions/806906/how-do-i-test-if-a-variable-is-a-number-in-bash

myerror()  {

	ihelp
	echo "**** $1"
	exit 1

}

for i in "$@"; do
	case $i in
		--kernel=*)
			kernel_version="${i#*=}"
			shift # past argument=value
			;;
		--uninstall)
			uninstall=1
			shift # past argument with no value
			;;
		--root=*)
			root_device="${i#*=}"
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
			myerror "Error: unknown argument. Exiting..."
			# unknown option
			;;
	esac
done # Credits https://stackoverflow.com/questions/192249/how-do-i-parse-command-line-arguments-in-bash

# check for user == root
whoami | grep root 2> /dev/null 1> /dev/null || myerror "Error: sorry, this must be run as root. Exiting..."

cwd="$(dirname $0)"

# This option is always mandatory (ubuntu/centos installing/uninstalling) so it is checked before all the others
[ -n "$kernel_version" ] || myerror "Error: missing argument '--kernel'. Exiting..."

# if we are NOT uninstalling we need some more checks
if [ -z "$uninstall" ]; then
	if [ -z "$root_device" ]; then

		echo " - No root device was specified, we start looking for it in /etc/fstab..."
		
		root_line="$(grep -vE '^[ #]+' /etc/fstab | grep -m 1 -oP '^.*?[^\s]+\s+/\s+')"
		root_first_char="$(echo $root_line | grep -o '^.')"

		case $root_first_char in
			U)
				uuid="$(echo $root_line | grep -oP '[\w\d]{8}-([\w\d]{4}-){3}[\w\d]{12}')"	
				root_device=/dev/"$(ls 2>/dev/null -l /dev/disk/by-uuid/ | grep $uuid | grep -oE '[^/]+$')"
				;;
			L)
				label="$(echo $root_line | grep -oP '=[^\s]+' | tr -d '=')"
				root_device=/dev/"$(ls 2>/dev/null -l /dev/disk/by-label/ | grep $label | grep -oE '[^/]+$')"
				;;
			/)
				device="$(echo $root_line | grep -oP '^[^\s]+')"
				root_device=/dev/"$(ls 2>/dev/null -l $device | grep -oP '[^/]+$')"
				;;
			*)
				myerror "Error: could not find the root device from /etc/fstab. Use the '--root' option. Exiting..."
				;;
		esac

		# TODO this check must be improved
		if ! echo "$root_device" | grep -P '^/dev/\w{1,4}\d{0,99}$' >/dev/null 2>/dev/null ; then
			myerror "Error: root_device '$root_device' must be in the form '/dev/xxx' or '/dev/xxxn with n as a positive integer. Exiting..."
		fi

		echo " - Root device '$root_device' was found!"
		echo ' - Is it ok to use it? [yN]'
		read yn

		{ [ "$yn" = "y" ] || [ "$yn" = "Y" ] ; } || myerror "Error: the root device we found was not ok. Use the '--root' option. Exiting..."
	fi

	[ -n "$ramdisk_size" ] ||  myerror "Error: missing argument '--size'. Exiting..."
	is_num "$ramdisk_size" || myerror "Error: The ramdisk size must be a positive integer. Exiting..."
	

fi

# Looks for the OS
if hostnamectl | grep "CentOS Linux" > /dev/null 2> /dev/null; then

	# CentOS
	echo " - CentOS detected!"

	if [ -z "$uninstall" ]; then
		echo " - Installing by copying 96rapiddisk into /usr/lib/dracut/modules.d/..."
		if [ -d /usr/lib/dracut/modules.d/96rapiddisk ] && [ -z "$force" ] ; then
			myerror "Error: /usr/lib/dracut/modules.d/96rapiddisk already exists, use '--force'. Exiting..."
		fi
		cp -rf "${cwd}"/centos/96rapiddisk /usr/lib/dracut/modules.d/
		echo " - Editing /usr/lib/dracut/modules.d/96rapiddisk/run_rapiddisk.sh..."
		sed -i 's/RAMDISKSIZE/'"${ramdisk_size}"'/' /usr/lib/dracut/modules.d/96rapiddisk/run_rapiddisk.sh
		sed -i 's,BOOTDEVICE,'"${root_device}"',' /usr/lib/dracut/modules.d/96rapiddisk/run_rapiddisk.sh
		echo " - chmod +x /usr/lib/dracut/modules.d/96rapiddisk/* ..."
		chmod +x /usr/lib/dracut/modules.d/96rapiddisk/*
	else
		echo " - Uninstalling by removing dir /usr/lib/dracut/modules.d/96rapiddisk..."
		if [ ! -d /usr/lib/dracut/modules.d/96rapiddisk ] && [ -z "$force" ] ; then
			myerror "Error: /usr/lib/dracut/modules.d/96rapiddisk does not exist, use '--force'. Exiting..."
		fi
		rm -rf /usr/lib/dracut/modules.d/96rapiddisk
	fi

	echo " - Running 'dracut --kver $kernel_version -f'"
	#dracut --add-drivers "rapiddisk rapiddisk-cache" --kver "$kernel_version" -f
	dracut --kver "$kernel_version" -f

	echo " - Done under CentOS. A reboot is needed."

elif hostnamectl | grep "Ubuntu" >/dev/null 2>/dev/null; then

	# Ubuntu
	echo " - Ubuntu detected!"

	# These checks are intended to find the best place to put the scripts under Ubuntu
	if [ -d /etc/initramfs-tools/hooks ] && [ -d /etc/initramfs-tools/scripts/init-premount ]; then
		hook_dest=/etc/initramfs-tools/hooks/rapiddisk_hook
		bootscript_dest=/etc/initramfs-tools/scripts/init-premount/rapiddisk
	elif [ -d /usr/share/initramfs-tools/hooks ] && [ -d /usr/share/initramfs-tools/scripts/init-premount ]; then
		hook_dest=/usr/share/initramfs-tools/hooks/rapiddisk_hook
		bootscript_dest=/usr/share/initramfs-tools/scripts/init-premount/rapiddisk
	else
		myerror "Error: I can't find any suitable place for initramfs scripts. Exiting..."
	fi

	if [ -n "$uninstall" ]; then
		echo " - Starting uninstall process..."

		if [ -x "$hook_dest" ] || [ -x "$bootscript_dest" ] || [ ! -z "$force" ]; then
			echo " - Uninstalling scripts..."
			rm -f "$hook_dest" 2>/dev/null
			rm -f "$bootscript_dest" 2>/dev/null
		else
			myerror "Error: hook and/or boot scripts are not installed, use '--force'. Exiting..."
		fi

		# TODO this check is not much useful since update-initrd includes many modules automatically and can lead to false
		#  positves

		#if lsinitramfs /boot/initrd.img-"$kernel_version" | grep rapiddisk >/dev/null 2>/dev/null ; then
		#	echo ""
		#	echo "Error: /boot/initrd.img-$kernel_version still contains rapiddisk stuff."
		#	echo
		#	exit 1
		#fi

	else
		if [ -x "$hook_dest" ] || [ -x "$bootscript_dest" ] && [ -z "$force" ]; then
			myerror "Error: initrd hook and/or boot scripts are already installed. You should use the '--force' option. Exiting..."
		elif [ ! -x "${cwd}/ubuntu/rapiddisk_hook" ] || [ ! -x "${cwd}/ubuntu/rapiddisk" ]; then
			myerror "Error: I can't find the scripts to be installed. Exiting..."
		else
			echo " - Copying ${cwd}/ubuntu/rapiddisk_hook to ${hook_dest}..."

			if ! cp "${cwd}/ubuntu/rapiddisk_hook" "${hook_dest}"; then
				myerror "Error: could not copy rapiddisk_hook to ${hook_dest}. Exiting..."
			fi

			chmod +x "${hook_dest}" 2>/dev/null

			echo " - Copying ${cwd}/ubuntu/rapiddisk to ${bootscript_dest}..."

			if ! cp "${cwd}/ubuntu/rapiddisk" "${bootscript_dest}"; then
				# if for any reason the first script was copied with no error, we remove it
				[ -f "${hook_dest}" ] && rm -f "${hook_dest}" 2>/dev/null
				myerror "Error: could not copy rapiddisk_hook to ${bootscript_dest}. Exiting..."
			fi

			sed -i 's/RAMDISKSIZE/'"${ramdisk_size}"'/' "${bootscript_dest}"
			sed -i 's,BOOTDEVICE,'"${root_device}"',' "${bootscript_dest}"

			chmod +x "${bootscript_dest}" 2>/dev/null
		fi
	fi

	echo " - Updating initramfs for kernel version ${kernel_version}..."
	echo ""
	update-initramfs -u -k "$kernel_version"
	echo ""
	echo " - Updating grub..."
	echo ""
	update-grub
	echo ""
	echo " - Done under Ubuntu. A reboot is needed."

else
	myerror "Error: operating system not supported. Exiting..."
fi

exit 0
