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

RM := rm -f
STRIP_CMD := strip -s
DIR := /sbin
DAEMON_CFLAGS := -DSERVER
INSTALL := install -D -m 755 -t $(DESTDIR)$(DIR)
INSTALL_STRIP := install -D -m 755 -s -t $(DESTDIR)$(DIR)
DEBUG_FLAGS := -DDEBUG -Wl,--export-dynamic -g $(CFLAGS)
DEBUG_LDFLAGS := -DDEBUG -Wl,--export-dynamic -g $(LDFLAGS)
ifndef CFLAGS
NDEBUG_CFLAGS := -O2 -Wno-unused-result -flto=auto -ffat-lto-objects -fstack-protector-strong
else
NDEBUG_CFLAGS := -O2 -Wno-unused-result -flto=auto -ffat-lto-objects -fstack-protector-strong $(CFLAGS)
endif
ifndef LDLIBS
COMMON_LDLIBS := -ljansson -ldevmapper -lpcre2-8
DAEMON_LDLIBS := -ljansson -ldevmapper -lpcre2-8 -lmicrohttpd
else
COMMON_LDLIBS := $(LDLIBS)
DAEMON_LDLIBS := $(LDLIBS)
endif
ifndef LDFLAGS
NDEBUG_LDFLAGS := -Wl,-Bsymbolic-functions -flto=auto -ffat-lto-objects -flto=auto -Wl,-z,relro
else
NDEBUG_LDFLAGS := -Wl,-Bsymbolic-functions -flto=auto -ffat-lto-objects -flto=auto -Wl,-z,relro $(LDFLAGS)
endif
ifndef CPPFLAGS
NDEBUG_CPPFLAGS := -Wdate-time
else
NDEBUG_CPPFLAGS := -Wdate-time $(CPPFLAGS)
endif
BIN_TOOL_NDEBUG := rapiddisk
BIN_DAEMON_NDEBUG := rapiddiskd
BIN_TOOL_DEBUG := rapiddisk_debug
BIN_DAEMON_DEBUG := rapiddiskd_debug
OBJS_TOOL_NDEBUG := main_ndebug.o utils_ndebug.o json_ndebug.o rdsk_ndebug.o nvmet_ndebug.o sys_ndebug.o
OBJS_DAEMON_NDEBUG := rapiddiskd_ndebug.o utils-server_ndebug.o json-server_ndebug.o nvmet-server_ndebug.o net_ndebug.o rdsk-server_ndebug.o sys-server_ndebug.o
OBJS_TOOL_DEBUG := main_debug.o utils_debug.o json_debug.o nvmet_debug.o rdsk_debug.o sys_debug.o
OBJS_DAEMON_DEBUG := rapiddiskd_debug.o utils-server_debug.o json-server_debug.o nvmet-server_debug.o net_debug.o rdsk-server_debug.o sys-server_debug.o
SRC := json.c main.c net.c nvmet.c rapiddiskd.c rdsk.c sys.c utils.c

.PHONY: all
all: $(BIN_TOOL_NDEBUG) $(BIN_DAEMON_NDEBUG)

# This checks avoid creating/including the .h dependencies Makefiles
# Disables parallelization if "clean" is present in the goal list
ifneq (,$(filter clean,$(MAKECMDGOALS)))
ifneq (1, $(words $(MAKECMDGOALS)))
.NOTPARALLEL:
endif
else
include $(SRC:.c=.d)
endif

.PHONY: clean
clean:
	rm -f *.d $(OBJS_TOOL_NDEBUG) $(OBJS_DAEMON_NDEBUG) $(OBJS_TOOL_DEBUG) $(OBJS_DAEMON_DEBUG) $(BIN_TOOL_NDEBUG) $(BIN_DAEMON_NDEBUG) $(BIN_TOOL_DEBUG) $(BIN_DAEMON_DEBUG)
    MAKECMDGOALS:=$(filter-out clean,$(MAKECMDGOALS)))

.PHONY: tools
tools: all

.PHONY: run-test
run-test: all debug

.PHONY: clean-tools
clean-tools: clean

.PHONY: tools-install
tools-install: install

.PHONY: debug
debug: $(BIN_TOOL_DEBUG) $(BIN_DAEMON_DEBUG)

.PHONY: tools-debug
tools-debug: debug

.PHONY: strip
strip: tools-strip

.PHONY: tools-strip
tools-strip: all
	$(STRIP_CMD) $(BIN_TOOL_NDEBUG)
	$(STRIP_CMD) $(BIN_DAEMON_NDEBUG)

