ifeq ($(CC),)
	CC := gcc -Werror
endif

BIN = rxflush rxio rxioctl

.PHONY: all
all: $(BIN)
	@echo Make has completed.

.PHONY: install
install: all
	@echo Nothing to do.

.PHONY: uninstall
uninstall:
	@echo Nothing to do.

rxflush: rxflush.c
	$(CC) rxflush.c -o rxflush

rxio: rxio.c
	$(CC) rxio.c -o rxio

rxioctl: rxioctl.c
	$(CC) rxioctl.c -o rxioctl

.PHONY: clean
clean:
	rm -f $(BIN)
