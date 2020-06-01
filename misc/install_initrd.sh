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
	echo "                 if not provided, /etc/fstab will be parsed to determine it automatically"
	echo "<ramdisk_size>   is the size in MB of the ramdisk to be used as cache"
	echo "<kernel_version> is needed to determine which initrd file to alter"
	echo "--force forces   replacing already installed scripts. See README.md"
	echo ""
	echo "--uninstall removes scripts and revert systems changes. Needs the '--kernel' option"
	echo "			  and make the script ignore all the other options."
	echo "--force     issue the rm command even if the install dir seems not to exists"
	echo ""
	echo "Note: kernel_version is really important: if you end up with a system that"
	echo "cannot boot, you can choose another kernel version from the grub menu,"
	echo "boot successfully, and use the --uninstall command with the kernel_version of"
	echo "the non-booting kernel to restore it."
	echo ""
	echo "You can usually use 'uname -r' to obtain the current kernel version."
	echo ""

}

is_num()  {

	[ "$1" ] && [ -z "${1//[0-9]/}" ]

} # Credits: https://stackoverflow.com/questions/806906/how-do-i-test-if-a-variable-is-a-number-in-bash

myerror()  {

#	ihelp
	echo "**** Error: $1 Exiting..."
	exit 1

}

centos_install () {

	mkdir -p "${module_destination}/${module_name}"
	cp "${cwd}/${os_name}/${module_name}/run_rapiddisk.sh" "${module_destination}/${module_name}/${module_k_command}"
	cp "${cwd}/${os_name}/${module_name}/module-setup.sh" "${module_destination}/${module_name}/"
	echo " - Editing module's file..."
	sed -i 's/RAMDISKSIZE/'"${ramdisk_size}"'/' "${module_destination}/${module_name}/${module_k_command}"
	sed -i 's,BOOTDEVICE,'"${root_device}"',' "${module_destination}/${module_name}/${module_k_command}"
	echo " - chmod +x module's files..."
	chmod +x "${module_destination}/${module_name}/*"
	echo " - Activating the script for the chosen kernel version..."
	touch "${module_destination}/${module_name}/${kernel_version_file}"

}

centos_end () {

	echo " - Running 'dracut --kver $kernel_version -f'"
#	dracut --kver "$kernel_version" -f
	echo " - Done under CentOS. A reboot is needed."

}

ubuntu_install () {

	echo " - Copying ${cwd}/ubuntu/rapiddisk_hook to ${hook_dest}..."

	if ! cp "${cwd}/ubuntu/rapiddisk_hook" "${hook_dest}"; then
		myerror "could not copy rapiddisk_hook to ${hook_dest}."
	fi

	chmod +x "${hook_dest}" 2>/dev/null

	echo " - Copying ${cwd}/ubuntu/rapiddisk to ${bootscript_dest}..."

	if ! cp "${cwd}/ubuntu/rapiddisk" "${bootscript_dest}"; then
		# if for any reason the first file was copied, we remove it
		[ -f "${hook_dest}" ] && rm -f "${hook_dest}" 2>/dev/null
		myerror "could not copy rapiddisk_hook to ${bootscript_dest}."
	fi

	sed -i 's/RAMDISKSIZE/'"${ramdisk_size}"'/' "${bootscript_dest}"
	sed -i 's,BOOTDEVICE,'"${root_device}"',' "${bootscript_dest}"

	sed -i 's/KERNELVERSION/'"${kernel_version}"'/g' "${hook_dest}"
	sed -i 's/KERNELVERSION/'"${kernel_version}"'/g' "${bootscript_dest}"

	chmod +x "${bootscript_dest}" 2>/dev/null

}

ubuntu_end () {

	echo " - Updating initramfs for kernel version ${kernel_version}..."
	echo ""
#	update-initramfs -u -k "$kernel_version"
	echo ""
	echo " - Updating grub..."
	echo ""
#	update-grub
	echo ""
	echo " - Done under Ubuntu. A reboot is needed."

}

# we check for user == root
whoami | grep root 2> /dev/null 1> /dev/null || myerror "Error: sorry, this must be run as root. Exiting..."

# Looks for the OS
if hostnamectl | grep "CentOS Linux" > /dev/null 2> /dev/null; then
	echo " - CentOS detected!"
	os_name="centos"
	install_mode="$install_mode $os_name"
elif hostnamectl | grep "Ubuntu" >/dev/null 2>/dev/null; then
	echo " - Ubuntu detected!"
	os_name="ubuntu"
	install_mode="$install_mode $os_name"
else
	myerror "operating system not supported."
fi

