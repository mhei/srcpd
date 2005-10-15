# Makefile.am for srcpd
# version: $Revision$
# last update: $Date$

sbin_PROGRAMS = srcpd

srcpd_SOURCES = config-srcpd.c config-srcpd.h \
	ddl-s88.c ddl-s88.h \
	ddl.c ddl.h \
	ddl_maerklin.c ddl_maerklin.h \
	ddl_nmra.c ddl_nmra.h \
	hsi-88.c hsi-88.h \
	i2c-dev.c i2c-dev.h \
	ib.c ib.h \
	io.c io.h \
	li100.c li100.h \
	loopback.c loopback.h \
	m605x.c m605x.h \
	netserver.c netserver.h \
	selectrix.c selectrix.h \
	srcp-descr.c srcp-descr.h \
	srcp-error.c srcp-error.h \
	srcp-fb.c srcp-fb.h \
	srcp-ga.c srcp-ga.h \
	srcp-gl.c srcp-gl.h \
	srcp-info.c srcp-info.h \
	srcp-lock.c srcp-lock.h \
	srcp-power.c srcp-power.h \
	srcp-session.c srcp-session.h \
	srcp-sm.c srcp-sm.h \
	srcp-srv.c srcp-srv.h \
	srcp-time.c srcp-time.h \
	srcpd.c stdincludes.h \
	threads.c threads.h \
        ttycygwin.c ttycygwin.h \
	zimo.c zimo.h

EXTRA_DIST = systemtest

# set the include path found by configure
INCLUDES = $(all_includes)

DEFS = -D_REENTRANT -DSYSCONFDIR=\"$(sysconfdir)\" -DLOCALEDIR=\"@LOCALEDIR@\" @DEFS@

LIBS = @LIBINTL@ @LIBS@

# the library search path.
srcpd_LDFLAGS = $(all_libraries) `./systemtest`
