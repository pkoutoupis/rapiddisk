SUBDIRS = src conf doc module test www

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
	cd module; make dkms; cd ..

.PHONY: tools-install
tools-install:
	cd src; make; make install; \
	cd ../conf; make install; \
	cd ../doc; make install; \
	cd ../www; make install; cd ..

.PHONY: tools-uninstall
tools-uninstall:
	cd src; make uninstall; \
	cd ../conf; make uninstall; \
	cd ../doc; make uninstall; \
	cd ../www; make install; cd ..

.PHONY: nocrypt
nocrypt:
	cd src; make nocrypt; \
	cd ../module; make; \
	cd ../test; make; cd ..

.PHONY: nocrypt-install
nocrypt-install:
	cd src; make nocrypt-install; \
	cd ../module; make install; \
	cd ../conf; make install; \
	cd ../doc; make install; \
	cd ../www; make install; cd ..
