#!/bin/bash

VERSION='0.1.2'
echo "install_initrd.sh version $VERSION"

ihelp()  {

	echo "Usage:"
	echo "$(basename "$0") --help"
	echo "$(basename "$0") --install --root=<root_partition> --size=<ramdisk_size> --kernel=<kernel_version> --cache-mode=<cache_mode> [--force]"
	echo "$(basename "$0") --uninstall --kernel=<kernel_version> [--force]"
	echo "$(basename "$0") --global-uninstall"
	echo ""
	echo "Where:"
	echo ""
	echo "--help           prints this help and exit"
	echo ""
	echo "In '--install' mode:"
	echo "<root_partition> is the device mounted as '/' as in /etc/fstab, in the /dev/xxxn format"
	echo "                 if not provided, /etc/fstab will be parsed to determine it automatically"
	echo "                 and you'll be asked to confirm"
	echo "<ramdisk_size>   is the size in MB of the ramdisk to be used as cache"
	echo "<kernel_version> is needed to determine which initrd file to alter"
	echo "<cache_mode>     is the rapiddisk caching mode (wt, wa, wb)"
	echo "--force          even if everything is already in place, force reinstalling."
	echo "                 Can be useful to change the ramdisk size without perform an uninstall"
	echo ""
	echo "In '--uninstall' mode:"
	echo "<kernel_version> is needed to determine which initrd file to alter. Other initrd files are left intact"
	echo "--force          perform the uninstall actions even if there is nothing to remove"
	echo ""
	echo "In '--global-uninstall' mode:"
	echo "                 everything ever installed by the script will be removed once and for all"
	echo "                 all the initrd files will be rebuild"
	echo ""
	echo "Note: kernel_version is really important: if you end up with a system that"
	echo "cannot boot, you can choose another kernel version from the grub menu,"
	echo "boot successfully, and use the --uninstall command with the <kernel_version> of"
	echo "the non-booting kernel to create a working initrd file."
	echo ""
	echo "You can usually try 'uname -r' to obtain the current kernel version."
	echo ""

}

is_num() {

	[ "$1" ] && [ -z "${1//[0-9]/}" ]

} # Credits: https://stackoverflow.com/questions/806906/how-do-i-test-if-a-variable-is-a-number-in-bash

myerror() {

	echo "**** Error: $1 Exiting..."
	exit 1

}

centos_install () {

	echo " - Creating module's dir..."
	mkdir -p "${module_destination}/${module_name}"
	echo " - Copying module's files in place..."
	cp -f "${cwd}/${os_name}/${module_name}/run_rapiddisk.sh.orig" "${module_destination}/${module_name}/"
	cp -f "${cwd}/${os_name}/${module_name}/module-setup.sh" "${module_destination}/${module_name}/"
	echo " - chmod module's files..."
	chmod +x "${module_destination}/${module_name}/module-setup.sh"
	chmod -x "${module_destination}/${module_name}/run_rapiddisk.sh.orig"
	echo " - Activating rapiddisk for kernel version ${kernel_version}."
	echo >"${module_destination}/${module_name}/${kernel_version_file}" "${ramdisk_size}"
	echo >>"${module_destination}/${module_name}/${kernel_version_file}" "${root_device}"
	echo >>"${module_destination}/${module_name}/${kernel_version_file}" "${cache_mode}"

}

centos_end () {

	echo " - Running 'dracut --kver $kernel_version -f'"
	dracut --kver "$kernel_version" -f
	echo " - Done under CentOS. A reboot is needed."

}

ubuntu_install () {

	echo " - Creating kernel options file ..."
	echo >"${kernel_version_file_dest}" "${ramdisk_size}"
	echo >>"${kernel_version_file_dest}" "${root_device}"
	echo >>"${kernel_version_file_dest}" "${cache_mode}"
	echo " - Copying ${cwd}/ubuntu/rapiddisk_hook to ${hook_dest}..."
	if ! cp -f "${cwd}/ubuntu/rapiddisk_hook" "${hook_dest}" ; then
		myerror "could not copy rapiddisk_hook to ${hook_dest}."
	fi
	chmod +x "${hook_dest}" 2>/dev/null
	echo " - Copying ${cwd}/ubuntu/rapiddisk_boot to ${bootscript_dest}..."
	if ! cp -f "${cwd}/ubuntu/rapiddisk_boot" "${bootscript_dest}" ; then
		myerror "could not copy rapiddisk_boot to ${bootscript_dest}."
	fi
	chmod +x "${bootscript_dest}" 2>/dev/null
	echo " - Copying ${cwd}/ubuntu/rapiddisk_sub.orig to ${subscript_dest_orig}..."
	if ! cp -f "${cwd}/ubuntu/rapiddisk_sub.orig" "${subscript_dest_orig}"; then
		myerror "could not copy rapiddisk_sub.orig to ${subscript_dest_orig}."
	fi
	chmod -x "${subscript_dest_orig}" 2>/dev/null

}

