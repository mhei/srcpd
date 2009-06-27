# Makefile.am for srcpd
# version: $Revision$
# last update: $Date$

SUBDIRS = src man

EXTRA_DIST = AUTHORS COPYING TODO NEWS ChangeLog INSTALL DESIGN \
	README README.freebsd README.loconet README.selectrix \
	PROGRAMMING-HOWTO srcpd.lsm srcpd.spec srcpd.redhat.spec \
        init.d.suse init.d.redhat init.d.lsb

dist_sysconf_DATA = srcpd.conf

udevdir = $(sysconfdir)/udev/rules.d
dist_udev_DATA = 10-liusb.rules
