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

SUBDIRS = src conf doc module test

.PHONY: all
all: $(SUBDIRS)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: clean
clean: $(SUBDIRS)

.PHONY: install
install: $(SUBDIRS)

.PHONY: uninstall
uninstall: $(SUBDIRS)

.PHONY: dkms-install
dkms-install:
	$(MAKE) -C module $@

.PHONY: dkms-uninstall
dkms-uninstall:
	$(MAKE) -C module $@

.PHONY: tools-install
tools-install:
	$(MAKE) -C src install
	$(MAKE) -C conf install
	$(MAKE) -C doc install

.PHONY: tools-uninstall
tools-uninstall:
	$(MAKE) -C src uninstall
	$(MAKE) -C conf uninstall
	$(MAKE) -C doc uninstall
