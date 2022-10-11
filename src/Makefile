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

CHMOD := chmod
CP := cp
RM := rm -f
MKDIR := mkdir -pv
STRIP := strip -s
LDLIBS += -ljansson -ldevmapper -lpcre2-8
DAEMON_LDLIBS := -lmicrohttpd
DAEMON_CFLAGS := -DSERVER
INSTALL := install -D -m 755 -t $(DESTDIR)$(DIR)
INSTALL_STRIP := install -D -m 755 -s -t $(DESTDIR)$(DIR)
DEBUG_FLAGS := -DDEBUG -Wl,--export-dynamic -ggdb -gdwarf
BIN := rapiddisk
BIN_D := rapiddiskd
BIN_DEBUG := rapiddisk_debug
BIN_D_DEBUG := rapiddiskd_debug
OBJS := main.o utils.o json.o rdsk.o nvmet.o sys.o
OBJS_D := rapiddiskd.o utils.o json-server.o net.o rdsk-server.o sys-server.o
OBJS_DEBUG := main_d.o utils_d.o json_d.o nvmet_d.o rdsk_d.o sys_d.o
OBJS_D_DEBUG := rapiddiskd_d.o utils_d.o json-server_d.o net_d.o rdsk-server_d.o sys-server_d.o

DIR := /sbin

.PHONY: all
all: $(BIN) $(BIN_D)
	@echo Make all has completed.

.PHONY: debug
debug: $(BIN_DEBUG) $(BIN_D_DEBUG)

.PHONY: strip
strip: all
	$(STRIP) $(BIN)
	$(STRIP) $(BIN_D)

.PHONY: install
install: all
	@echo Installing all $(BIN) $(BIN_D) binary files.
	$(INSTALL) $(BIN) $(BIN_D)

.PHONY: install-strip
install-strip: all
	@echo Installing stripped $(BIN) $(BIN_D) binary files.
	$(INSTALL_STRIP) $(BIN) $(BIN_D)

.PHONY: uninstall
uninstall:
	@echo Uninstalling $(BIN) $(BIN_D) binary files.
	$(RM) $(DESTDIR)$(DIR)/$(BIN)
	$(RM) $(DESTDIR)$(DIR)/$(BIN_D)

%-server_d.o: CFLAGS += $(DAEMON_CFLAGS)
%-server_d.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%-server.o: CFLAGS += $(DAEMON_CFLAGS)
%-server.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%_d.o: CFLAGS += $(DEBUG_FLAGS)
%_d.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $(BIN) $(OBJS) $(LOADLIBES) $(LDLIBS)

$(BIN_D): LDLIBS += $(DAEMON_LDLIBS)
$(BIN_D): $(OBJS_D)
	$(CC) $(LDFLAGS) -o $(BIN_D) $(OBJS_D) $(LOADLIBES) $(LDLIBS)

$(BIN_DEBUG): LDFLAGS += $(DEBUG_FLAGS)
$(BIN_DEBUG): $(OBJS_DEBUG)
	$(CC) $(LDFLAGS) -o $(BIN_DEBUG) $(OBJS_DEBUG) $(LOADLIBES) $(LDLIBS)

$(BIN_D_DEBUG): LDLIBS += $(DAEMON_LDLIBS)
$(BIN_D_DEBUG): LDFLAGS += $(DEBUG_FLAGS)
$(BIN_D_DEBUG): $(OBJS_D_DEBUG)
	$(CC) $(LDFLAGS) -o $(BIN_D_DEBUG) $(OBJS_D_DEBUG) $(LOADLIBES) $(LDLIBS)

.PHONY: clean
clean:
	rm -f *.o $(BIN) $(BIN_D) $(BIN_DEBUG) $(BIN_D_DEBUG)
