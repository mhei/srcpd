Summary: srcpd is a server daemon to control (some) digital model railroads
Name: srcpd
Version: 2.0
Release: 1
Source0: %{name}-%{version}.tar.gz
License: GPL
Group: Games/Daemon
%description
The srcpd is a server daemon that enables you to control and play with 
a digital model railroad using any SRCP Client. Actually it requires an 
Intellibox (tm) or a Marklin Interface 6050 or 6051 (tm?). More 
information about SRCP and links to many really cool clients (and 
other servers for different hardware) can be found at 
http://srcpd.sourceforge.net and http://www.der-moba.de/Digital
%prep
%setup -q
%build
./configure --prefix=/usr --mandir=/usr/share/man
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
sbin/insserv etc/init.d/srcpd

%files
%defattr(-,root,root)
/usr/sbin/srcpd
/etc/init.d/srcpd
/usr/sbin/rcsrcpd
%config /usr/etc/srcpd.xml
/usr/share/man/man8/srcpd.8
%doc COPYING AUTHORS README NEWS

%preun
sbin/insserv -r etc/init.d/srcpd