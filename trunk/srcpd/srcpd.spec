Summary: srcpd is a server daemon to control (some) digital model railroads
Name: srcpd
Version: 2.0
Release: 2
Source0: %{name}-%{version}-%{release}.tar.gz
License: GPL
Group: Games/Daemon
Packager: Matthias Trute <mtrute@users.sf.net>
Vendor: the srcpd team
URL: http://srcpd.sourceforge.net

%description

The srcpd is a server daemon that enables you to control and play with 
a digital model railroad using any SRCP Client. Actually it supports an 
Intellibox (tm), a Marklin Interface 6050 or 6051 (tm?), and many more
interfaces. 

More information about SRCP and links to many really cool clients (and 
other servers for different hardware) can be found at 
http://srcpd.sourceforge.net and http://www.der-moba.de/Digital

This is a beta release, do not use for production!
%prep
%setup -q
%build
make -f Makefile.dist
./configure --prefix=/usr --mandir=/usr/share/man --sysconfdir=/etc
make

%install
make install
cp rcsrcpd /etc/init.d/srcpd
chmod 744 /etc/init.d/srcpd
ln -sf ../../etc/init.d/srcpd /usr/sbin/rcsrcpd

%post
#
# Initialize runlevel links
#
if [ -x /usr/lib/lsb/install_initd ] ; then
    /usr/lib/lsb/install_initd /etc/init.d/srcpd
fi
%files
%defattr(-,root,root)
/usr/sbin/srcpd
/etc/init.d/srcpd
/usr/sbin/rcsrcpd
%config(noreplace) /etc/srcpd.conf
%doc COPYING AUTHORS README NEWS README.ibox README.freebsd

%preun
/etc/init.d/srcpd stop
if [ -x /usr/lib/lsb/install_initd ] ; then
    /usr/lib/lsb/remove_initd /etc/init.d/srcpd
fi
