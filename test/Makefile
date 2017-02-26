# Copyright © 2016 -2017 Petros Koutoupis
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

ifeq ($(CC),)
	CC := gcc -Werror
endif

BIN = rxflush rxio rxioctl

.PHONY: all
all: $(BIN)
	@echo Make has completed.

.PHONY: install
install: all
	@echo Nothing to do.

.PHONY: uninstall
uninstall:
	@echo Nothing to do.

rxflush: rxflush.c
	$(CC) rxflush.c -o rxflush

rxio: rxio.c
	$(CC) rxio.c -o rxio

rxioctl: rxioctl.c
	$(CC) rxioctl.c -o rxioctl

.PHONY: clean
clean:
	rm -f $(BIN)
