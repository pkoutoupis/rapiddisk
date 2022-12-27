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

MODULE_SUBDIR = module
TEST_SUBDIR = test
TOOLS_SUBDIR = src
CONF_SUBDIRS = conf doc

.PHONY: all
all: $(MODULE_SUBDIR) $(TOOLS_SUBDIR) $(TEST_SUBDIR)

.PHONY: $(MODULE_SUBDIR)
$(MODULE_SUBDIR):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TEST_SUBDIR)
$(TEST_SUBDIR):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TOOLS_SUBDIR)
$(TOOLS_SUBDIR):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(CONF_SUBDIRS)
$(CONF_SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: clean
clean: $(MODULE_SUBDIR) $(TOOLS_SUBDIR) $(TEST_SUBDIR)

.PHONY: run-test
run-test: $(MODULE_SUBDIR) $(TOOLS_SUBDIR) $(TEST_SUBDIR)

.PHONY: install
install: $(MODULE_SUBDIR) $(TOOLS_SUBDIR) $(CONF_SUBDIRS)

.PHONY: install-strip
install-strip: MAKECMDGOALS=install
install-strip: install
	$(MAKE) -C $(TOOLS_SUBDIR) install-strip

.PHONY: uninstall
uninstall: $(MODULE_SUBDIR) $(TOOLS_SUBDIR) $(CONF_SUBDIRS)

.PHONY: dkms-install
dkms-install: $(MODULE_SUBDIR)

.PHONY: dkms-uninstall
dkms-uninstall: $(MODULE_SUBDIR)

.PHONY: tools
tools: $(TOOLS_SUBDIR)

.PHONY: clean-tools
clean-tools: $(TOOLS_SUBDIR)

.PHONY: tools-install
tools-install: $(TOOLS_SUBDIR)

.PHONY: tools-install-strip
tools-install-strip: $(TOOLS_SUBDIR)

.PHONY: tools-uninstall
tools-uninstall: $(TOOLS_SUBDIR)

.PHONY: tools-debug
tools-debug: $(TOOLS_SUBDIR)

.PHONY: tools-strip
tools-strip: $(TOOLS_SUBDIR)

.PHONY: debug
debug: tools-debug

