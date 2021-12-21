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
