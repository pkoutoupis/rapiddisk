![RapidDisk](logo.png)

Author: Petros Koutoupis (<petros@petroskoutoupis.com>)

Building and Installing the rapiddisk kernel modules and utilities
------------------------------------------------------------------

Change into the project's parent directory path.

> To build the rapiddisk management utility, you will need to have the
> `libjansson` and `libmicrohttpd` development library files installed
> on your host system.
>
> You are required to having either the full kernel source or the kernel
> headers installed for your current kernel revision.

To build rapiddisk from source, you would type the following on the command
line:

```console
# make
```

To install rapiddisk (must execute with superuser rights: `sudo`):

```console
# make install
```

To uninstall rapiddisk (must execute with superuser rights: `sudo`):

```console
# make uninstall
```

The rapiddisk utility will install in `/sbin/`

For utility information please reference the rapiddisk manual page:

```console
# man 1 rapiddisk
```

Inserting/Removing the rapiddisk / rapiddisk-cache kernel modules
-----------------------------------------------------------------

To insert the rapiddisk module:

```console
# modprobe rapiddisk
```

To remove the rapiddisk module:

```console
# modprobe -r rapiddisk
```

To insert the rapiddisk-cache module:

```console
# modprobe rapiddisk-cache
```

To remove the rapiddisk-cache module:

```console
# modprobe -r rapiddisk-cache
```

Building and installing / uninstalling the tools ONLY
-----------------------------------------------------

Installing:

```console
# make tools-install
```

Uninstalling:

```console
# make tools-uninstall
```

Installing modules for DKMS support
-----------------------------------

```console
# make dkms-install
```

Uninstalling modules for DKMS support
-----------------------------------

```console
# make dkms-uninstall
```

Managing the RapidDisk daemon service
--------------------------------------------------------

After installation, to start the service via systemd:

```console
# systemctl start rapiddiskd.service
```

To check the status of the service via systemd:

```console
# systemctl status rapiddiskd.service
```

To stop the service via systemd:

```console
# systemctl stop rapiddiskd.service
```

To start the service at boot via systemd:

```console
# systemctl enable rapiddiskd.service
```

