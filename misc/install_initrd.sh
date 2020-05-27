#!/bin/bash

ihelp () {

    # echo "$(basename $0) - installs hook and boot script to enable rapiddisk cache on boot volume."
    # echo
    echo "Usage:"
    echo "$(basename "$0") --boot=<boot_partition> --size=<ramdisk_size> --kernel=<kernel_version> [--force]"
    echo "$(basename "$0") --kernel=<kernel_version> --uninstall"
    echo ""
    echo "Where:"
    echo ""
    echo "<boot_partition> is the device mounted as '/' as in /etc/fstab in the /dev/xxxn format"
    echo "<ramdisk_size> is the size in MB of the ramdisk to be used as cache"
    echo "<kernel_version> is needed to determine which initrd file to alter"
    echo "--force forces replacing already installed scripts"
    echo ""
    echo "--uninstall removes scripts and revert systems changes. Needs the '--kernel' option"
    echo "            and make the script ignore all the other options."
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


is_num () {

    [ "$1" ] && [ -z "${1//[0-9]}" ]

} # Credits: https://stackoverflow.com/questions/806906/how-do-i-test-if-a-variable-is-a-number-in-bash

myerror () {

    echo ""
    echo "$1"
    echo ""
    ihelp
    exit 1

}

finalstuff () {

    echo " - Updating initramfs for kernel version ${kernel_version}..."
    echo ""
    update-initramfs -u -k "$kernel_version"
    echo ""
    echo " - Updating grub..."
    echo ""
    update-grub
    echo ""
    echo " - All done. A reboot is needed."

}

for i in "$@" ; do
    case $i in
      --kernel=*)
            kernel_version="${i#*=}"
            shift # past argument=value
      ;;
      --uninstall)
            uninstall=1
            # shift # past argument with no value
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
            myerror "Error: Unknown argument. Exiting..."
            # unknown option
      ;;
    esac
done # Credits https://stackoverflow.com/questions/192249/how-do-i-parse-command-line-arguments-in-bash

if ! whoami | grep root 2>/dev/null 1>/dev/null ; then
    myerror "Sorry, this must be run as root."
fi

cwd="$(dirname $0)"

# This option is always mandatory (ubuntu/centos installing/uninstalling) so it is checked before all the others
if [ -z "$kernel_version" ] ; then
	myerror "Error: missing argument '--kernel'."
fi

# if we are NOT uninstalling we need some more checks
if [ -z $uninstall ] ; then
    if [ -z "$ramdisk_size" ] ; then
        myerror "Error: missing argument '--size'."
    fi

    if [ -z "$boot_device" ] ; then
        myerror "Error: missing argument '--boot'."
    fi

    if ! is_num "$ramdisk_size" ; then
        myerror "Error: The ramdisk size must be a positive integer. Exiting..."
    fi

    # TODO this check must be improved
    if ! echo "$boot_device" | grep -E '^/dev/[[:alpha:]]{1,4}[[:digit:]]{0,99}$' >/dev/null 2>/dev/null ; then
        myerror "Error: boot_device must be in the form '/dev/xxx' or '/dev/xxxn with n as a positive integer. Exiting..."
    fi
fi

