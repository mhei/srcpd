Summary: srcpd is a server daemon to control (some) digital model railroads
Summary(de): srcpd ist ein SRCP-Server zur Steuerung von digitalen Modelleisenbahnen
Name: srcpd
Version: 2.0.7
Release: 1
Source0: %{name}-%{version}.tar.gz
License: GPL
Group: Games/Daemon
Packager: Matthias Trute <mtrute@users.sf.net>
Vendor: the srcpd team
URL: http://srcpd.sourceforge.net/
Prefix: /usr
Buildroot: %{_tmppath}/%{name}-%{version}-buildroot
Requires: libxml2 zlib

%description

The srcpd is a server daemon that enables you to control and play with 
a digital model railroad using any SRCP Client. Actually it supports an 
Intellibox (tm), a Maerklin Interface 6050 or 6051 (tm?), and many more
interfaces. 

More information about SRCP and links to many really cool clients (and 
other servers for different hardware) can be found at 
http://srcpd.sourceforge.net/ and http://www.der-moba.de/Digital/

This is a beta release, do not use for production!

%prep
%setup -q

%build
# make -f Makefile.dist
CFLAGS=$RPM_OPT_FLAGS \
./configure --prefix=%{_prefix} \
            --mandir=%{_mandir} \
	    --sysconfdir=%{_sysconfdir}
make

%install
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT%{_sbindir}
install -d $RPM_BUILD_ROOT%{_sysconfdir}/init.d
make DESTDIR=$RPM_BUILD_ROOT install
install -m 744 rcsrcpd $RPM_BUILD_ROOT%{_sysconfdir}/init.d/srcpd
ln -s %{_sysconfdir}/init.d/srcpd $RPM_BUILD_ROOT%{_sbindir}/rcsrcpd

%clean
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT

%post
# Initialize runlevel links
if [ -x /usr/lib/lsb/install_initd ] ; then
    /usr/lib/lsb/install_initd %{_sysconfdir}/init.d/srcpd
fi

%files
%defattr(-,root,root)
%{_sbindir}/srcpd
%{_sbindir}/rcsrcpd
%{_sysconfdir}/init.d/srcpd
%docdir %{_mandir}/man8/*
%{_mandir}/man8/*
%config(noreplace) %{_sysconfdir}/srcpd.conf
%doc COPYING AUTHORS README NEWS README.ibox README.freebsd PROGRAMMING-HOWTO

%preun
# remove runlevel links
%{_sysconfdir}/init.d/srcpd stop
if [ -x /usr/lib/lsb/install_initd ] ; then
    /usr/lib/lsb/remove_initd %{_sysconfdir}/init.d/srcpd
fi

%changelog
* Mon Nov 01 2004 Guido Scholz <guido.scholz@bayernline.de>
- Changed sysconfdir patch

* Mon Oct 18 2004 Guido Scholz <guido.scholz@bayernline.de>
- Update to srcpd-2.0-6

* Thu Jan 08 2004 Guido Scholz <guido.scholz@bayernline.de>
- adaptation to SuSE 9.0