ubuntu_end () {

	echo " - Updating initramfs for kernel version ${kernel_version}..."
	echo ""
	update-initramfs -u -k "$kernel_version"
	echo ""
	echo " - Updating grub..."
	echo ""
	update-grub
	echo ""
	echo " - Done under Ubuntu. A reboot is needed."

}

install_options_checks () {

	[ -n "$ramdisk_size" ] || myerror "missing argument '--size'."
	is_num "$ramdisk_size" || myerror "the ramdisk size must be a positive integer."
	[ -n "$cache_mode" ] || myerror "missing argument '--cache-mode'."
	cache_mode="$(echo "$cache_mode" | tr '[:upper:]' '[:lower:]')"
	if [[ ! "$cache_mode" = w[tab] ]] ; then
		myerror "cache mode in '--cache-mode parameter' must be one of 'wt', 'wa' or 'wb'."
	fi
	if [ -z "$root_device" ] ; then
		echo " - No root device was specified, we start looking for it in /etc/fstab..."
		root_line="$(grep -vE '^[ #]+' /etc/fstab | grep -m 1 -oP '^.*?[^\s]+\s+/\s+')"
		root_first_char="$(echo "$root_line" | grep -o '^.')"
		case $root_first_char in
			U)
				uuid="$(echo "$root_line" | grep -oP '[\w\d]{8}-([\w\d]{4}-){3}[\w\d]{12}')"
				root_device=/dev/"$(ls 2>/dev/null -l /dev/disk/by-uuid/*"$uuid"* | grep -oE '[^/]+$')"
				;;
			L)
				label="$(echo "$root_line" | grep -oP '=[^\s]+' | tr -d '=')"
				root_device=/dev/"$(ls 2>/dev/null -l /dev/disk/by-label/*"$label"* | grep -oE '[^/]+$')"
				;;
			/)
				device="$(echo "$root_line" | grep -oP '^[^\s]+')"
				root_device=/dev/"$(ls 2>/dev/null -l "$device" | grep -oP '[^/]+$')"
				;;
			*)
				myerror "could not find the root device from /etc/fstab. Use the '--root' option."
				;;
		esac
		# TODO this check must be improved
		if ! echo "$root_device" | grep -P '^/dev/\w{1,4}\d{0,99}$' >/dev/null 2>/dev/null ; then
			myerror "root_device '$root_device' must be in the form '/dev/xxx' or '/dev/xxxn with n as a positive integer. Use the '--root' option."
		fi
		echo " - Root device '$root_device' was found!"
		echo ' - Is it ok to use it? [yN]'
		read -r yn
		if [ ! "$yn" = "y" ] && [ ! "$yn" = "Y" ] ; then
			myerror "please use the '--root' option."
		fi
	fi

}

# checks for current user == root
whoami | grep '^root$' 2>/dev/null 1>/dev/null || myerror "sorry, this must be run as root."
# checks for rapiddisk executables
rapiddisk_command="$(which 2>/dev/null rapiddisk | head -n 1)"
if [ -z "$rapiddisk_command" ] ; then
	myerror "'rapiddisk' command not found."
fi
# looks for the OS name
hostnamectl="$(which 2>/dev/null hostnamectl | head -n 1)"
if [ -z "$hostnamectl" ] ; then
	myerror "'hostnamectl' command not found."
fi
if "$hostnamectl" 2>/dev/null | grep "CentOS" >/dev/null 2>/dev/null; then
	os_name="centos"
	kernel_installed="$(rpm -qa kernel-*| sed -E 's/^kernel-[^[:digit:]]+//'|sort -u)"
elif "$hostnamectl" 2>/dev/null | grep "Ubuntu" >/dev/null 2>/dev/null; then
	os_name="ubuntu"
	kernel_installed="$(dpkg-query --list | grep -P 'linux-image-(unsigned-)?\d' |grep '^.i'| awk '{ print $2 }'| sed -re 's,linux-image-(unsigned-)?,,')"
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
			if [ -n "$install_mode" ] ; then
				ihelp
				myerror "only one betweeen '--install, '--uninstall' and --global-uninstall can be specified."
			fi
			install_mode=global_uninstall
			shift # past argument with no value
			;;
		--install)
			if [ -n "$install_mode" ] ; then
				ihelp
				myerror "only one betweeen '--install, '--uninstall' and --global-uninstall can be specified."
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
		--cache-mode=*)
			cache_mode="${i#*=}"
			shift # past argument=value
            ;;
		--force)
			force=1
			shift # past argument with no value
			;;
		--help)
			ihelp
			exit 1
			;;
		-h)
			ihelp
			exit 1
			;;
		*)
			ihelp
			myerror "unknown argument."
			;;
	esac
done # Credits https://stackoverflow.com/questions/192249/how-do-i-parse-command-line-arguments-in-bash

# the action must always be specified
if [ -z "$install_mode" ] ; then
	ihelp
	myerror "one betweeen '--install, '--uninstall' and '--global-uninstall' must be specified."
