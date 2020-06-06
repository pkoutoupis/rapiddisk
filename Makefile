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