.PHONY: install
install: all
	@echo Installing all $(BIN_TOOL_NDEBUG) $(BIN_DAEMON_NDEBUG) binary files.
	$(INSTALL) $(BIN_TOOL_NDEBUG) $(BIN_DAEMON_NDEBUG)

.PHONY: install-strip
install-strip: tools-install-strip

.PHONY: tools-install-strip
tools-install-strip: all
	@echo Installing stripped $(BIN_TOOL_NDEBUG) $(BIN_DAEMON_NDEBUG) binary files.
	$(INSTALL_STRIP) $(BIN_TOOL_NDEBUG) $(BIN_DAEMON_NDEBUG)

.PHONY: uninstall
uninstall:
	@echo Uninstalling $(BIN_TOOL_NDEBUG) $(BIN_DAEMON_NDEBUG) binary files.
	$(RM) $(DESTDIR)$(DIR)/$(BIN_TOOL_NDEBUG)
	$(RM) $(DESTDIR)$(DIR)/$(BIN_DAEMON_NDEBUG)

.PHONY: tools-uninstall
tools-uninstall: uninstall

%_debug.o: override CFLAGS += $(DEBUG_FLAGS)
%_debug.o: %.c %.d
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%-server_debug.o: %.c %.d
	$(CC) $(CPPFLAGS) $(DAEMON_CFLAGS) $(CFLAGS) -c $< -o $@

%_ndebug.o: CFLAGS = $(NDEBUG_CFLAGS)
%_ndebug.o: CPPFLAGS = $(NDEBUG_CPPFLAGS)
%_ndebug.o : %.c %.d
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%-server_ndebug.o: %.c %.d
	$(CC) $(CPPFLAGS) $(DAEMON_CFLAGS) $(CFLAGS) -c $< -o $@

$(BIN_TOOL_NDEBUG): LDLIBS = $(COMMON_LDLIBS)
$(BIN_TOOL_NDEBUG): LDFLAGS = $(NDEBUG_LDFLAGS)
$(BIN_TOOL_NDEBUG): $(OBJS_TOOL_NDEBUG)
	$(CC) $(LDFLAGS) -o $(BIN_TOOL_NDEBUG) $(OBJS_TOOL_NDEBUG) $(LOADLIBES) $(LDLIBS)

$(BIN_DAEMON_NDEBUG): LDLIBS = $(DAEMON_LDLIBS)
$(BIN_DAEMON_NDEBUG): LDFLAGS = $(NDEBUG_LDFLAGS)
$(BIN_DAEMON_NDEBUG): $(OBJS_DAEMON_NDEBUG)
	$(CC) $(LDFLAGS) -o $(BIN_DAEMON_NDEBUG) $(OBJS_DAEMON_NDEBUG) $(LOADLIBES) $(LDLIBS)

$(BIN_TOOL_DEBUG): override LDFLAGS += $(DEBUG_LDFLAGS)
$(BIN_TOOL_DEBUG): override LDLIBS += $(COMMON_LDLIBS)
$(BIN_TOOL_DEBUG): $(OBJS_TOOL_DEBUG)
	$(CC) $(LDFLAGS) -o $(BIN_TOOL_DEBUG) $(OBJS_TOOL_DEBUG) $(LOADLIBES) $(LDLIBS)

$(BIN_DAEMON_DEBUG): override LDFLAGS = $(DEBUG_LDFLAGS)
$(BIN_DAEMON_DEBUG): override LDLIBS = $(DAEMON_LDLIBS)
$(BIN_DAEMON_DEBUG): $(OBJS_DAEMON_DEBUG)
	$(CC) $(LDFLAGS) -o $(BIN_DAEMON_DEBUG) $(OBJS_DAEMON_DEBUG) $(LOADLIBES) $(LDLIBS)

.PHONY: tools-clean
tools-clean: clean

.PHONY: dkms-uninstall dkms-install
dkms-uninstall dkms-install:

%.d: %.c
    ifeq (,$(filter clean,$(MAKECMDGOALS)))
	@echo Creating Makefile $@
	@set -e; rm -f $@; \
	$(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1_ndebug.o \1_debug.o \1\.d : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$; \
	$(CC) -MM $(DAEMON_CFLAGS) $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1-server_ndebug\.o \1-server_debug\.o \1\.d : ,g' < $@.$$$$ >> $@; \
	rm -f $@.$$$$
    endif

%.c:
