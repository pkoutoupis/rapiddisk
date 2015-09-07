CP := cp
RM := rm
MKDIR := mkdir -p
CHMOD := chmod

.PHONY: all
all: 
	@echo Nothing to do.

.PHONY: install
install: all
	@echo Installing the RapidDisk RESTful API.
	$(MKDIR) $(DESTDIR)/var/www/html/rapiddisk
	$(CP) -rf * $(DESTDIR)/var/www/html/rapiddisk/
	$(CHMOD) 755 -R $(DESTDIR)/var/www/html/rapiddisk/*

.PHONY: uninstall
uninstall: 
	@echo Uninstalling the RapidDisk RESTful API.
	$(RM) -rf $(DESTDIR)/var/www/html/rapiddisk/

.PHONY: clean
clean:
	@echo Nothing to do.
