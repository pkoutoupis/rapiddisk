# Copyright © 2011 - 2021 Petros Koutoupis
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

CHMOD := chmod
CP := cp
RM := rm -f
MKDIR := mkdir -pv

BIN = rapiddisk
DIR := /sbin

.PHONY: all
all: $(BIN) $(BIN)d
	@echo Make has completed.

.PHONY: install
install: all
	@echo Installing all $(BIN) binary files.
	$(MKDIR) $(DESTDIR)$(DIR)
	$(CP) $(BIN)* $(DESTDIR)$(DIR)
	$(CHMOD) 755 $(DESTDIR)$(DIR)/$(BIN)*

.PHONY: uninstall
uninstall:
	@echo Uninstalling $(BIN) binary files.
	$(RM) $(DESTDIR)$(DIR)/$(BIN)*

json.o: json.c
	$(CC) -c json.c -o json.o

json-server.o: json.c
	$(CC) -c json.c -DSERVER -o json-server.o

net.o: net.c
ifeq ($(shell grep -q "enum MHD_Result" $(DESTDIR)/usr/include/microhttpd.h; echo $$?),1)
	$(CC) -c net.c -DLEGACY -o net.o
else
	$(CC) -c net.c -o net.o
endif

rdsk.o: rdsk.c
	$(CC) -c rdsk.c -o rdsk.o

sys.o: sys.c
	$(CC) -c sys.c -o sys.o

$(BIN): main.c json.o rdsk.o sys.o
	$(CC) main.c -o $(BIN) json.o rdsk.o sys.o -ljansson

$(BIN)d: daemon.c json-server.o net.o
	$(CC) daemon.c -o $(BIN)d json-server.o net.o -lpthread -ljansson -lmicrohttpd


.PHONY: clean
clean:
	rm -f *.o $(BIN) $(BIN)d
