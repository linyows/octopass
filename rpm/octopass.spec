%define name      octopass
%define release   1
%define version   0.1.0
%define buildroot %{_tmppath}/%{name}-%{version}-%{release}-root

Name:             %{name}
Epoch:            1
Version:          %{version}
Release:          %{release}
Summary:          Management linux user and authentication by therganization/teamn Github.
License:          GPLv3+
URL:              https://github.com/linyows/octopass
Source0:          %{name}-%{version}.tar.gz
Group:            Development/Tools
BuildRoot:        %{buildroot}
BuildRequires:    gcc make
Requires:         glibc libcurl-devel jansson-devel

%description
This is linux user management tool by the organization/team on github, and authentication.
Depending on github for user management, there are certain risks,
but features easy handling and ease of operation.

%prep
%setup -q

%build
rm -rf $RPM_BUILD_ROOT
make build

%install
make install

%clean
make clean

%post

%preun

%postun

%files
%doc LICENSE README.md
%{_sbindir}/*
%defattr(-,root,root)

%changelog
* Fri Feb 3 2017 linyows <linyows@gmail.com> - 1:0.1.0-1
- hello world!