# Looks for the OS
if hostnamectl | grep "CentOS Linux" >/dev/null 2>/dev/null ; then

    # CentOS
    echo " - CentOS detected!"

    if [ -z $uninstall ] ; then
        echo " - Installing by copying 96rapiddisk into /usr/lib/dracut/modules.d/..."
        rm -rf /usr/lib/dracut/modules.d/96rapiddisk
        cp -rf "${cwd}"/96rapiddisk /usr/lib/dracut/modules.d/
        echo " - Editing /usr/lib/dracut/modules.d/96rapiddisk/run_rapiddisk.sh..."
        sed -i 's/RAMDISKSIZE/'"${ramdisk_size}"'/' /usr/lib/dracut/modules.d/96rapiddisk/run_rapiddisk.sh
        sed -i 's,BOOTDEVICE,'"${boot_device}"',' /usr/lib/dracut/modules.d/96rapiddisk/run_rapiddisk.sh
        echo " - chmod +x /usr/lib/dracut/modules.d/96rapiddisk/* ..."
        chmod +x /usr/lib/dracut/modules.d/96rapiddisk/*
    else
        echo " - Uninstalling by removing dir /usr/lib/dracut/modules.d/96rapiddisk..."
        rm -rf /usr/lib/dracut/modules.d/96rapiddisk
    fi

    echo " - Running 'dracut --kver $kernel_version -f'"
    #dracut --add-drivers "rapiddisk rapiddisk-cache" --kver "$kernel_version" -f
    dracut --kver "$kernel_version" -f

    echo " - Done under CentOS."
    exit 0
elif hostnamectl | grep "Ubuntu" >/dev/null 2>/dev/null ; then

    # Ubuntu
    echo " - Ubuntu detected!"

    # These checks are intended to find the best place to put the scripts under Ubuntu
    if [ -d /etc/initramfs-tools/hooks ] && [ -d /etc/initramfs-tools/scripts/init-premount ] ; then
      hook_dest=/etc/initramfs-tools/hooks/rapiddisk_hook
      bootscript_dest=/etc/initramfs-tools/scripts/init-premount/rapiddisk
    elif [ -d /usr/share/initramfs-tools/hooks ] && [ -d /usr/share/initramfs-tools/scripts/init-premount ] ; then
      hook_dest=/usr/share/initramfs-tools/hooks/rapiddisk_hook
      bootscript_dest=/usr/share/initramfs-tools/scripts/init-premount/rapiddisk
    else
      myerror "Error: I can't find any suitable place for initramfs scripts."
    fi

    if [ -n "$uninstall" ] ; then
		echo " - Starting uninstall process..."

		if [ ! -x "$hook_dest" ] || [ ! -x "$bootscript_dest" ] && [ -z "$force" ] ; then
			myerror "Error: Initrd hook and/or boot scripts are NOT installed. You should use the '--force' option. Exiting..."
		else
			echo " - Uninstalling scripts..."
			rm -f "$hook_dest" 2>/dev/null
			rm -f "$bootscript_dest" 2>/dev/null
		fi
		# TODO this check is not much useful since update-initrd includes many modules automatically and can lead to false
		#  positves

		#if lsinitramfs /boot/initrd.img-"$kernel_version" | grep rapiddisk >/dev/null 2>/dev/null ; then
		#    echo ""
		#    echo "Error: /boot/initrd.img-$kernel_version still contains rapiddisk stuff."
		#    echo
		#    exit 1
		#fi

		finalstuff
		exit 0
	fi

	if [ ! -x "${cwd}/ubuntu/rapiddisk_hook" ] || [ ! -x "${cwd}/ubuntu/rapiddisk" ] ; then
        myerror "Error: I can't find the scripts to be installed. Exiting..."
    fi

    ##############################################################################
    # TODO The following piece of code was there to check if the user specified
    # the same device present in /etc/fstab - but if in fstab UUIDs are used, or
    # the LABEL= syntax, it looses meaning.
    ##############################################################################

    ##current_boot_device="$(grep -vE '^#' /etc/fstab|grep -m 1 -Eo '.*\s+/\s+'|awk 'BEGIN { OFS="\\s" } { print $1 }')"
    ##if ! echo "$current_boot_device" | grep -E '^'"$boot_device"'$' 2>/dev/null >/dev/null ; then
    ##    echo ""
    ##    echo "ERROR: The specified boot device is different from the one in /etc/fstab ('${current_boot_device}'). Exiting..."
    ##    echo ""
    ##    ihelp
    ##    exit 1
    ##fi

    ##############################################################################

    if [ -x "$hook_dest" ] || [ -x "$bootscript_dest" ] && [ -z "$force" ] ; then
		myerror "Error: Initrd hook and/or boot scripts are already installed. You should use the '--force' option. Exiting..."
    else
		echo " - Copying ${cwd}/ubuntu/rapiddisk_hook to ${hook_dest}..."
		if ! cp "${cwd}/ubuntu/rapiddisk_hook" "${hook_dest}" ; then
			myerror "Error: Could not copy rapiddisk_hook to ${hook_dest}. Exiting..."
		fi

		chmod +x "${hook_dest}"

		echo " - Copying ${cwd}/ubuntu/rapiddisk to ${bootscript_dest}..."
		if ! cp "${cwd}/ubuntu/rapiddisk" "${bootscript_dest}" ; then
	        myerror "Error: Could not copy rapiddisk_hook to ${bootscript_dest}. Exiting..."
		fi

		sed -i 's/RAMDISKSIZE/'"${ramdisk_size}"'/' "${bootscript_dest}"
		sed -i 's,BOOTDEVICE,'"${boot_device}"',' "${bootscript_dest}"

		chmod +x "${bootscript_dest}"
    fi

    finalstuff
else
    myerror "Operating system not supported. Exiting..."
fi

exit 0

