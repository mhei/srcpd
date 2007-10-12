# Makefile.am for srcpd
# version: $Revision$
# last update: $Date$

SUBDIRS = src debian

EXTRA_DIST = AUTHORS COPYING TODO NEWS ChangeLog INSTALL DESIGN \
	README README.freebsd README.ibox README.selectrix \
	PROGRAMMING-HOWTO srcpd.lsm srcpd.spec srcpd.redhat.spec \
        init.d.suse init.d.redhat init.d.lsb

dist_sysconf_DATA = srcpd.conf

dist_man_MANS = srcpd.8

udevdir = $(sysconfdir)/udev/rules.d
udev_DATA = 10-liusb.rules