### install_initrd.sh
##### A simple bash script to add rapiddisk cache to your root volume

###### Author: Matteo Tenca (<shub@shub.it>)

Warning: this is experimental - use at your own risk and only if you
know what you're doing.

To be able to map a rapiddisk cache to your `/` volume to speed it up,
the filesystem should be umounted. So the mapping must happen at boot
time. This script adds the correct initrd related files to the system,
and recreates the initrd file used at boot, so that at next reboot the
`/` volume will be cached.

**Note**: there is one initrd file for each kernel version installed in
your system (and bootable). This script modifies the updating process of
initrd files system-wide, and updates only one initrd file, the one
related to the `--kernel` parameter value. But if you update any other
initrd file by hand, before using the `--uninstall` command, you'll end
up with rapiddisk installed in it too.

### Usage when installing:
```
sudo ./install_initrd.sh --root=<root_partition> 
    --size=<ramdisk_size> --kernel=<kernel_version> [--force] 
```

#### --root=<root_partition>
<root_partition> must be be equal to the root device where `/` is
mounted, for example `/dev/sda1`. You can find it this way:

```
$ mount | grep -P 'on\s+/\s+'
/dev/sda1 on / type xfs (rw,relatime,seclabel,attr2,inode64,noquota)
```

The <root_partition> in this case is `/dev/sda1`. If you omit this
paramenter, the script will parse `/etc/fstab` to find the correct
device by itself. Then it will ask you to confirm. If you don't have any
clue about your root device, omit `--root` and check the script's
proposal.

#### --size=<ramdisk_size>

<ramdisk_size> indicates the number of megabyte rapiddisk will use as
cache for your root device. For example `--size=1000` means a 1 GB
cache.

#### --kernel=<kernel_version>

Instructs the script about which initrd file to update. On many linux
distributions there are more than one kernel version installed at the
same time, and there is one initrd file for each version.

You can find the kernel version you are using with `uname -r`:

```
$ uname -r
5.4.0-31-generic
```

If you specify `--kernel=5.4.0-31-generic`, only the initrd file
associated with that kernel version will be updated with all the
rapiddisk components and things. To keep it simple, if you're **sure**
you have a second bootable kernel installed other than the one you're
using, you can specify:

```
--kernel=`uname -r`
```

And go.

**Warning: any other initrd file that will be updated from now on, will
end up with rapiddisk installed, no matter the kernel version you
specified here. Use the `--uninstall` command if don't want this to
happen.**

**This is not a script's issue, it happens because initrd configuration
files are global.**

#### --force

This forces the script to reinstall the needed things and redo the jobs,
even if all seems to be already installed. If you force the reinstall
with a different `--root`, only the last one you specified will be
cached.

### Usage when uninstalling:

```
sudo ./install_initrd.sh --uninstall --kernel=<kernel_version> [--force] 
```

#### --uninstall

This switch causes the script to revert any change system-wide and
rebuild the initrd file. This way rapiddisk will not be started during  
the boot sequence. Since the initrd setup files are removed system-wide,
any future update of any initrd file will remove rapiddisk from it too.

#### --kernel=<kernel_version>

As above, instructs the script about which initrd file to update,
choosing it by the kernel version.

#### --force

Trys to remove installed files even if are not there. It will update the
initrd file too.

### Tips

You can see the content of an initrd with `lsinitrd` on CentOS and
`lsinitramfs` on Ubuntu:

CentOS
```
$ sudo lsinitrd /boot/initramfs-`uname -r`.img |grep -i rapid
rapiddisk
-rwxr-xr-x   2 root     root            0 Jan  3 19:12 usr/lib/dracut/hooks/pre-mount/00-run_rapiddisk.sh
-rwxr-xr-x   1 root     root        40000 Jan  3 19:12 usr/lib/modules/4.18.0-147.8.1.el8_1.x86_64/kernel/drivers/block/rapiddisk-cache.ko
-rwxr-xr-x   1 root     root        32008 Jan  3 19:12 usr/lib/modules/4.18.0-147.8.1.el8_1.x86_64/kernel/drivers/block/rapiddisk.ko
-rwxr-xr-x   1 root     root        36240 Jan  3 19:12 usr/sbin/rapiddisk
-rwxr-xr-x   2 root     root          120 Jan  3 19:12 usr/sbin/run_rapiddisk.sh
```

Ubuntu
```
$ sudo lsinitramfs /boot/initrd.img-`uname -r` |grep -i rapid
scripts/init-premount/rapiddisk
usr/lib/modules/5.4.0-31-generic/updates/dkms/rapiddisk-cache.ko
usr/lib/modules/5.4.0-31-generic/updates/dkms/rapiddisk.ko
usr/sbin/rapiddisk
```
