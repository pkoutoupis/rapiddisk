# Copyright © 2011 - 2018 Petros Koutoupis
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
#
# SPDX-License-Identifier: GPL-2.0-or-later

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

.PHONY: install
install: all
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

main.o: core.o
ifeq ($(RH_VER_NUM), 6)
	$(CC) main.c -DNO_JANSSON -o rapiddisk core.o
else
	$(CC) main.c -o rapiddisk core.o -ljansson
endif

rapiddisk: main.o

.PHONY: clean
clean:
	rm -f *.o $(BIN)
