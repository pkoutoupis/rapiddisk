# Copyright © 2011 - 2022 Petros Koutoupis
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

DEBUG_FLAGS := -DDEBUG -Wl,--export-dynamic -ggdb

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
	$(CHMOD) 755 $(DESTDIR)$(DIR)/$(BIN)
	$(CHMOD) 755 $(DESTDIR)$(DIR)/$(BIN)d

.PHONY: uninstall
uninstall:
	@echo Uninstalling $(BIN) binary files.
	$(RM) $(DESTDIR)$(DIR)/$(BIN)
	$(RM) $(DESTDIR)$(DIR)/$(BIN)d

json.o: json.c common.h cli.h
	$(CC) -c json.c -o json.o

json-server.o: json.c common.h
	$(CC) -c json.c -DSERVER -o json-server.o

net.o: net.c common.h daemon.h
	$(CC) -c net.c -o net.o

nvmet.o: nvmet.c common.h cli.h
	$(CC) -c nvmet.c -o nvmet.o

rdsk.o: rdsk.c common.h cli.h
	$(CC) -c rdsk.c -o rdsk.o

sys.o: sys.c common.h cli.h
	$(CC) -c sys.c -o sys.o

sys-server.o: sys.c common.h
	$(CC) -c sys.c -DSERVER -o sys-server.o

$(BIN): main.c json.o rdsk.o nvmet.o sys.o common.h cli.h
	$(CC) main.c -o $(BIN) json.o nvmet.o rdsk.o sys.o -ljansson

$(BIN)d: daemon.c json-server.o net.o sys-server.o common.h daemon.h
	$(CC) daemon.c -o $(BIN)d json-server.o net.o sys-server.o -lpthread -ljansson -lmicrohttpd

json_d.o: json.c common.h cli.h
	$(CC) $(DEBUG_FLAGS) -c json.c -o json_d.o

json-server_d.o: json.c common.h
	$(CC) $(DEBUG_FLAGS) -c json.c -DSERVER -o json-server_d.o

net_d.o: net.c common.h daemon.h
	$(CC) $(DEBUG_FLAGS) -c net.c -o net_d.o

nvmet_d.o: nvmet.c common.h cli.h
	$(CC) $(DEBUG_FLAGS) -c nvmet.c -o nvmet_d.o

rdsk_d.o: rdsk.c common.h cli.h
	$(CC) $(DEBUG_FLAGS) -c rdsk.c -o rdsk_d.o

sys_d.o: sys.c common.h cli.h
	$(CC) $(DEBUG_FLAGS) -c sys.c -o sys_d.o

sys-server_d.o: sys.c common.h
	$(CC) $(DEBUG_FLAGS) -c sys.c -DSERVER -o sys-server_d.o

$(BIN)_debug: main.c json_d.o rdsk_d.o nvmet_d.o sys_d.o common.h cli.h
	$(CC) $(DEBUG_FLAGS) main.c -o $(BIN)_debug json_d.o nvmet_d.o rdsk_d.o sys_d.o -ljansson

$(BIN)d_debug: daemon.c json-server_d.o net_d.o sys-server_d.o common.h daemon.h
	$(CC) $(DEBUG_FLAGS) daemon.c -o $(BIN)d_debug json-server_d.o net_d.o sys-server_d.o -lpthread -ljansson -lmicrohttpd

.PHONY: clean
clean:
	rm -f *.o $(BIN) $(BIN)d $(BIN)_debug $(BIN)d_debug

.PHONY: debug
debug: $(BIN)_debug $(BIN)d_debug
