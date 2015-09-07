Summary: The RapidDisk software defined advanced RAM drive and storage caching solution.
Name: rapiddisk
Version: 3.2
Release: 1
License: General Public License Version 2
Group: Applications/System
Source: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
Requires: gcc,make,kernel-headers,kernel-devel,dkms,cryptsetup-luks-libs,httpd,php
BuildRequires: kernel-headers,kernel-devel,gcc,make,cryptsetup-luks-devel,zlib-devel

%description
The RapidDisk software defined advanced RAM drive and storage
caching solution. This suite includes a collection of modules,
configuration files, and command line utilities, and a RESTful
API for managing RapidDisk enabled storage volumes.

%package utils
Summary: The RapidDisk administration utilities and API.
Group: Applications/System

%description utils
The RapidDisk software defined advanced RAM drive and storage
caching solution. This packages includes a collection of
configuration files, command line utilities, and a RESTful API
for managing RapidDisk enabled storage volumes.

%prep
%setup -T -D -n rxdsk

%build
make

%install
rm -rf %{buildroot}
cd bin
make DESTDIR=%{buildroot}/ install
cd ../doc
make DESTDIR=%{buildroot}/ install
cd ../conf
make DESTDIR=%{buildroot}/ install
cd ../module
mkdir -pv %{buildroot}/usr/src/rapiddisk-%{version}/
cp -v dkms.conf  %{buildroot}/usr/src/rapiddisk-%{version}/
cp -v {rxdsk,rxcache}.c  %{buildroot}/usr/src/rapiddisk-%{version}/
cp -v Makefile  %{buildroot}/usr/src/rapiddisk-%{version}/
cd ../www
make DESTDIR=%{buildroot}/ install

%post
dkms add -m rapiddisk -v %{version}
dkms build -m rapiddisk -v %{version}
dkms install -m rapiddisk -v %{version}
echo -ne "#!/bin/sh\nmodprobe rxdsk max_sectors=2048 nr_requests=1024 2>&1 >/dev/null" > /etc/sysconfig/modules/rxdsk.modules
echo -ne "\nmodprobe rxcache 2>&1 >/dev/null" >> /etc/sysconfig/modules/rxdsk.modules
echo -ne "#!/bin/sh\nmodprobe dm-crypt 2>&1 >/dev/null" > /etc/sysconfig/modules/dm-crypt.modules
chmod +x /etc/sysconfig/modules/{rxdsk,dm-crypt}.modules
sh /etc/sysconfig/modules/dm-crypt.modules
sh /etc/sysconfig/modules/rxdsk.modules

%preun
dkms remove -m rapiddisk -v %{version} --all

%postun
rm -f /lib/modules/$(uname -r)/{extra,updates,weak-updates}/{rxdsk,rxcache}.ko
rm -f /etc/sysconfig/modules/dm-crypt.modules
rm -f /etc/sysconfig/modules/rxdsk.modules

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
/etc/*
/sbin/*
/usr/src/rapiddisk-*
/var/www/html/*
%doc %attr(0444,root,root) /usr/share/man/man1/*

%files utils
%defattr(-,root,root)
/etc/*
/sbin/*
/var/www/html/*
%doc %attr(0444,root,root) /usr/share/man/man1/*

%changelog
* Tue Aug 13 2015 Petros Koutoupis <petros@petroskoutoupis.com>
- Adding a separate utils only build.
* Tue Jul 7 2015 Petros Koutoupis <petros@petroskoutoupis.com>
- Initial RPM package build.
