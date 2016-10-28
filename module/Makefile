VERSION = 4.4

ifeq ($(KSRC),)
	KSRC := /lib/modules/$(shell uname -r)/build
endif

ifeq ($(KVER),)
        KVER := $(shell uname -r)
endif

MKDIR := mkdir -pv

obj-m += rapiddisk.o
obj-m += rapiddisk-cache.o

all:
	KCPPFLAGS="-DNOTMAINLINE" $(MAKE) -C$(KSRC) M=$(PWD)

install:
	$(MKDIR) $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/
	install -o root -g root -m 0755 rapiddisk.ko $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/
	install -o root -g root -m 0755 rapiddisk-cache.ko $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/
	depmod -a

uninstall:
	rm -f $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/rapiddisk.ko
	rm -f $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/rapiddisk-cache.ko
	depmod -a

clean:
	rm -rf *.o *.ko *.symvers *.mod.c .*.cmd Module.markers modules.order
	rm -rf .tmp_versions .rapiddisk.o.d *.unsigned *.sdtinfo.c .ctf/

.PHONY: dkms
dkms:
	sudo mkdir -pv /usr/src/rapiddisk-$(VERSION)/
	sudo cp -v * /usr/src/rapiddisk-$(VERSION)/
	sudo dkms add -m rapiddisk -v $(VERSION)
	sudo dkms build -m rapiddisk -v $(VERSION)
	sudo dkms install -m rapiddisk -v $(VERSION)
