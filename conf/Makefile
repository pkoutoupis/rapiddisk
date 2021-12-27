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
RM := rm -rf
MKDIR := mkdir -pv -m 755
SERVICE = rapiddisk
CFG_DIR := /etc/$(SERVICE)/
SYSD_DIR := /etc/systemd/system
CHMOD := chmod

.PHONY: all
all: 
	@echo Nothing to do.

.PHONY: install
install: all
	@echo Installing rapiddisk high availability and rapiddiskd daemon files.
	$(MKDIR) $(DESTDIR)$(CFG_DIR)
	$(CP) $(SERVICE).* $(DESTDIR)$(CFG_DIR)
	$(CHMOD) -R 755 $(DESTDIR)$(CFG_DIR)
	$(MKDIR) $(DESTDIR)$(SYSD_DIR)
	$(CP) $(SERVICE)d.* $(DESTDIR)$(SYSD_DIR)
	$(CHMOD) -R 755 $(DESTDIR)$(SYSD_DIR)

.PHONY: uninstall
uninstall: 
	@echo Uninstalling rapiddisk high availability and rapiddiskd daemon files.
	$(RM) $(DESTDIR)$(CFG_DIR)
	$(RM) $(DESTDIR)$(SYSD_DIR)/$(SERVICE)d.*

.PHONY: clean
clean:
	@echo Nothing to do.
