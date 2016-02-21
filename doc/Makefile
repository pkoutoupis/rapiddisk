CP := cp
RM := rm -f
MKDIR := mkdir -pv

.PHONY: all
all: 
	@echo Nothing to do.

.PHONY: install
install: all
	@echo Installing rapiddisk man page.
	$(MKDIR) $(DESTDIR)/usr/share/man/man1/
	$(CP) rapiddisk*.1 $(DESTDIR)/usr/share/man/man1/

.PHONY: uninstall
uninstall: 
	@echo Uninstalling rapiddisk man page.
	$(RM) $(DESTDIR)/usr/share/man/man1/rapiddisk*.1

.PHONY: clean
clean:
	@echo Nothing to do.
