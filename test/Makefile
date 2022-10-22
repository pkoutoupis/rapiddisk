# Copyright © 2016 - 2022 Petros Koutoupis
# All rights reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-2.0-or-later

ifeq ($(CC),)
	CC := gcc -Werror
endif

BIN = rxflush rxio rxioctl rxro

.PHONY: all
all: $(BIN)

.PHONY: run-test
run-test: all
	./test-leaks.sh
	./cache-test.sh

.PHONY: clean
clean:
	rm -f $(BIN)

.PHONY: install install-strip uninstall debug tools-strip tools-debug tools-uninstall tools-install-strip tools-install clean-tools tools dkms-uninstall dkms-install
install install-strip uninstall debug tools-strip tools-debug tools-uninstall tools-install-strip tools-install clean-tools tools dkms-uninstall dkms-install:
