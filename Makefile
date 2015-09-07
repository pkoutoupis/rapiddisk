SUBDIRS = bin conf doc module www

.PHONY: all
all:
	for i in $(SUBDIRS); do cd $$i; make; cd ..; done

.PHONY: install
install:
	for i in $(SUBDIRS); do cd $$i; make install; cd ..; done

.PHONY: uninstall
uninstall:
	for i in $(SUBDIRS); do cd $$i; make uninstall; cd ..; done

.PHONY: clean
clean:
	for i in $(SUBDIRS); do cd $$i; make clean; cd ..; done

.PHONY: dkms
dkms:
	sudo mkdir -pv /usr/src/rapiddisk-3.2/
	sudo cp -v module/* /usr/src/rapiddisk-3.2/
	sudo dkms add -m rapiddisk -v 3.2
	sudo dkms build -m rapiddisk -v 3.2
	sudo dkms install -m rapiddisk -v 3.2

.PHONY: tools-install
tools-install:
	cd bin; make; make install; cd ../doc; make install; cd ../www; make install; cd ..

.PHONY: tools-uninstall
tools-uninstall:
	cd bin; make uninstall; cd ../doc; make uninstall; cd ../www; make install; cd ..