# parsing arguments
for i in "$@"; do
	case $i in
		--kernel=*)
			kernel_version="${i#*=}"
			shift # past argument=value
			;;
		--uninstall)
			install_mode=simple_uninstall
			shift # past argument with no value
			;;
		--global-uninstall)
			if [ ! -z "$install_mode" ] ; then
				myerror "only one betweeen '--install, '--uninstall' and ---uninstall-global can be specified."
			fi
			install_mode=global_uninstall
			shift # past argument with no value
			;;
		--install)
			if [ ! -z "$install_mode" ] ; then
				myerror "only one betweeen '--install, '--uninstall' and ---uninstall-global can be specified."
			fi
			install_mode=simple_install
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
			ihelp
			myerror "unknown argument."
			# unknown option
			;;
	esac
done # Credits https://stackoverflow.com/questions/192249/how-do-i-parse-command-line-arguments-in-bash

# what to do must always be specified
if [ -z "$install_mode" ] ; then
	myerror "one betweeen '--install, '--uninstall' and '---uninstall-global' must be specified."
fi

# --kernel option is mandatory, except when --global-uninstall is specified
if { [ -z "$kernel_version" ] && [ ! "$nstall_mode" = "global_uninstall" ] ; } ; then
	myerror "missing argument '--kernel'."
fi

cwd="$(dirname $0)"

if [ "$install_mode" = "simple_install centos" ] ; then
	# installing on CentOS

	# prepare some vars
	module_destination="/usr/lib/dracut/modules.d"
	module_name="96rapiddisk"
	module_command="run_rapiddisk.sh"
	kernel_version_file="$kernel_version"
	module_k_command="${module_destination}/${module_name}/${$kernel_version}_run_rapiddisk.sh"

	# with --force, we install
	if [ ! -z "$force" ] ; then
		centos_install
	fi

	# check for dracut rapiddisk module situation
	if [ -d "${module_destination}/${module_name}" ] ; then
		# module installed, check for kernel activation file
		module_installed=1
		if [ -f "${module_destination}/${module_name}/${kernel_version_file}" ] ; then
			# everything is already installed
			kernel_active=1
		fi
	fi

	if [ ! -z "$kernel_active" ] ; then
		# nothing to do
		myerror "module already installed and already active for kernel version $kernel_version. Use '--force' to
		reinstall."
	elif [ ! -z "$module_installed" ] ; then
		# module installed, but inactive for the kernel version specified
		echo " - Module already installed, activating it for kernel version $kernel_version."
		touch "${module_destination}/${module_name}/${kernel_version}" || myerror "error while activating rapiddisk
		module for kernel version $kernel_version."
	else
		# the module is not installed, do that and activate it for this kernel version
		centos_install
	fi

elif [ "$install_mode" = "simple_install ubuntu" ] ; then
	# installing on Ubuntu

	# prepare some vars
	etc_dir_hooks="/etc/initramfs-tools/hooks"
	etc_dir_scripts="/etc/initramfs-tools/scripts/init-premount"
	usr_dir_hooks="/usr/share/initramfs-tools/hooks"
	usr_dir_scripts="/usr/share/initramfs-tools/scripts/init-premount"

	# These checks are intended to find the best place to put the scripts under Ubuntu
	if [ -d "$etc_dir_hooks" ] && [ -d "$etc_dir_scripts" ]; then
		hook_dest="${etc_dir_hooks}/rapiddisk_hook_${kernel_version}"
		bootscript_dest="${etc_dir_scripts}/rapiddisk_${kernel_version}"
	elif [ -d "$usr_dir_hooks" ] && [ -d "$usr_dir_scripts" ]; then
		hook_dest="${usr_dir_hooks}/rapiddisk_hook_${kernel_version}"
		bootscript_dest="${usr_dir_scripts}/rapiddisk_${kernel_version}"
	else
		myerror "I can't find any suitable place for initramfs scripts."
	fi

	# with --force, we just install
	if [ ! -z "$force" ] ; then
		ubuntu_install
		ubuntu_end
	fi

	if [ -x "$hook_dest" ] || [ -x "$bootscript_dest" ]; then
		myerror "initrd hook and/or boot scripts are already installed. You should use the '--force' option."
	else
		ubuntu_install
		ubuntu_end
	fi
fi

exit 0

#
#	if [ -n "$uninstall" ]; then
#		echo " - Starting uninstall process..."
#
#		if [ -x "$hook_dest" ] || [ -x "$bootscript_dest" ] || [ ! -z "$force" ]; then
#			echo " - Uninstalling scripts..."
#			rm -f "$hook_dest" 2>/dev/null
#			rm -f "$bootscript_dest" 2>/dev/null
#		else
#			myerror "Error: hook and/or boot scripts are not installed, use '--force'. Exiting..."
#		fi
#
#		# TODO this check is not much useful since update-initrd includes many modules automatically and can lead to false
#		#  positves
#
#		#if lsinitramfs /boot/initrd.img-"$kernel_version" | grep rapiddisk >/dev/null 2>/dev/null ; then
#		#	echo ""
#		#	echo "Error: /boot/initrd.img-$kernel_version still contains rapiddisk stuff."
#		#	echo
#		#	exit 1
#		#fi
#
#	else
#
#	fi
#
#	echo " - Updating initramfs for kernel version ${kernel_version}..."
#	echo ""
#	update-initramfs -u -k "$kernel_version"
#	echo ""
#	echo " - Updating grub..."
#	echo ""
#	update-grub
#	echo ""
#	echo " - Done under Ubuntu. A reboot is needed."
#
#
##
#
## If we are NOT uninstalling, we need some more checks
#if [ -z "$uninstall" ] ; then
#	if [ -z "$root_device" ] ; then
#		echo " - No root device was specified, we start looking for it in /etc/fstab..."
#
#		root_line="$(grep -vE '^[ #]+' /etc/fstab | grep -m 1 -oP '^.*?[^\s]+\s+/\s+')"
#		root_first_char="$(echo $root_line | grep -o '^.')"
#
#		case $root_first_char in
#			U)
#				uuid="$(echo $root_line | grep -oP '[\w\d]{8}-([\w\d]{4}-){3}[\w\d]{12}')"
#				root_device=/dev/"$(ls 2>/dev/null -l /dev/disk/by-uuid/ | grep $uuid | grep -oE '[^/]+$')"
#				;;
#			L)
#				label="$(echo $root_line | grep -oP '=[^\s]+' | tr -d '=')"
#				root_device=/dev/"$(ls 2>/dev/null -l /dev/disk/by-label/ | grep $label | grep -oE '[^/]+$')"
#				;;
#			/)
#				device="$(echo $root_line | grep -oP '^[^\s]+')"
#				root_device=/dev/"$(ls 2>/dev/null -l $device | grep -oP '[^/]+$')"
#				;;
#			*)
#				myerror "Error: could not find the root device from /etc/fstab. Use the '--root' option. Exiting..."
#				;;
#		esac
#
#		# TODO this check must be improved
#		if ! echo "$root_device" | grep -P '^/dev/\w{1,4}\d{0,99}$' >/dev/null 2>/dev/null ; then
#			myerror "Error: root_device '$root_device' must be in the form '/dev/xxx' or '/dev/xxxn with n as a positive integer. Exiting..."
#		fi
#
#		echo " - Root device '$root_device' was found!"
#		echo ' - Is it ok to use it? [yN]'
#		read -r yn
#
#		{ [ "$yn" = "y" ] || [ "$yn" = "Y" ] ; } || myerror "Error: the root device we found was not ok. Use the '--root' option. Exiting..."
#	fi
#
#fi
#
#
#if [ ! -z "$centos" ] ; # CentOS
#	echo " - CentOS detected!"
#
#	if [ ! -z "$install" ] ; then
#
#
#
#
#	if [ ! -z "$forced" ] ; then
#		# This an ununforced operation - we check if the dracut's module is already installed
#		if { [ -d /usr/lib/dracut/modules.d/96rapiddisk ] ||
#		[ -f "/usr/lib/dracut/modules.d/96rapiddisk/$kernel_version" ] ; } ;  then
#			myerror "Error: dracut's rapiddisk module seems to be already there.  Use '--force'. Exiting..."
#			fi
#	fi
#
#	if [ -z "$uninstall" ] ; then
#
#		# This check is performed
#
#
#	# Check if we have to install or uninstall
#	if [ ! -z "$uninstall" ] ; then
#		# We have to install the module
#
#		# Installing dracuts's module and switch it on for the kernel version specified
#
#	else
#		# Uninstalling
#
#
#		echo " - Uninstalling by removing dir /usr/lib/dracut/modules.d/96rapiddisk..."
#		if [ ! -d /usr/lib/dracut/modules.d/96rapiddisk ] && [ -z "$force" ] ; then
#			myerror "Error: /usr/lib/dracut/modules.d/96rapiddisk does not exist, use '--force'. Exiting..."
#		fi
#		# rm -rf /usr/lib/dracut/modules.d/96rapiddisk
#		touchfile="/usr/lib/dracut/modules.d/96rapiddisk/$kernel_version"
#		if [ -f "$touchfile" ] ; then
#			rm -f "$touchfile"
#		else
#			myerror "Error: $touchfile does not exist. Exiting..."
#		fi
#	fi
#	echo " - Running 'dracut --kver $kernel_version -f'"
#	dracut --kver "$kernel_version" -f
#
#	echo " - Done under CentOS. A reboot is needed."
#
#elif hostnamectl | grep "Ubuntu" >/dev/null 2>/dev/null; then
#
#
#else
#
#fi

#exit 0
