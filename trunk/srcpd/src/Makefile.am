# Makefile.am for srcpd
# version: $Revision$
# last update: $Date$

sbin_PROGRAMS = srcpd

srcpd_SOURCES = \
	clientservice.c \
	config-srcpd.c \
	ddl.c \
	ddl_maerklin.c \
	ddl_nmra.c \
	ddl-s88.c \
	hsi-88.c \
	i2c-dev.c \
	ib.c \
	io.c \
	li100-main.c \
	loconet.c \
	loopback.c \
	m605x.c \
	netservice.c \
	portio.c \
	selectrix.c \
	srcp-command.c \
	srcpd.c \
	srcp-descr.c \
	srcp-error.c \
	srcp-fb.c \
	srcp-ga.c \
	srcp-gl.c \
	srcp-gm.c \
	srcp-info.c \
	srcp-lock.c \
	srcp-power.c \
	srcp-server.c \
	srcp-session.c \
	srcp-sm.c \
	srcp-time.c \
	syslogmessage.c \
	ttycygwin.c \
	zimo.c

noinst_HEADERS = \
	clientservice.h \
	config-srcpd.h \
	ddl.h \
	ddl_maerklin.h \
	ddl_nmra.h \
	ddl-s88.h \
	hsi-88.h \
	i2c-dev.h \
	ib.h \
	io.h \
	li100.c \
	li100.h \
       	loconet.h \
	loopback.h \
	m605x.h \
	netservice.h \
	portio.h \
	selectrix.h \
	srcp-command.h \
	srcp-descr.h \
	srcp-error.h \
	srcp-fb.h \
       	srcp-ga.h \
	srcp-gl.h \
	srcp-gm.h \
	srcp-info.h \
	srcp-lock.h \
	srcp-power.h \
	srcp-server.h \
	srcp-session.h \
	srcp-sm.h \
	srcp-time.h \
	syslogmessage.h \
	ttycygwin.h \
	zimo.h


DEFS = -D_REENTRANT -DSYSCONFDIR=\"$(sysconfdir)\" @DEFS@

