# Changelog

### Version 0.1.1

Released 2022-03-02

* Fixed issue in boot script under Ubuntu 18 and earlier
* Improved global uninstall under Ubuntu

### Version 0.1.0

Released 2021-12-12

* Added --cache-mode parameter
* Added sanity checks in Ubuntu initrd scripts
* Added many debug messages in Ubuntu initrd scripts
* Added a workaround to Ubuntu boot script in case the first ramdisk can't be attached
* Now initrd scripts are placed in /usr dir under Ubuntu
* The dm-writecache module is added to the initrd file only when needed under Ubuntu
* Fixed Centos script to report dependencies correctly
