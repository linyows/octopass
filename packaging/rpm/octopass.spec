Summary:          Management linux user and authentication with team or collaborator on Github.
Name:             octopass
Version:          0.10.0
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
* Fri Jan 24 2025 linyows <linyows@gmail.com> - 0.10.0-1
- Zig rewrite of octopass
- Zero external dependencies (no libcurl, no libjansson)
- Memory safety with compile-time checks
- Easy cross-compilation
- SELinux policy included
