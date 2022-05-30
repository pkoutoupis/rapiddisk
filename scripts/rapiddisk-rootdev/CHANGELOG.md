# Changelog

### Version 0.1.5

Released 2022-05-30

* rapiddisk-on-boot now tries to invoke dkms to build modules for the specified kernel if not present (Ubuntu)
* Added a prerm hook script called when a kernel package is removed (Ubuntu deb package)

### Version 0.1.4

Released 2022-05-28

* Added prerm and postinst actions to rapiddisk-on-boot debian files
* Improved workaround to attach ramdisk under Ubuntu
* Added a clean script to improve the workaround

### Version 0.1.3

Released 2022-05-07

* Now searches for data files in /etc/rapiddisk-on-boot first
* Can be installed as a deb package under Ubuntu
* Added man page

### Version 0.1.2

Released 2022-04-26

* Fixed issue under Ubuntu, which caused unsigned kernel images not to be detected, preventing installation. Thanks to 
  [Augusto7743](https://github.com/Augusto7743), and to [Mark Boddington](https://github.com/TuxInvader) (who provided the solution)
* Added rapiddisk executable presence check
* Added rapiddisk's modules presence check for the chosen kernel under Ubuntu

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
