# Copyright © 2011-2018 Petros Koutoupis
# All rights reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; under version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-2.0-only

VERSION = 6.0

ifeq ($(KSRC),)
	KSRC := /lib/modules/$(shell uname -r)/build
endif

ifeq ($(KVER),)
        KVER := $(shell uname -r)
endif

MKDIR := mkdir -pv

obj-m += rapiddisk.o
obj-m += rapiddisk-cache.o

all:
	$(MAKE) -C$(KSRC) M=$(PWD)

install:
	$(MKDIR) $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/
	install -o root -g root -m 0755 rapiddisk.ko $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/
	install -o root -g root -m 0755 rapiddisk-cache.ko $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/
	depmod -a

uninstall:
	rm -f $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/rapiddisk.ko
	rm -f $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/rapiddisk-cache.ko
	depmod -a

clean:
	rm -rf *.o *.ko *.symvers *.mod.c .*.cmd Module.markers modules.order *.o.*
	rm -rf .tmp_versions .rapiddisk.o.d *.unsigned *.sdtinfo.c .ctf/ .cache.mk

.PHONY: dkms
dkms:
	sudo mkdir -pv /usr/src/rapiddisk-$(VERSION)/
	sudo cp -v * /usr/src/rapiddisk-$(VERSION)/
	sudo dkms add -m rapiddisk -v $(VERSION)
	sudo dkms build -m rapiddisk -v $(VERSION)
	sudo dkms install -m rapiddisk -v $(VERSION)
