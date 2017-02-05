Summary:          Management linux user and authentication by therganization/teamn Github.
Name:             octopass
Version:          0.1.0
Release:          1
License:          GPLv3
URL:              https://github.com/linyows/octopass
Source:           %{name}-%{version}.tar.gz
Group:            System Environment/Base
Packager:         linyows <linyows@gmail.com>
Requires:         glibc libcurl-devel jansson-devel
BuildRoot:        %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:    gcc make
BuildArch:        i386, x86_64

%description
This is linux user management tool by the organization/team on github, and authentication.
Depending on github for user management, there are certain risks,
but features easy handling and ease of operation.

%prep
%setup -q -n %{name}-version-%{version}

%build
rm -rf $RPM_BUILD_ROOT
make build

%install
%{__rm} -rf %{buildroot}
make install
make LIBDIR="%{buildroot}%{_libdir}" install

%clean
make clean
%{__rm} -rf %{buildroot}

%post

%preun

%postun

%files
%doc LICENSE README.md
%{_sbindir}/*
%defattr(-, root, root)
%{_libdir}/*

%changelog
* Fri Feb 3 2017 linyows <linyows@gmail.com> - 1:0.1.0-1
- hello world!
