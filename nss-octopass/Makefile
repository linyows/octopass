CC=gcc
CFLAGS=-Wall -Wstrict-prototypes -Werror -fPIC -std=c99 -D_GNU_SOURCE
LD_SONAME=-Wl,-soname,libnss_octopass.so.2
LIBRARY=libnss_octopass.so.2.0
LINKS=libnss_octopass.so.2 libnss_octopass.so

CFLAGS_TEST=-DNSS_OCTOPASS_SCRIPT=\"./nss-octopass\"
LIBS=-lpthread

DESTDIR=/
PREFIX=$(DESTDIR)/usr
LIBDIR=$(PREFIX)/lib
BUILD=.libs

SOURCES=Makefile nss_octopass.c version libnss-octopass.spec
VERSION=$(shell cat version)

default: build
build: nss_octopass

nss_octopass_build_dir:
	[ -d $(BUILD) ] || mkdir $(BUILD)

nss_octopass-passwd:
	$(CC) $(CFLAGS) -c nss_octopass-passwd.c -o $(BUILD)/nss_octopass-passwd.o

nss_octopass-group:
	$(CC) $(CFLAGS) -c nss_octopass-group.c -o $(BUILD)/nss_octopass-group.o

nss_octopass-shadow:
	$(CC) $(CFLAGS) -c nss_octopass-shadow.c -o $(BUILD)/nss_octopass-shadow.o

nss_octopass_services: nss_octopass-passwd nss_octopass-group nss_octopass-shadow

nss_octopass: nss_octopass_build_dir nss_octopass_services
	$(CC) $(CFLAGS) -c nss_octopass.c -o $(BUILD)/nss_octopass.o

	$(CC) -shared $(LD_SONAME) -o $(BUILD)/$(LIBRARY) \
		$(BUILD)/nss_octopass.o \
		$(BUILD)/nss_octopass-passwd.o \
		$(BUILD)/nss_octopass-group.o \
		$(BUILD)/nss_octopass-shadow.o \
        -lcurl -ljansson

nss_octopass_test: $(SOURCE) nss_octopass_test.c nss_octopass.h
	$(CC) $(CFLAGS) $(CFLAGS_TEST) $(LIBS) $(SOURCE) nss_octopass_test.c -o nss_octopass_test $(LIBS)
	strip nss_octopass_test

clean:
	rm -rf $(BUILD)

install:
	[ -d $(LIBDIR) ] || install -d $(LIBDIR)
	install $(BUILD)/$(LIBRARY) $(LIBDIR)
	cd $(LIBDIR); for link in $(LINKS); do ln -sf $(LIBRARY) $$link ; done

distclean: clean
	rm -f *~ \#*

dist:
	rm -rf libnss-octopass-$(VERSION) libnss-octopass-$(VERSION).tar libnss-octopass-$(VERSION).tar.gz
	mkdir libnss-octopass-$(VERSION)
	cp $(SOURCES) libnss-octopass-$(VERSION)
	tar cf libnss-octopass-$(VERSION).tar libnss-octopass-$(VERSION)
	gzip -9 libnss-octopass-$(VERSION).tar
rm -rf libnss-octopass-$(VERSION)

.PHONY: clean install nss_octopass_build_dir nss_octopass dist distclean
