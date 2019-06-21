Summary:          Management linux user and authentication with team or collaborator on Github.
Name:             octopass
Version:          0.7.0
Release:          1
License:          GPLv3
URL:              https://github.com/linyows/octopass
Source:           %{name}-%{version}.tar.gz
Group:            System Environment/Base
Packager:         linyows <linyows@gmail.com>
%if 0%{?rhel} < 6
Requires:         glibc curl-devel jansson-devel
%else
Requires:         glibc libcurl-devel jansson-devel
%endif
BuildRequires:    gcc, make, selinux-policy-devel
BuildRoot:        %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildArch:        i386, x86_64

%define debug_package %{nil}

%description
This is user management tool for linux by github.
The name-resloves and authentication is provided the team or collaborator on
github. Features easy handling and ease of operation.

%prep
%setup -q -n %{name}-%{version}

%build
make

%install
%{__rm} -rf %{buildroot}
mkdir -p %{buildroot}/usr/{lib64,bin}
mkdir -p %{buildroot}%{_sysconfdir}
make PREFIX=%{buildroot}/usr install
install -d -m 755 %{buildroot}/var/cache/octopass
install -m 644 octopass.conf.example %{buildroot}%{_sysconfdir}/octopass.conf.example
mkdir -p %{buildroot}%{_datadir}/selinux/packages/%{name}
install -m 644 %{name}.pp %{buildroot}%{_datadir}/selinux/packages/%{name}/%{name}.pp

%clean
%{__rm} -rf %{buildroot}

%post
# First install
if [ "$1" -le "1" ]; then
  /usr/sbin/semodule -i %{_datadir}/selinux/packages/%{name}/%{name}.pp 2>/dev/null || :
  fixfiles -R octopass restore
fi

%preun
# Final removal
if [ "$1" -lt "1" ]; then
  /usr/sbin/semodule -r octopass 2>/dev/null || :
  fixfiles -R octopass restore
fi

%postun
# Upgrade
if [ "$1" -ge "1" ]; then
  /usr/sbin/semodule -i %{_datadir}/selinux/packages/%{name}/%{name}.pp 2>/dev/null || :
fi

%files
%defattr(-, root, root)
/usr/lib64/libnss_octopass.so
/usr/lib64/libnss_octopass.so.2
/usr/lib64/libnss_octopass.so.2.0
/usr/bin/octopass
%attr(0777,root,root) /var/cache/octopass
/etc/octopass.conf.example
%{_datadir}/selinux/packages/%{name}/%{name}.pp

%changelog
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