fi
# --kernel option is mandatory, except when --global-uninstall is specified
if [ -z "$kernel_version" ] && [ ! "$install_mode" = "global_uninstall" ] ; then
	ihelp
	myerror "missing argument '--kernel'."
fi
# check if the kernel version specified is installed
if [ ! "$install_mode" = "global_uninstall" ] ; then
	for v in $kernel_installed
	do
		if [ "$v" = "$kernel_version" ] ; then
			kernel_found=1
			break
		fi
	done
	if [ -z $kernel_found ] ; then
		myerror "the kernel version you specified is not installed on the machine."
	fi
fi
# start installation
cwd="$(dirname "$0")"
if [ "$os_name" = "centos" ] ; then
	echo " - CentOS detected!"
	# prepare some vars
	module_destination="/usr/lib/dracut/modules.d"
	module_name="96rapiddisk"
	kernel_version_file="$kernel_version"
	# what should we do?
	if [ "$install_mode" = "simple_install" ] ; then
		# installing on CentOS
		# now we can perform some parameters' checks which would be senseless to do earlier
		install_options_checks
		if [ -z "$force" ] ; then
			# without --force, we have do some more checks
			if [ -d "${module_destination}/${module_name}" ] ; then
				# module installed, check for kernel activation file
				if [ -f "${module_destination}/${module_name}/${kernel_version_file}" ] ; then
					# everything is already installed
					myerror "module already installed and already active for kernel version $kernel_version. Use '--force' to reinstall."
				fi
			fi
		fi
		centos_install
		centos_end
	elif [ "$install_mode" = "simple_uninstall" ] ; then
		echo " - Uninstalling for kernel ${kernel_version}..."
		if [ -z "$force" ] ; then
			if [ ! -f "${module_destination}/${module_name}/${kernel_version_file}" ] ; then
				 myerror "module not active for kernel version ${kernel_version}. Use '--force' to remove it anyway."
			fi
		fi
		echo " - Removing rapiddisk from ${kernel_version} initrd file..."
		rm -f "${module_destination}/${module_name}/${kernel_version_file}"
		centos_end
	elif [ "$install_mode" = "global_uninstall" ] ; then
		echo " - Global uninstalling and rebuilding initrd for kernel ${kernel_version}..."
		rm -rf "${module_destination:?}/${module_name:?}"
		echo " - Rebuilding all the initrd files.."
		for k in $kernel_installed
		do
			echo " - dracut --kver $k -f"
			dracut --kver "$k" -f
		done
	fi
elif [ "$os_name" = "ubuntu" ] ; then
	echo " - Ubuntu detected!"
	# prepare some vars
	hooks_dir="/usr/share/initramfs-tools/hooks"
	scripts_dir="/usr/share/initramfs-tools/scripts/init-premount"
	if [ ! -d "$hooks_dir" ] || [ ! -d "$scripts_dir" ] ; then
		myerror "I can't find any suitable place to write initramfs' scripts."
	fi
	hook_dest="${hooks_dir}/rapiddisk_hook"
	bootscript_dest="${scripts_dir}/rapiddisk_boot"
	subscript_dest_orig="${hooks_dir}/rapiddisk_sub.orig"
	kernel_version_file="rapiddisk_kernel_${kernel_version}"
	kernel_version_file_dest="${hooks_dir}/${kernel_version_file}"
	# what should we do?
	if [ "$install_mode" = "simple_install" ] ; then
		# installing on Ubuntu
		# check if rapiddisk modules are installed for chosen kernel
		# TODO: move this check to install_options_checks function if is ok under CentOS too
		if modinfo >/dev/null 2>&1 -k "$kernel_version" -n rapiddisk ; then
			modules_found=1
		fi
		if [ -z "$modules_found" ] ; then
			myerror "no rapiddisk modules found for chosen kernel."
		fi
		if [ -z "$force" ] ; then
			if [ -f "${kernel_version_file_dest}" ] ; then
				myerror "the config file for kernel version ${kernel_version} is already installed. Use '--force' to reinstall it."
			fi
		fi
		# now we can perform some parameters' checks which would be senseless to do earlier
		install_options_checks
		# calls install function
		ubuntu_install
	elif [ "$install_mode" = "simple_uninstall" ] ; then
		if [ -z "$force" ] ; then
			if [ ! -f "${kernel_version_file_dest}" ] ; then
				myerror "it seems there is not a valid installation for kernel version ${kernel_version}. You should use the '--force' option."
			fi
		fi
		echo " - Uninstalling for kernel $kernel_version..."
		rm -f "${kernel_version_file_dest}" 2>/dev/null
	elif [ "$install_mode" = "global_uninstall" ] ; then
		echo " - Global uninstalling for all kernel versions.."
		echo " - Deleting all files and rebuilding all the initrd files.."
		rm -f "${hooks_dir:?}"/rapiddisk_kernel_*
		rm -f "${hook_dest}" 2>/dev/null
		rm -f "${bootscript_dest}" 2>/dev/null
		rm -f "${subscript_dest_orig}" 2>/dev/null
		kernel_version=all
	fi
	ubuntu_end
fi

exit 0
