CP := cp
RM := rm -f
MKDIR := mkdir -pv -m 755
MAN_DIR := /usr/share/man/man1

.PHONY: all
all: 
	@echo Nothing to do.

.PHONY: install
install: all
	@echo Installing rapiddisk man page.
	$(MKDIR) $(DESTDIR)$(MAN_DIR)
	$(CP) rapiddisk*.1 $(DESTDIR)$(MAN_DIR)/

.PHONY: uninstall
uninstall: 
	@echo Uninstalling rapiddisk man page.
	$(RM) $(DESTDIR)$(MAN_DIR)/rapiddisk*.1

.PHONY: clean
clean:
	@echo Nothing to do.
