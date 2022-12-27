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

CP := cp
RM := rm -f
MKDIR := mkdir -pv -m 755
MAN_DIR := /usr/share/man/man1

.PHONY: all
all: 
	@echo Nothing to do.

.PHONY: install
install: all
	@echo Installing rapiddisk man pages.
	$(MKDIR) $(DESTDIR)$(MAN_DIR)
	$(CP) rapiddisk*.1 $(DESTDIR)$(MAN_DIR)/

.PHONY: uninstall
uninstall: 
	@echo Uninstalling rapiddisk man pages.
	$(RM) $(DESTDIR)$(MAN_DIR)/rapiddisk.1
	$(RM) $(DESTDIR)$(MAN_DIR)/rapiddiskd.1

.PHONY: clean install-strip debug tools-strip tools-debug tools-uninstall tools-install-strip tools-install clean-tools tools dkms-uninstall dkms-install
clean install-strip debug tools-strip tools-debug tools-uninstall tools-install-strip tools-install clean-tools tools dkms-uninstall dkms-install:
