VERSION = 3.5

ifeq ($(KSRC),)
	KSRC := /lib/modules/$(shell uname -r)/build
endif

ifeq ($(KVER),)
        KVER := $(shell uname -r)
endif

obj-m += rxdsk.o
obj-m += rxcache.o

all:
	KCPPFLAGS="-DNOTMAINLINE" $(MAKE) -C$(KSRC) M=$(PWD)

install:
	install -o root -g root -m 0755 rxdsk.ko $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/
	install -o root -g root -m 0755 rxcache.ko $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/
	depmod -a

uninstall:
	rm -f $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/rxdsk.ko
	rm -f $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/rxcache.ko
	depmod -a

clean:
	rm -rf *.o *.ko *.symvers *.mod.c .*.cmd Module.markers modules.order .tmp_versions .rxdsk.o.d *.unsigned

.PHONY: dkms
dkms:
	sudo mkdir -pv /usr/src/rapiddisk-$(VERSION)/
	sudo cp -v * /usr/src/rapiddisk-$(VERSION)/
	sudo dkms add -m rapiddisk -v $(VERSION)
	sudo dkms build -m rapiddisk -v $(VERSION)
	sudo dkms install -m rapiddisk -v $(VERSION)
