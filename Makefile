CC=gcc
CFLAGS=-Wall -Wstrict-prototypes -Werror -fPIC -std=c99 -D_GNU_SOURCE
LD_SONAME=-Wl,-soname,libnss_octopass.so.2
LIBRARY=libnss_octopass.so.2.0
LINKS=libnss_octopass.so.2 libnss_octopass.so

PREFIX=/usr
#LIBDIR=$(PREFIX)/lib64
LIBDIR=$(PREFIX)/lib/x86_64-linux-gnu
ifeq ($(wildcard $(LIBDIR)/.*),)
LIBDIR=$(PREFIX)/lib
endif
BINDIR=$(PREFIX)/bin
BUILD=tmp/libs
CACHE=/var/cache/octopass

DIST ?= $(shell grep VERSION_CODENAME /etc/os-release | awk -F '=' '{print $$2}' ORS='')
SOURCES_RPM=Makefile octopass.h octopass*.c nss_octopass*.c octopass.conf.example COPYING selinux/octopass.pp
SOURCES=Makefile octopass.h octopass*.c nss_octopass*.c octopass.conf.example COPYING
VERSION=$(shell awk -F\" '/^\#define OCTOPASS_VERSION / { print $$2; exit }' octopass.h)
CRITERION_VERSION=2.4.2
JANSSON_VERSION=2.4

INFO_COLOR=\033[1;34m
RESET=\033[0m
BOLD=\033[1m

ifeq ("$(shell ls .env)",".env")
include .env
export $(shell sed 's/=.*//' .env)
endif

default: build
build: nss_octopass octopass_cli

build_dir: ## Create directory for build
	test -d $(BUILD) || mkdir -p $(BUILD)

cache_dir: ## Create directory for cache
	test -d $(CACHE) || mkdir -p $(CACHE) && chmod 777 $(CACHE)

deps: ## Install dependencies
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Installing Dependencies$(RESET)"
	git clone --depth=1 https://github.com/vstakhov/libucl.git tmp/libucl
	pushd tmp/libucl; ./autogen.sh; ./configure && make && make install; popd
	git clone --depth=1 https://github.com/allanjude/uclcmd.git tmp/uclcmd

criterion: ## Installing criterion
	test -f $(BUILD)/criterion.tar.bz2 || curl -sL https://github.com/Snaipe/Criterion/releases/download/v$(CRITERION_VERSION)/criterion-v$(CRITERION_VERSION)-linux-x86_64.tar.bz2 -o $(BUILD)/criterion.tar.bz2
	cd $(BUILD); tar xf criterion.tar.bz2; cd ../
	mv $(BUILD)/criterion-v$(CRITERION_VERSION)/include/criterion $(PREFIX)/include/criterion
	mv $(BUILD)/criterion-v$(CRITERION_VERSION)/lib/libcriterion.* $(LIBDIR)/

nss_octopass: build_dir cache_dir ## Build nss_octopass
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Building nss_octopass$(RESET)"
	$(CC) $(CFLAGS) -c nss_octopass-passwd.c -o $(BUILD)/nss_octopass-passwd.o
	$(CC) $(CFLAGS) -c nss_octopass-group.c -o $(BUILD)/nss_octopass-group.o
	$(CC) $(CFLAGS) -c nss_octopass-shadow.c -o $(BUILD)/nss_octopass-shadow.o
	$(CC) $(CFLAGS) -c octopass.c -o $(BUILD)/octopass.o
	$(CC) -shared $(LD_SONAME) -o $(BUILD)/$(LIBRARY) \
		$(BUILD)/octopass.o \
		$(BUILD)/nss_octopass-passwd.o \
		$(BUILD)/nss_octopass-group.o \
		$(BUILD)/nss_octopass-shadow.o \
		-lcurl -ljansson

octopass_cli: build_dir cache_dir ## Build octopass cli
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Building octopass cli$(RESET)"
	$(CC) $(CFLAGS) -c nss_octopass-passwd_cli.c -o $(BUILD)/nss_octopass-passwd_cli.o
	$(CC) $(CFLAGS) -c nss_octopass-group_cli.c -o $(BUILD)/nss_octopass-group_cli.o
	$(CC) $(CFLAGS) -c nss_octopass-shadow_cli.c -o $(BUILD)/nss_octopass-shadow_cli.o
	$(CC) $(CFLAGS) -c octopass_cli.c -o $(BUILD)/octopass_cli.o
	$(CC) -o $(BUILD)/octopass \
		$(BUILD)/octopass_cli.o \
		$(BUILD)/nss_octopass-passwd_cli.o \
		$(BUILD)/nss_octopass-group_cli.o \
		$(BUILD)/nss_octopass-shadow_cli.o \
		-lcurl -ljansson

test: build_dir cache_dir ## Run unit testing
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Testing$(RESET)"
	test -d $(PREFIX)/include/criterion || $(MAKE) criterion
	$(CC) octopass_test.c \
		nss_octopass-passwd_test.c \
		nss_octopass-group_test.c \
		nss_octopass-shadow_test.c -lcurl -ljansson -lcriterion -o $(BUILD)/test && \
		$(BUILD)/test --verbose

integration: build install ## Run integration test
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Integration Testing$(RESET)"
	#test -d /usr/lib/x86_64-linux-gnu && ln -sf /usr/lib/libnss_octopass.so.2.0 /usr/lib/x86_64-linux-gnu/libnss_octopass.so.2.0 || true
	cp octopass.conf.example /etc/octopass.conf
	sed -i -e 's/^passwd:.*/passwd: files octopass/g' /etc/nsswitch.conf
	sed -i -e 's/^shadow:.*/shadow: files octopass/g' /etc/nsswitch.conf
	sed -i -e 's/^group:.*/group: files octopass/g' /etc/nsswitch.conf
	test/integration.sh

format: ## Format with clang-format
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Formatting$(RESET)"
	for f in $(ls *.{c,h}); do\
		clang-format -i $f;\
	done
	test -z "$$(git status -s -uno)"

install: install_lib install_cli ## Install octopass

install_lib: ## Install only shared objects
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Installing as Libraries$(RESET)"
	[ -d $(LIBDIR) ] || install -d $(LIBDIR)
	install $(BUILD)/$(LIBRARY) $(LIBDIR)
	cd $(LIBDIR); for link in $(LINKS); do ln -sf $(LIBRARY) $$link ; done;

install_cli: ## Install only cli command
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Installing as Command$(RESET)"
	cp $(BUILD)/octopass $(BINDIR)/octopass

source_for_rpm: ## Create source for RPM
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Distributing$(RESET)"
	rm -rf tmp.$(DIST) octopass-$(VERSION).tar.gz
	mkdir -p tmp.$(DIST)/octopass-$(VERSION)
	$(MAKE) selinux_policy
	cp $(SOURCES_RPM) tmp.$(DIST)/octopass-$(VERSION)
	cd tmp.$(DIST) && \
		tar cf octopass-$(VERSION).tar octopass-$(VERSION) && \
		gzip -9 octopass-$(VERSION).tar
	cp tmp.$(DIST)/octopass-$(VERSION).tar.gz ./builds
	rm -rf tmp.$(DIST)

selinux_policy: ## Build policy file for SELinux
	make -C selinux -f /usr/share/selinux/devel/Makefile

rpm: source_for_rpm ## Packaging for RPM
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Packaging for RPM$(RESET)"
	cp builds/octopass-$(VERSION).tar.gz /root/rpmbuild/SOURCES
	spectool -g -R rpm/octopass.spec
	rpmbuild -ba rpm/octopass.spec
	cp /root/rpmbuild/RPMS/*/*.rpm /octopass/builds

jansson: build_dir ## Build and Install Janson
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Building and Installing Jansson$(RESET)"
	mkdir -p /usr/src/redhat/SOURCES
	test -f $(BUILD)/jansson.spec || curl -sLk https://raw.github.com/nmilford/rpm-jansson/master/jansson.spec -o $(BUILD)/jansson.spec
	test -f /usr/src/redhat/SOURCES/jansson-$(JANSSON_VERSION).tar.gz || curl -sLk http://www.digip.org/jansson/releases/jansson-$(JANSSON_VERSION).tar.gz -o /usr/src/redhat/SOURCES/jansson-$(JANSSON_VERSION).tar.gz
	rpmbuild -bb $(BUILD)/jansson.spec
	rm -rf /usr/src/redhat/RPMS/*/jansson-*.rpm

source_for_deb: ## Create source for DEB
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Distributing$(RESET)"
	rm -rf tmp.$(DIST) octopass_$(VERSION).orig.tar.xz
	mkdir -p tmp.$(DIST)/octopass-$(VERSION)
	cp $(SOURCES) tmp.$(DIST)/octopass-$(VERSION)
	cd tmp.$(DIST) && \
		tar cf octopass_$(VERSION).tar octopass-$(VERSION) && \
		xz -v octopass_$(VERSION).tar
	mv tmp.$(DIST)/octopass_$(VERSION).tar.xz tmp.$(DIST)/octopass_$(VERSION).orig.tar.xz

deb: source_for_deb ## Packaging for DEB
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Packaging for DEB$(RESET)"
	cd tmp.$(DIST) && \
		tar xf octopass_$(VERSION).orig.tar.xz && \
		cd octopass-$(VERSION) && \
		dh_make --single --createorig -y && \
		rm -rf debian/*.ex debian/*.EX debian/README.Debian && \
		cp -v ../../debian/* debian/ && \
		sed -i -e 's/DIST/$(DIST)/g' debian/changelog && \
		debuild -uc -us
	cd tmp.$(DIST) && \
		find . -name "*.deb" ! -name "*-dbgsym_*.deb" | sed -e 's/\(\(.*octopass_.*\).deb\)/mv \1 \2.$(DIST).deb/g' | sh && \
		cp *.deb ../builds
	rm -rf tmp.$(DIST)


github_release: pkg ## Upload archives to Github Release on Mac
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Releasing for Github$(RESET)"
	go get github.com/tcnksm/ghr
	rm -rf builds/.keep && ghr v$(VERSION) builds && git checkout builds/.keep

packagecloud_release: ## Upload archives to PackageCloud on Mac
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Releasing for PackageCloud$(RESET)"
	go get github.com/mlafeldt/pkgcloud/...
	pkgcloud-push linyows/octopass/el/8 builds/octopass-$(VERSION)-1.x86_64.el8.rpm
	pkgcloud-push linyows/octopass/el/7 builds/octopass-$(VERSION)-1.x86_64.el7.rpm
	#pkgcloud-push linyows/octopass/el/6 builds/octopass-$(VERSION)-1.x86_64.el6.rpm
	pkgcloud-push linyows/octopass/ubuntu/focal builds/octopass_$(VERSION)-1_amd64.focal.deb
	pkgcloud-push linyows/octopass/ubuntu/bionic builds/octopass_$(VERSION)-1_amd64.bionic.deb
	#pkgcloud-push linyows/octopass/ubuntu/xenial builds/octopass_$(VERSION)-1_amd64.xenial.deb
	#pkgcloud-push linyows/octopass/ubuntu/trusty builds/octopass_$(VERSION)-1_amd64.trusty.deb
	#pkgcloud-push linyows/octopass/ubuntu/precise builds/octopass_$(VERSION)-1_amd64.precise.deb
	pkgcloud-push linyows/octopass/debian/bullseye builds/octopass_$(VERSION)-1_amd64.bullseye.deb
	pkgcloud-push linyows/octopass/debian/buster builds/octopass_$(VERSION)-1_amd64.buster.deb
	pkgcloud-push linyows/octopass/debian/stretch builds/octopass_$(VERSION)-1_amd64.stretch.deb
	#pkgcloud-push linyows/octopass/debian/jessie builds/octopass_$(VERSION)-1_amd64.jessie.deb

pkg: ## Create some distribution packages
	rm -rf builds && mkdir builds
	docker-compose up

dist: ## Upload archives to Github Release on Mac
	@test -z $(GITHUB_TOKEN) || $(MAKE) github_release
	@test -z $(PACKAGECLOUD_TOKEN) || $(MAKE) packagecloud_release

clean: ## Delete tmp directory
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Cleaning$(RESET)"
	rm -rf $(TMP)

distclean: clean
	rm -f builds/octopass*

help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "$(INFO_COLOR)%-30s$(RESET) %s\n", $$1, $$2}'

.PHONY: help clean install build_dir cache_dir nss_octopass octopass_cli dist distclean deps test rpm
