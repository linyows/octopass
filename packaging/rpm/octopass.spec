Summary:          Management linux user and authentication with team or collaborator on Github.
Name:             octopass
Version:          1.1.0
Release:          1%{?dist}
License:          MIT
URL:              https://github.com/linyows/octopass
Source:           %{name}-%{version}.tar.gz
Group:            System Environment/Base
Packager:         linyows <linyows@gmail.com>
Requires:         glibc
BuildRequires:    selinux-policy-devel
BuildRoot:        %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildArch:        x86_64 aarch64

%define debug_package %{nil}

%description
This is user management tool for linux by github (Zig implementation).
The name-resolves and authentication is provided the team or collaborator on
github. Features easy handling and ease of operation.
Zero external dependencies beyond libc.

%prep
%setup -q -n %{name}-%{version}

%build
# Build SELinux policy
make -C selinux -f /usr/share/selinux/devel/Makefile

%install
%{__rm} -rf %{buildroot}
mkdir -p %{buildroot}%{_libdir}
mkdir -p %{buildroot}%{_bindir}
mkdir -p %{buildroot}%{_sysconfdir}
mkdir -p %{buildroot}/var/cache/octopass
mkdir -p %{buildroot}%{_datadir}/selinux/packages/%{name}

# Detect architecture and copy binaries
%ifarch x86_64
install -m 755 zig-out/linux-amd64/octopass %{buildroot}%{_bindir}/
install -m 755 zig-out/linux-amd64/libnss_octopass.so.2.0.0 %{buildroot}%{_libdir}/
%endif
%ifarch aarch64
install -m 755 zig-out/linux-arm64/octopass %{buildroot}%{_bindir}/
install -m 755 zig-out/linux-arm64/libnss_octopass.so.2.0.0 %{buildroot}%{_libdir}/
%endif

ln -sf libnss_octopass.so.2.0.0 %{buildroot}%{_libdir}/libnss_octopass.so.2
ln -sf libnss_octopass.so.2.0.0 %{buildroot}%{_libdir}/libnss_octopass.so
install -m 644 octopass.conf.example %{buildroot}%{_sysconfdir}/octopass.conf.example
install -m 644 selinux/%{name}.pp %{buildroot}%{_datadir}/selinux/packages/%{name}/%{name}.pp

%clean
%{__rm} -rf %{buildroot}

%post
# Install SELinux policy
if [ "$1" -le "1" ]; then
  /usr/sbin/semodule -i %{_datadir}/selinux/packages/%{name}/%{name}.pp 2>/dev/null || :
  fixfiles -R octopass restore 2>/dev/null || :
fi

echo ""
echo "octopass has been installed."
echo ""
echo "To complete setup:"
echo "1. Edit /etc/octopass.conf with your GitHub token and organization"
echo "2. Add 'octopass' to /etc/nsswitch.conf:"
echo "   passwd: files octopass"
echo "   group:  files octopass"
echo "   shadow: files octopass"
echo "3. Configure SSH in /etc/ssh/sshd_config:"
echo "   AuthorizedKeysCommand /usr/bin/octopass %u"
echo "   AuthorizedKeysCommandUser root"
echo ""

%preun
# Remove SELinux policy on final removal
if [ "$1" -lt "1" ]; then
  /usr/sbin/semodule -r octopass 2>/dev/null || :
  fixfiles -R octopass restore 2>/dev/null || :
fi

%postun
# Reinstall SELinux policy on upgrade
if [ "$1" -ge "1" ]; then
  /usr/sbin/semodule -i %{_datadir}/selinux/packages/%{name}/%{name}.pp 2>/dev/null || :
fi

%files
%defattr(-, root, root)
%{_libdir}/libnss_octopass.so
%{_libdir}/libnss_octopass.so.2
%{_libdir}/libnss_octopass.so.2.0.0
%{_bindir}/octopass
%attr(0777,root,root) /var/cache/octopass
%config(noreplace) %{_sysconfdir}/octopass.conf.example
%{_datadir}/selinux/packages/%{name}/%{name}.pp

%changelog
* Sun Jan 26 2025 linyows <linyows@gmail.com> - 1.1.0-1
- Add HMAC-SHA256 signed shared cache
- Share cache across all users to improve cache hit rate
- Add signature verification to prevent cache tampering

* Sat Jan 25 2025 linyows <linyows@gmail.com> - 1.0.0-1
- Rewrite octopass from C to Zig
- Zero external dependencies (no libcurl, no libjansson)
- Memory safety with compile-time checks
- Easy cross-compilation for Linux (amd64/arm64)
- Environment variable support for configuration override
- SELinux policy included

* Thu Apr 27 2021 linyows <linyows@gmail.com> - 0.7.1-1
- Bugfixes

* Fri Jun 21 2019 linyows <linyows@gmail.com> - 0.7.0-1
- Resolve a problem of cache file permission

* Mon Oct 22 2018 linyows <linyows@gmail.com> - 0.6.0-1
- Add policy for SELinux

* Wed Oct 10 2018 linyows <linyows@gmail.com> - 0.5.1-1
- Fix for systemd-networkd SEGV

* Thu Oct 02 2018 linyows <linyows@gmail.com> - 0.5.0-1
- Support slug for GitHub team API

* Mon Apr 02 2018 linyows <linyows@gmail.com> - 0.4.1-1
- Page size changes to 100 from 30 on Github organization API

* Mon Sep 25 2017 linyows <linyows@gmail.com> - 0.4.0-1
- Support github repository collaborators as name resolve

* Thu Sep 14 2017 linyows <linyows@gmail.com> - 0.3.5-1
- Bugfixes

* Sun May 07 2017 linyows <linyows@gmail.com> - 0.3.3-1
- Fix segmentation fault

* Tue Feb 28 2017 linyows <linyows@gmail.com> - 0.3.2-1
- Example typo

* Mon Feb 27 2017 linyows <linyows@gmail.com> - 0.3.1-1
- Bugfixes

* Sun Feb 26 2017 linyows <linyows@gmail.com> - 0.3.0-1
- Support shared-users option

* Sun Feb 19 2017 linyows <linyows@gmail.com> - 0.2.0-1
- Change implementation in Go to C

* Fri Feb 03 2017 linyows <linyows@gmail.com> - 0.1.0-1
- Initial packaging
