### Release 7.2.1 ###

- module: Added support for RHEL 8.4 kernel
- utility: Added support for libmicrohttpd v0.9.71 and newer while still supporting legacy versions

### Release 7.2.0 ###

- module: Updated for 5.12 kernels and later (thank you Michael)
- utility: remove unused headers (thank you Marcel Huber)
- utility: add CLI support for dm-writecache wrapper
- utility: Fixed property check bug when parsing sysfs block subtree params (github issue #55)

### Release 7.1.0 ###

- module: Updated for 5.9 kernels and later
- documentation: Updated copyrights

### Release 7.0.1 ###

- misc: Fixed typo in utility Makefile

### Release 7.0.0 ###

- module: Updated for 5.8 kernels and later
- module: fixed cache status format typo
- daemon: Implement http-driven API to monitor/manage rapiddisk/cache functions
- utility: Removing support for RHEL / CentOS 6.x
- utility: Restructured userspace CLI
- test: Restructured and improved test framework
- misc: Code / documentation cleanup

### Release 6.1 ###

- kernel: added support for 5.7 kernel
- utility: fixed GCC compilation warnings
- utility: code style cleanup
- installer: additional dkms installation/removal cleanup (thank you Shub77)
- documentation: fixed / updated README to reflect newer dkms installation/removal instructions
- documentation: updated copyright dates
- documentation: added AUTHORS file
- misc: added experimental scripts to enable rapiddisk/cache on root device during boot (thank you Shub77)

### Release 6.0-1 ###

- kernel: Fix dkms version in module/Makefile.
- installer: modified dkms installation/removal procedures (thank you Shub77)

### Release 6.0 ###

- kernel: Fixed module compilation error with modern version of GCC.
- utility: Remove dm-crypt code; Not sure why i had it in the first place. Doesn't really belong. Just use cryptsetup.
- utility: Removed archive/restore code and dependency on zlib. Again, can just use dd and tar. Is anyone even using this?
- misc: Updated licensing and logo location (thanks Boian!).

### Release 5.2 ###

- kernel: added support for 4.17 kernel.
- build: Cleaned up module clean Makefile.
- Updated Copyright years.

### Release 5.1 ###

- kernel: added support for 4.13 kernel.
- kernel: added support for 4.12 kernel (thank you Marcel Huber).
- utility: fixed compilation warnings (thank you Marcel Huber).

### Release 5.0 ###

- kernel: Remove kernel mainline specific code (intended for brd replacement).
- kernel: Change spinlock types to work better with virtio (github issue #13).
- test: Updated tests to a work with the modern version of RapidDisk.
- utility: Add JSON output for RapidDisk configuration (requires libjansson).
- www: remove fat-free (f3) RESTful API.

### Release 4.5 ###

- kernel: cache - Fixed I/O handler bug for 4.8+ kernels
- documentation: Cleaned up formatting and license disclaimers (thanks Boian!)

### Release 4.4 ###

- kernel: Update to 4.8 and 4.9 kernels.
- build: Cleaned up Makefiles (thanks Marcel!)

### Release 4.3 ###

- kernel: Add support for the 4.7 kernel (patch supplied by Marcel Huber)
- packaging: Updated DEB control for PHP changes between Ubuntu 14.04/16.04

### Release 4.2-3 ###

- utility: Fixed bug in `make install` with `nocrypt` enabled.
- documentation: clean up.

### Release 4.2-2 ###

- utility: Added more complex default DES key with backwards compatibility to legacy key.
- ha: added write around support to HA resource agents.
- documentation: corrections / clean up.

### Release 4.2 ###

- kernel: Added Write-Around support to `rapiddisk-cache`.
- kernel: Fixed `LINUX_VERSION_CODE` check for `rapiddisk-cache` to accommodate changes in 3.8.3.
- utility: Added a `nocrypt` build flag.
- utility: Added user definable keys for encryption setup.

### Release 4.1-2 ###

- kernel: Readjusted misaligned discard request check to build on kernels older than 4.3.

### Release 4.1 ###

- kernel: Refuse misaligned discard requests.
- kernel: Convert `ENOMEM` to `ENOSPC` when cannot alloc pages.
- kernel: Added 4k physical block size attribute.

### Release 4.0-4 ###

- Code cleanup.
- packaging: Also need to remove `CONFIG_BLK_DEV_RAM_COUNT` from distro specific packages.

### Release 4.0-3 ###

- utility: Fixed all references of RapidCache to RapidDisk-Cache.

### Release 4.0-2 ###

- utility: Updated fatfree-framework.
- utility: Addressed bug in RapidDisk REST implementation.

### Release 4.0 ###

- Fixed `libcryptsetup` build error for RHEL6.
- Renamed `rxdsk`/`rxcache` modules.
- Did massive cleanup of administration utility code.
- Converted most (if not all) return codes to POSIX.1 error numbers.
- Code cleanup in RESTful API.
- RapidDisk volumes now show up as non-rotational.
- Cleaned up ioctls in both module and utilies.

### Release 3.7 ###

- Cleaned up kernel module code.
- Fixed stack overflow bug in administration utility.
- Fixed error print statement in administration utility.
- Updated copyright years.
- Cleaned up build environment, including Makefiles.
- Fixed bug in configuring encryption on device.

### Release 3.6-3 ###

- Updated/corrected documentation in manual page and in source.

### Release 3.6-2 ###

- Fixed a memory leak in administration utility.
- Placed better checks before deallocating memory in administration utility.

### Release 3.6 ###

- Updated RapidDisk modules for Linux kernel version 4.4.

### Release 3.5-2 ###

- Updated kernel documentation.

### Release 3.5 ###

- Forcing `rxdsk` driver to do the drive enumeration. Removed functionality from administration utility.
- Appropriately initializing major number variable to 0 before registering block device module.
- Administration utility now checks for `sysfs` entry of `rxdsk` and not module name in `/proc/modules`.
- Converted sector size input in `rxdsk` module to KB input. Modified administration utility to this.
- Fixed bug in check for total `rxdsk` devices in module.

### Release 3.4-2 ###

- Updated Makefile for tools install to add HA scripts.
- Fixed bugs with HA scripts (both `rgmanager` and `pacemaker`).

### Release 3.4 ###

- Added ability to autoload RapidDisk volumes during module insertion.
- Fixed bug in RapidDisk (volatile) volume size definition across 32 to 64 bit types.
- Making use of `BIT()` macro in the driver.
- Removed RapidDisk-NV support. It was redundant with the recently kernel integrated `pmem` code.

### Release 3.3 ###

- Updated code for the 4.3 kernel.
- Cleaned up the main `Makefile`.
- Cleaned up entire driver code. Adjusted formatting.

### Release 3.2 ###

- Replaced `procfs` management to `sysfs`.
- Identified & corrected a couple of memory leaks.
- Massive code cleanup (intended for kernel submission).
- Minor code optimizations (slight performance improvements).

### Release 3.1-2 ###

- Fixed on-line menu of administration binary.
- Updated spec file to autoload modules after install.
- Corrected package description in spec file.

### Release 3.1 ###

- Fixed memory leak and an exit on failure before removing mutex during a `procfs` read.
- Added RESTful test file to test API from CLI.
- Integrated encryption support via `dm-crypt`.
- Enabled RPM builds for Red Hat / CentOS 6 & 7.
- Enabled RapidDisk YUM repo for Red Hat / CentOS 6 & 7 support.
- Added Pacemaker and rgmanager resource files to enable HA support.

### Release 3.0 ###

- Added NVDIMM support.
- Added RESTful API support.
- Updated administration binary and cleaned up a lot of its code.
- Removed pyRxAdm graphical wrapper.
- Fixed bug when erroring during RapidCache module insertion.
- Cleaned up RapidCache module code (removed `procfs` entry).

### Release 2.13 ###

- Bug fix with `rxadm` binary and mapping RapidCache to pre-existing partitions.
- Added more information to RapidDisk `procfs` file.
- Addressed compilation warnings for GCC 5.1

### Release 2.12 ###

- Updated modules for kernel 3.14

### Release 2.11 ###

- Updated `rxcache` for Red Hat 6.4 (device mapper conflict)
- Addressed incorrect description of maximum number of rxdsks supported.

### Release 2.10 ###

- Updated modules for kernel 3.10.
- Updated `Makefile` for cross compiling install.

### Release 2.9.2 ###

- Addressed a bug in `rxdsk` print statement (wrong type). Thanks go to Neo for discovering and patching it.

### Release 2.9.1 ###

- Minor update adding DKMS support.
- Adding support to build and install/uninstall tools separately (i.e. without modules, as in when installing with DKMS).

### Release 2.9 ###

- Added better implementation of `BLKFLSBUF` ioctl to rxdsk module. This will "flush data" and truncate pages.
- Added flush command to `rxadm` utility.
- Added support for Linux kernel 3.9. Tested on 3.9.2.

### Release 2.8 ###

- Cleaned up code and removed unused and unimplemented caching feature (write-around).
- Added support for Linux kernel 3.8. Tested on 3.8-rc7.

### Release 2.7 ###

- Made some modifications to the modules' makefile.
- Added support for Linux kernels 3.6 & 3.7. Tested on 3.6.9 and 3.7-rc8.

### Release 2.6 ###

- Minor `rxcache` kernel update: Make spinlocks less greedy by removing most of the disable ALL interupts spinlocks and replacing them with spinlocks to disable interrupts ONLY from bottom halves.

### Release 2.5 ###

- Added support for building in 3.4 and later Linux kernels.
- Update module Makefile to point to a different `DESTDIR` and `KSRC` (for cross-compiling)

### Release 2.4 ###

- Removed warning for RapidCache build. 
- Addressed an issue with `md` raid 1 (mirror) and using `rxdsk` in which the `md` driver would routinely send i/o of size 0 and `rxdsk` would return an `EIO`, failing the array. Problem and solution found and provided by Dmitry Trikoz of Stratus Technologies.

### Release 2.3 ###

- Addressed warning generated for kernels 3.2 and later with the return type of the `blk_queue_make_request` `request_queue` function.
- Added comments and cleaned error messages in pyRxAdm.
- Added comments to `rxadm` files.

### Release 2.2.1 ###

- Added additional functionality to pyRxAdm (add, map, archive, restore) also added some more error checking. 
- Fixed bug in `rxadm` (`archive.c`) during the archival process.
- Updated version no. (`cmd/common.h`, `rxcommon.h`) and removed b's to move from beta to production.
- Added a couple more switches to list version/help info of `rxadm` (`main.c`)
- Cleaned up `rxadm` logo for pyRxAdm (`rxadm_logo_48x48.png`)

### Release 2.2 ###

- Added cmd/pyRxAdm wrapper
- Modifed short-list feature output and modified error statement (`cmd.c`, `common.h`)
- Modified `cmd/Makefile`
- Added logo for wrapper (`misc/rxadm_logo_48x48.png`).
- Updated version no. (`rxcommon.h`)

### Release 2.1 ###

- Added `--short-list` support.
- Cleaned up debug messages on modules and added a couple of more.

### Release 2.0.1 ###

- Fixed bug #5 relating to using the `rxadm` utility without any nodes listed in `/dev/mapper`.
- Cleaned up a few messages in both `rxdsk.c` and `rxcache.c`.

### Release 2.0 ###

- Added `rxcache` write/read through caching module support.
- Added `rxcache` management features in `rxadm` utility.
- Modified input for archive/restore in `rxadm` to not use absolute path for `rxdsk` node. It maintains a form of consistency across all commands.

### Release 1.4 ###

- Fixed bug #4 by adding the `BLKFLSBUF` ioctl() command to process. This is specifically for when the user places an rxd node in an `mdadm` raid array.
	- Added a new test file to test the new ioctl command.
- Cleaned up the Makefiles a bit and now the user can build and install the kernel module from the root of the package tree as opposed to doing it from the module directory.

### Release 1.3.2 ###

- Fixed bug #3 which was for a warning during the build of `rxadm` on an x86_64 architecture. Thanks go to Gustaf Ullberg for discovering the root cause and providing a quick solution.

### Release 1.3.1r2 ###

- Removed the "b" from version strings to signify non-beta. This project seems to be production ready.
- Also added a test suite and some additional test tools for testing data integrity, performance, etc. This will help a lot for feature testing in future releases.

### Release 1.3.1b ###

- Added check in management utility to make sure that `rxdsk` node is present before archiving and restoring compressed/ decompressed images.

### Release 1.3b ###

- Added new feature to archive/restore an rxd volume to/from a zlib compressed data file (many thanks to Simon Ball for suggesting a similar feature).
- Added `discard` support.
- Added an ioctl to handle an invalid query sent by later versions of `udev` that correspond with Linux kernels 2.6.35 and above.
- Also integrated patch submitted by James Plummer of Stratus Technologies to address 32 bit limitation of `rxadm` utility `rxdsk` creation/resizing by casting the variable to a 64-bit type. Patch also included minor clean up code/optimizations for the same `rxadm` util.

### Release 1.2b ###

- Optimized the configuring of the request queue.
- Added checks to build from 2.6.32 all the way to the latest (currently 3.0.3).

### Release 1.1b ###

- Added support for dynamic resizing of attached `rxdsk` volumes.

### Release 1.0b ###

- Official public release.
