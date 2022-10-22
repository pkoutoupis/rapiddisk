# Copyright © 2011 - 2022 Petros Koutoupis
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

VERSION = 8.2.0

ifeq ($(KSRC),)
	KSRC := /lib/modules/$(shell uname -r)/build
endif

ifeq ($(KVER),)
	KVER := $(shell uname -r)
endif

MKDIR := mkdir -pv
CP := cp -v
DKMSFILES := rapiddisk.c rapiddisk-cache.c dkms.conf Makefile
DKMSDEST := /usr/src/rapiddisk-$(VERSION)

obj-m += rapiddisk.o
obj-m += rapiddisk-cache.o

all:
	$(MAKE) -C $(KSRC) M=$(CURDIR)

install: all
	$(MKDIR) $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/
	install -o root -g root -m 0755 rapiddisk.ko $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/
	install -o root -g root -m 0755 rapiddisk-cache.ko $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/
	depmod -a

uninstall:
	rm -f $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/rapiddisk.ko
	rm -f $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/rapiddisk-cache.ko
	depmod -a

clean:
	rm -rf *.o *.ko *.symvers *.mod.c .*.cmd Module.markers modules.order *.o.* built-in*
	rm -rf .tmp_versions .rapiddisk.o.d .rapiddisk-cache.o.d *.unsigned *.sdtinfo.c .ctf/ .cache.mk *.mod

dkms-install: clean uninstall
ifeq ($(shell which dkms >/dev/null 2>/dev/null; echo $$?),1)
	$(error No 'dkms' command found)
else ifeq ($(shell dkms status rapiddisk/$(VERSION) -k $(KVER) | grep '$(KVER)' >/dev/null 2>/dev/null; echo $$?),0)
	$(error rapiddisk version $(VERSION) is already installed for kernel $(KVER). Use 'make dkms-uninstall' first)
else
	dkms add . || true
	dkms install rapiddisk/$(VERSION) -k $(KVER)
endif

dkms-uninstall: clean uninstall
ifeq ($(shell which dkms >/dev/null 2>/dev/null; echo $$?),1)
	$(error No 'dkms' command found)
else ifeq ($(shell dkms status rapiddisk/$(VERSION) -k $(KVER) | grep '$(KVER)' >/dev/null 2>/dev/null; echo $$?),0)
	dkms remove rapiddisk/$(VERSION) -k $(KVER)
else
	$(error rapiddisk version $(VERSION) is not installed for kernel $(KVER))
endif

.PHONY: test
run-test: all

.PHONY: install-strip debug tools-strip tools-debug tools-uninstall tools-install-strip tools-install clean-tools tools
install-strip debug tools-strip tools-debug tools-uninstall tools-install-strip tools-install clean-tools tools:
