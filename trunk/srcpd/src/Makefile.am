# Makefile.am for srcpd
# version: $Revision$
# last update: $Date$

sbin_PROGRAMS = srcpd

srcpd_SOURCES = config-srcpd.c ddl-s88.c ddl.c ddl_maerklin.c ddl_nmra.c \
	hsi-88.c i2c-dev.c ib.c io.c portio.c loconet.c loopback.c m605x.c netserver.c \
	selectrix.c srcp-descr.c srcp-error.c srcp-fb.c srcp-ga.c srcp-gl.c srcp-info.c \
	srcp-lock.c srcp-power.c srcp-session.c srcp-sm.c srcp-srv.c srcp-time.c srcpd.c \
	threads.c ttycygwin.c zimo.c li100-main.c srcp-gm.c

EXTRA_DIST = systemtest

# set the include path found by configure
INCLUDES = $(all_includes) $(XML_CPPFLAGS)

DEFS = -D_REENTRANT -DSYSCONFDIR=\"$(sysconfdir)\" @DEFS@

# the library search path.
srcpd_LDFLAGS = $(all_libraries) `./systemtest` $(XML_LIBS)
noinst_HEADERS = config-srcpd.h ddl.h ddl_maerklin.h ddl-s88.h hsi-88.h\
	i2c-dev.h ib.h io.h li100.c li100.h loopback.h m605x.h netserver.h\
	srcp-descr.h srcp-error.h srcp-fb.h srcp-info.h srcp-lock.h srcp-power.h\
	srcp-session.h srcp-sm.h srcp-srv.h srcp-time.h stdincludes.h threads.h\
	ttycygwin.h zimo.h srcp-gm.h
