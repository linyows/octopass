CC=gcc
CFLAGS=-Wall -Wstrict-prototypes -Werror -fPIC -std=c99 -D_GNU_SOURCE
LD_SONAME=-Wl,-soname,libnss_octopass.so.2
LIBRARY=libnss_octopass.so.2.0
LINKS=libnss_octopass.so.2 libnss_octopass.so

CFLAGS_TEST=-DNSS_OCTOPASS_SCRIPT=\"./nss-octopass\"
LIBS=-lpthread
CLI=nss-octopass

PREFIX=/usr
LIBDIR=$(PREFIX)/lib64
ifeq ($(wildcard $(LIBDIR)/.*),)
LIBDIR=$(PREFIX)/lib
endif
BINDIR=$(PREFIX)/sbin
BUILD=tmp/libs
CACHE=/tmp/nss_octopass-*

SOURCES=Makefile nss_octopass.c version libnss-octopass.spec
VERSION=$(shell cat version)
CRITERION_VERSION=2.3.0

default: build
build: nss_octopass nss_octopass_cli

build_dir:
	test -d $(BUILD) || mkdir -p $(BUILD)

nss_octopass-passwd:
	$(CC) $(CFLAGS) -c nss_octopass-passwd.c -o $(BUILD)/nss_octopass-passwd.o

nss_octopass-group:
	$(CC) $(CFLAGS) -c nss_octopass-group.c -o $(BUILD)/nss_octopass-group.o

nss_octopass-shadow:
	$(CC) $(CFLAGS) -c nss_octopass-shadow.c -o $(BUILD)/nss_octopass-shadow.o

nss_octopass_services: nss_octopass-passwd nss_octopass-group nss_octopass-shadow

nss_octopass: build_dir nss_octopass_services
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

deps:
	git clone --depth=1 https://github.com/vstakhov/libucl.git tmp/libucl
	pushd tmp/libucl; ./autogen.sh; ./configure && make && make install; popd
	git clone --depth=1 https://github.com/allanjude/uclcmd.git tmp/uclcmd

depsdev: build_dir
	test -f $(BUILD)/criterion.tar.bz2 || curl -sL https://github.com/Snaipe/Criterion/releases/download/v$(CRITERION_VERSION)/criterion-v$(CRITERION_VERSION)-linux-x86_64.tar.bz2 -o $(BUILD)/criterion.tar.bz2
	cd $(BUILD); tar xf criterion.tar.bz2; cd ../
	mv $(BUILD)/criterion-v$(CRITERION_VERSION)/include/criterion /usr/include/criterion
	mv $(BUILD)/criterion-v$(CRITERION_VERSION)/lib/libcriterion.* $(LIBDIR)/

test_without_depsdev:
	$(CC) nss_octopass_test.c \
		nss_octopass-passwd_test.c \
		nss_octopass-group_test.c \
		nss_octopass-shadow_test.c -lcurl -ljansson -lcriterion -o $(BUILD)/test && \
		$(BUILD)/test --verbose

test: depsdev test_without_depsdev

nss_octopass-passwd_cli:
	$(CC) $(CFLAGS) -c nss_octopass-passwd_cli.c -o $(BUILD)/nss_octopass-passwd_cli.o

nss_octopass-group_cli:
	$(CC) $(CFLAGS) -c nss_octopass-group_cli.c -o $(BUILD)/nss_octopass-group_cli.o

nss_octopass-shadow_cli:
	$(CC) $(CFLAGS) -c nss_octopass-shadow_cli.c -o $(BUILD)/nss_octopass-shadow_cli.o

nss_octopass_cli_services: nss_octopass-passwd_cli nss_octopass-group_cli nss_octopass-shadow_cli

nss_octopass_cli: build_dir nss_octopass_cli_services
	$(CC) $(CFLAGS) -c nss_octopass_cli.c -o $(BUILD)/nss_octopass_cli.o
	$(CC) -o $(BUILD)/$(CLI) \
		$(BUILD)/nss_octopass_cli.o \
		$(BUILD)/nss_octopass-passwd_cli.o \
		$(BUILD)/nss_octopass-group_cli.o \
		$(BUILD)/nss_octopass-shadow_cli.o \
		-lcurl -ljansson

cli: nss_octopass_cli

install: install_lib install_cli

install_lib:
	[ -d $(LIBDIR) ] || install -d $(LIBDIR)
	install $(BUILD)/$(LIBRARY) $(LIBDIR)
	cd $(LIBDIR); for link in $(LINKS); do ln -sf $(LIBRARY) $$link ; done

install_cli:
	cp $(BUILD)/$(CLI) $(BINDIR)/$(CLI)

clean:
	rm -rf $(TMP)
	rm -rf $(CACHE)

distclean: clean
	rm -f *~ \#*

.PHONY: clean install build_dir nss_octopass dist distclean deps depsdev test test_without_depsdev
