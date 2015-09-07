ifeq ($(AR),)
        AR := ar
endif

ifeq ($(CC),)
	CC := gcc -Werror
endif

CHMOD := chmod
CP := cp
RM := rm
MKDIR := mkdir -p

BIN = rapiddisk
DIR := /sbin

.PHONY: all
all: rapiddisk
	@echo Make has completed.

.PHONY: install
install: all
	@echo Installing all binary files.
	$(MKDIR) $(DESTDIR)$(DIR)
	$(CP) $(BIN) $(DESTDIR)$(DIR)
	$(CHMOD) 755 $(DESTDIR)$(DIR)/$(BIN)*
	ln -s $(BIN) $(DESTDIR)/sbin/rxadm

.PHONY: uninstall
uninstall: 
	@echo Uninstalling rapiddisk files.
	$(RM) -f $(DESTDIR)$(DIR)/$(BIN)*
	$(RM) -f $(DESTDIR)$(DIR)/rxadm

core.o: core.c
	$(CC) -c core.c -o core.o

archive.o: archive.c
	$(CC) -c archive.c -o archive.o
 
crypt.o: crypt.c
	$(CC) -c crypt.c -o crypt.o

main.o: core.o archive.o crypt.o
	$(CC) main.c -o rapiddisk core.o archive.o crypt.o -lz -lcryptsetup

rapiddisk: main.o

.PHONY: clean
clean:
	rm -f *.o $(BIN)
