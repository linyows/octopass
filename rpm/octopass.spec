Summary:          Management linux user and authentication with the organization/team on Github.
Name:             octopass
Version:          0.1.0
Release:          1
License:          GPLv3
URL:              https://github.com/linyows/octopass
Source0:          %{name}-%{version}.tar.gz
Source1:          https://github.com/linyows/octopass/releases/download/v%{version}/linux_amd64.zip#/go-octopass-%{version}.zip#/go-%{name}-%{version}.zip
Group:            System Environment/Base
Packager:         linyows <linyows@gmail.com>
Requires:         glibc libcurl-devel jansson-devel
BuildRoot:        %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:    gcc make
BuildArch:        i386, x86_64

%define debug_package %{nil}

%description
This is linux user management tool with the organization/team on github, and authentication.
Depending on github for user management, there are certain risks,
but features easy handling and ease of operation.

%prep
%setup -q -n %{name}-%{version}

%build
make
unzip %{SOURCE1}

%install
%{__rm} -rf %{buildroot}
mkdir -p %{buildroot}/usr/{lib64,bin}
mkdir -p %{buildroot}%{_sysconfdir}
make PREFIX=%{buildroot}/usr install
install -d -m 755 %{buildroot}/var/cache/octopass
install -m 755 octopass %{buildroot}/usr/bin/octopass
install -m 644 octopass.conf.example %{buildroot}%{_sysconfdir}/octopass.conf.example

%clean
%{__rm} -rf %{buildroot}

%post

%preun

%postun

%files
%defattr(-, root, root)
/usr/lib64/libnss_octopass.so
/usr/lib64/libnss_octopass.so.2
/usr/lib64/libnss_octopass.so.2.0
/usr/bin/nss-octopass
/usr/bin/octopass
/var/cache/octopass
/etc/octopass.conf.example

%changelog
* Fri Feb 3 2017 linyows <linyows@gmail.com> - 1:0.1.0-1
- hello world!
