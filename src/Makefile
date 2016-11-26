# Copyright © 2011-2016 Petros Koutoupis
# All rights reserved.
#
# This file is part of RapidDisk.
#
# RapidDisk is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# RapidDisk is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with RapidDisk.  If not, see <http://www.gnu.org/licenses/>.

ifeq ($(CC),)
	CC := gcc -Werror
endif

ifneq ("$(wildcard /etc/redhat-release)", "")
	RH_VER_NUM := $(shell /bin/grep -o [0-9].[0-9] /etc/redhat-release|cut -d'.' -f1)
else
	RH_VER_NUM := 0
endif

CHMOD := chmod
CP := cp
RM := rm -f
MKDIR := mkdir -pv

BIN = rapiddisk
DIR := /sbin

.PHONY: all
all: rapiddisk
	@echo Make has completed.

.PHONY: nocrypt
nocrypt: rapiddisk-nocrypt
	@echo Make has completed.

.PHONY: install
install: all
	@echo Installing all binary files.
	$(MKDIR) $(DESTDIR)$(DIR)
	$(CP) $(BIN) $(DESTDIR)$(DIR)
	$(CHMOD) 755 $(DESTDIR)$(DIR)/$(BIN)*
	ln -s $(BIN) $(DESTDIR)$(DIR)/rxadm

.PHONY: nocrypt-install
nocrypt-install: nocrypt
	@echo Installing all binary files.
	$(MKDIR) $(DESTDIR)$(DIR)
	$(CP) $(BIN) $(DESTDIR)$(DIR)
	$(CHMOD) 755 $(DESTDIR)$(DIR)/$(BIN)*
	ln -s $(BIN) $(DESTDIR)$(DIR)/rxadm

.PHONY: uninstall
uninstall:
	@echo Uninstalling rapiddisk files.
	$(RM) $(DESTDIR)$(DIR)/$(BIN)*
	$(RM) $(DESTDIR)$(DIR)/rxadm

core.o: core.c
ifeq ($(RH_VER_NUM), 6)
	$(CC) -c core.c -DNO_JANSSON -o core.o
else
	$(CC) -c core.c -o core.o
endif

archive.o: archive.c
	$(CC) -c archive.c -o archive.o

crypt.o: crypt.c
ifeq ($(RH_VER_NUM), 6)
	$(CC) -c crypt.c -DLEGACY_CRYPT -o crypt.o
else
	$(CC) -c crypt.c -o crypt.o
endif

crypt-nocrypt.o: crypt.c
	$(CC) -c crypt.c -DNO_CRYPT -o crypt-nocrypt.o

main.o: core.o archive.o crypt.o
ifeq ($(RH_VER_NUM), 6)
	$(CC) main.c -DNO_JANSSON -o rapiddisk core.o archive.o crypt.o -lz -lcryptsetup
else
	$(CC) main.c -o rapiddisk core.o archive.o crypt.o -lz -lcryptsetup -ljansson
endif

main-nocrypt.o: core.o archive.o crypt-nocrypt.o
	$(CC) main.c -DNO_CRYPT -o rapiddisk core.o archive.o crypt-nocrypt.o -lz -ljansson

rapiddisk: main.o

rapiddisk-nocrypt: main-nocrypt.o

.PHONY: clean
clean:
	rm -f *.o $(BIN)
