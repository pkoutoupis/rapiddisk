CP := cp
RM := rm
MKDIR := mkdir -p
CHMOD := chmod
DIR := /etc/rapiddisk/

.PHONY: all
all: 
	@echo Nothing to do.

.PHONY: install
install: all
	@echo Installing rapiddisk cluster files.
	$(MKDIR) $(DESTDIR)$(DIR)
	$(CP) rapiddisk.* $(DESTDIR)$(DIR)
	$(CHMOD) -R 755 $(DESTDIR)$(DIR)

.PHONY: uninstall
uninstall: 
	@echo Uninstalling rapiddisk cluster files.
	$(RM) -f $(DESTDIR)$(DIR)

.PHONY: clean
clean:
	@echo Nothing to do.
