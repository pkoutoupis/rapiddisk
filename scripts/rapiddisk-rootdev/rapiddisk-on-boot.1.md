% RAPIDDISK-ON-BOOT(1) GNU
% [Matteo Tenca](https://github.com/matteotenca)
% 2022-05-30

# NAME
rapiddisk-on-boot - a simple bash script to add rapiddisk cache to your root volume

# SYNOPSIS
**rapiddisk-on-boot** **\-\-install** [**\-\-root=***root_partition*] **\-\-size=***ramdisk_size* **\-\-kernel=***kernel_version* **\-\-cache-mode=***mode* [**\-\-force**]\
**rapiddisk-on-boot** **\-\-uninstall** **\-\-kernel=***kernel_version* [**\-\-force**]\
**rapiddisk-on-boot** **\-\-global-uninstall**

# DESCRIPTION
To be able to map a rapiddisk cache to your \`/\` volume to speed it up,
the filesystem has to be umounted, and this is not possible. So, the
mapping must happen at boot time, before \`/\` is mounted. This script
adds the correct initrd related files to the system, and recreates the
initrd file(s) used at boot, so that, upon the next reboot, \`/\` will
end up mounted on a rapiddisk cached device.

## Long story

An initrd (intial ramdisk) file is a special kind of archive used during boot. The kernel, upon loading, may need to
perform some actions before it can mount the final root filesystem, such as loading furher kernel modules, or
make accessible a
partition placed on a software RAID array. To achieve these goals, an initrd file is specially
crafted, so that the kernel, upon boot, can load it, extract it to a ramdisk, mount it as a temporary
filesystem and access this preliminary boot environment to perform the kind of operations described above.
**rapiddisk-on-boot** adds to this operation list the mapping of a cache to the device where the root filesystem resides. So
that, when at last the root filesystem is mounted, it will be cached by rapiddisk.

## System-wide but kenel version-wise

rapiddisk's modules, like every kernel module, are specific for each kernel version, so must be built and installed
separately for each kernel you want to use them with.
On the other hand, the tools used to create initrd files follow a system-wide rule set. They are smart enough to
choose the right module version to add for the right kernel. But there is no way to tell them to use a different rule
set for each
kernel version. So, for example, the boot-time instruction to map a 100 MB ramdisk cache to /dev/sda1 would be
valid, and added, to every initrd files upon re-building, regardless of the kernel version it is built or rebuilt
for.
\
This script alters this approach: the rule set remains system-wide, but allows the user to choose different options
for each kernel version.

# OPTIONS

## Installing (\-\-install)
The installation process (**\-\-install** switch) can be performed several times. The options you provide will affect only one initrd file per time, i.e. the one related to the specified kernel version (**\-\-kernel** switch). If you use the **\-\-install** option using a kernel version for which a configuration was already created by **rapiddisk-on-boot**, it is mandatory to specify the **\-\-force** argument too to overwrite the configuration.

**\-\-install**
: triggers the installation mode

**\-\-root=***root_partition*
:  must be equal to the root device where \`/\` is mounted, for example \`/dev/sda1\`. If you omit this paramenter, the script will parse \`/etc/fstab\` to find the correct device by itself, and ask for confirmation before doing anything.

**\-\-size=***ramdisk_size*
: *ramdisk_size* indicates the number of megabyte rapiddisk will use as cache for your root device. For example \`--size=1000\` means your cache will be 1 GB big.

**\-\-cache-mode=***mode*
: *mode* indicates the caching behavoiur, must be \`wt\`, \`wa\` or \`wb\`. See **rapiddisk(1)**'s documentation for more.

**\-\-kernel=***kernel_version*
: instructs the script about which initrd file to update. On many linux distributions there are more than one kernel version installed at the same time, and there is one initrd file for each version.
: You can find the kernel version you are using with \`uname -r\`

**\-\-force**
: forces the script to perform a reinstall with the new parameters for the specified kernel version. It is mandatory when a configuration already exists for the provided kernel version.

## Uninstalling (\-\-uninstall)
From the initrd file linked to the specified kernel version, the rapiddisk cache creation will be removed. If rapiddisk was installed for other kernel versions, those installations will not be affected.

**\-\-uninstall**
: this switch causes the script to alter the system-wide rule set so that rapiddisk will not be invoked at boot time *for the specified kernel version only*. The system-wide rule set is not removed, just altered, so that it is possible to load rapiddisk on boot on different kernel versions independently.

**\-\-kernel=***kernel_version*
: instructs the script about which initrd file to update.

**\-\-force**
: trys to remove the installation even if it cannot be found.

## Global uninstalling (\-\-global-uninstall)
**\-\-global-uninstall**
: This operation removes everything the script could have installed, and rebuilds ALL the initrd files. A complete uninstallation is performed.

# FILES
*/usr/share/rapiddisk-on-boot/\**

# NOTES
You can see the content of an initrd with \`lsinitrd\` on CentOS and \`lsinitramfs\` on Ubuntu.

# BUGS
Under Centos 8, using xfs as the root filesystem, rapiddisk may not work if write-back mode is selected.

# EXAMPLES
**rapiddisk-on-boot \-\-install \-\-kernel=\`uname -r\` \-\-root=/dev/sda1 \-\-cache-mode=wa \-\-size=100**
: on next boot with the current running kernel will be created a 100 MB ramdisk and it will be attached to /dev/sda1

**rapiddisk-on-boot \-\-uninstall \-\-kernel=\`uname -r\`**
: removes creation of a rapiddisk cache at boot time for the current running kernel

**rapiddisk-on-boot \-\-global-uninstall**
: removes any file installed for every installed kernel - complete clean.

# SEE ALSO
**rapiddisk(1)**
