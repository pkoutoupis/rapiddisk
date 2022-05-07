# rapiddisk-on-boot
### A simple bash script to add rapiddisk cache to your root volume

##### Author: [Matteo Tenca](https://github.com/matteotenca) (<https://github.com/matteotenca/>)

**Warning**: this is **experimental** - use at your own risk and **only if you
know what you're doing.**

### Long story short

To be able to map a rapiddisk cache to your `/` volume to speed it up,
the filesystem has to be umounted, and this is not possible. So, the
mapping must happen at boot time, before `/` is mounted. This script
adds the correct initrd related files to the system, and recreates the
initrd file(s) used at boot, so that, upon the next reboot, `/` will
end up mounted on a rapiddisk cached device.

### Long story

An initrd (intial ramdisk) file is a special kind of archive used during boot. The kernel, upon loading, may need to
perform some actions before it can mount the final root filesystem, such as loading furher kernel modules, or
make accessible a
partition placed on a software RAID array. To achieve these goals, an initrd file is specially
crafted, so that the kernel, upon boot, can load it, extract it to a ramdisk, mount it as a temporary
filesystem and access this preliminary boot environment to perform the kind of operations described above.
`rapiddisk-on-boot` adds to this operation list the mapping of a cache to the device where the root filesystem resides. So
that, when at last the root filesystem is mounted, it will be cached by rapiddisk.

### System-wide but kenel version-wise

rapiddisk's modules, like every kernel module, are specific for each kernel version, so must be built and installed
separately for each kernel you want to use them with.
On the other hand, the tools used to create initrd files follow a system-wide rule set. They are smart enough to
choose the right module version to add for the right kernel. But there is no way to tell them to use a different rule
set for each
kernel version. So, for example, the boot-time instruction to map a 100 MB ramdisk cache to /dev/sda1 would be
valid, and added, to every initrd files upon re-building, regardless of the kernel version it is built or rebuilt
for.

This script alters this approach: the rule set remains system-wide, but allows the user to choose different options
for each kernel version.

### 1) Usage when installing (`--install`)
```
sudo rapiddisk-on-boot --install --root=<root_partition> 
    --size=<ramdisk_size> --kernel=<kernel_version> 
    --cache-mode=<mode> [--force] 
```

The installation process (`--install` switch) can be performed several times. The options you provide will affect only one initrd file per time, i.e. the one related to the specified kernel version (**--kernel** switch). If you use the `--install` option using a kernel version for which a configuration was already created by `rapiddisk-on-boot`, it is mandatory to specify the `--force` argument too to overwrite the configuration.


#### --root=<root_partition>
<root_partition> must be equal to the root device where `/` is
mounted, for example `/dev/sda1`. You can find it this way:

```
$ mount | grep -P 'on\s+/\s+'
/dev/sda1 on / type xfs (rw,relatime,seclabel,attr2,inode64,noquota)
```

The <root_partition> in this case is `/dev/sda1`.

**If you omit this paramenter, the script will parse `/etc/fstab` to find the correct device by itself, and ask for
confirmation before doing anything.**

If you don't have any clue about your root device, omit `--root` and check the script's
proposal.

#### --size=<ramdisk_size>

<ramdisk_size> indicates the number of megabyte rapiddisk will use as
cache for your root device. For example `--size=1000` means your cache will be 1 GB big.

#### --cache-mode=<cache_mode>

<cache_mode> indicates the caching behavoiur, must be `wt`, `wa` or `wb`.
See `rapiddisk`'s documentation for more.

#### --kernel=<kernel_version>

Instructs the script about which initrd file to update. On many linux
distributions there are more than one kernel version installed at the
same time, and there is one initrd file for each version.

You can find the kernel version you are using with `uname -r`:

```
$ uname -r
5.4.0-31-generic
```

If you're **sure** you have a second bootable kernel installed other than the one you're
using, you can specify:

```
--kernel=`uname -r`
```

And go.

#### --force

This forces the script to perform a reinstall with the new parameters for the specified kernel version. It is mandatory when a configuration already exists for the provided kernel version.

### 2) Usage when uninstalling (`--uninstall`)

```
sudo rapiddisk-on-boot --uninstall --kernel=<kernel_version> [--force] 
```

From the initrd file linked to the specified kernel version, the rapiddisk cache creation will be removed. If
rapiddisk was `--installed` for other kernel versions, those
installations will not be affected.

#### --uninstall

This switch causes the script to alter the system-wide rule set so that rapiddisk will not be invoked at boot time **for the specified kernel version only**.

Note: The system-wide rule set is not removed, just altered, so that it is possible to load rapiddisk on boot on different kernel versions independently.

#### --kernel=<kernel_version>

Instructs the script about which initrd file to update.

#### --force

Trys to remove the installation even if it cannot be found.

### 3) Usage when global uninstalling (`--global-uninstall`)

```
sudo rapiddisk-on-boot --global-uninstall 
```

This operation removes everything the script could have installed, and rebuilds ALL the initrd files. A complete
uninstallation is performed.

## Notes

* Under Centos 8, using xfs as the root filesystem, rapiddisk may not work if write-back mode is selected.

## Tips

You can see the content of an initrd with `lsinitrd` on CentOS and
`lsinitramfs` on Ubuntu:

#### CentOS
```
$ sudo lsinitrd /boot/initramfs-`uname -r`.img |grep -i rapidd
rapiddisk
-rwxr-xr-x   2 root     root            0 Jan  3 19:12 usr/lib/dracut/hooks/pre-mount/00-run_rapiddisk.sh
-rwxr-xr-x   1 root     root        40000 Jan  3 19:12 usr/lib/modules/4.18.0-147.8.1.el8_1.x86_64/extra/rapiddisk-cache.ko.xz
-rwxr-xr-x   1 root     root        32008 Jan  3 19:12 usr/lib/modules/4.18.0-147.8.1.el8_1.x86_64/extra/rapiddisk.ko.xz
-rwxr-xr-x   1 root     root        36240 Jan  3 19:12 usr/sbin/rapiddisk
-rwxr-xr-x   2 root     root          120 Jan  3 19:12 usr/sbin/run_rapiddisk.sh
$
```

To check the rapiddisk options for the current kernel:
```
$ sudo lsinitrd /boot/initramfs-`uname -r`.img -f usr/sbin/run_rapiddisk.sh
#!/bin/bash

modprobe rapiddisk
modprobe rapiddisk-cache
rapiddisk -a 300
rapiddisk -m rd0 -b /dev/sda2 -p wa
$
```

#### Ubuntu
```
$ sudo lsinitramfs /boot/initrd.img-`uname -r` |grep -i rapidd
scripts/init-premount/rapiddisk_boot
usr/lib/modules/5.4.0-37-generic/updates/dkms/rapiddisk-cache.ko
usr/lib/modules/5.4.0-37-generic/updates/dkms/rapiddisk.ko
usr/sbin/rapiddisk
usr/sbin/rapiddisk_sub
$
```
