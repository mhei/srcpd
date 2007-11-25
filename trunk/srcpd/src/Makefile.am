# Makefile.am for srcpd
# version: $Revision$
# last update: $Date$

sbin_PROGRAMS = srcpd

srcpd_SOURCES = \
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
	clientservice.c \
	portio.c \
	selectrix.c \
	srcpd.c \
	srcp-command.c \
	srcp-descr.c \
	srcp-error.c \
	srcp-fb.c \
	srcp-ga.c \
	srcp-gl.c \
	srcp-gm.c \
	srcp-info.c \
	srcp-lock.c \
	srcp-power.c \
	srcp-session.c \
	srcp-sm.c \
	srcp-server.c \
	srcp-time.c \
	netservice.c \
	ttycygwin.c \
	zimo.c

noinst_HEADERS = \
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
	clientservice.h \
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
	srcp-session.h \
	srcp-sm.h \
	srcp-server.h \
	srcp-time.h \
	stdincludes.h \
	netservice.h \
	ttycygwin.h \
	zimo.h

EXTRA_DIST = systemtest

# set the include path found by configure
INCLUDES = $(all_includes) $(XML_CPPFLAGS)

DEFS = -D_REENTRANT -DSYSCONFDIR=\"$(sysconfdir)\" @DEFS@

# the library search path.
srcpd_LDFLAGS = $(all_libraries) `./systemtest` $(XML_LIBS)

