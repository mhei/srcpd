init.ddir = /etc/init.d

sbin_PROGRAMS = srcpd
init.d_SCRIPTS = rcsrcpd
srcpd_SOURCES = config-srcpd.c srcpd.c ib.c io.c threads.c srcp-time.c \
		srcp-srv.c srcp-power.c srcp-gl.c srcp-ga.c srcp-fb.c \
		netserver.c m605x.c ddl-s88.c hsi-88.c li100.c loopback.c srcp-error.c \
		srcp-lock.c srcp-session.c srcp-descr.c srcp-info.c srcp-sm.c i2c-dev.c \
		zimo.c ddl.c ddl_maerklin.c ddl_nmra.c

EXTRA_DIST = AUTHORS COPYING ChangeLog INSTALL README TODO srcpd.lsm srcpd.conf m605x.c \
	    m605x.h netserver.c netserver.h srcp-fb.c stdincludes.h srcpd.8\
	    srcp-fb.h srcp-ga.c srcp-ga.h srcp-gl.c srcp-gl.h srcp-power.c srcp-power.h \
	    srcp-srv.c srcp-srv.h srcp-time.c srcp-time.h threads.c threads.h io.h io.c \
	    ib.c ib.h srcpd.c config-srcpd.c config-srcpd.h ddl-s88.c ddl-s88.h hsi-88.c \
	    hsi-88.h li100.c li100.h loopback.h loopback.c srcp-error.c srcp-error.h \
	    srcp-session.c srcp-descr.c srcp-session.h srcp-descr.h srcp-lock.c srcp-lock.h \
	    srcp-info.c srcp-info.h srcp-sm.c srcp-sm.h zimo.c zimo.h i2c-dev.c i2c-dev.h \
	    ddl.c ddl_maerklin.c ddl_nmra.c ddl.h ddl_maerklin.h ddl_nmra.h systemtest\
	    ibox/ibox.h ibox/ibox.c ibox/Makefile ibox/mkibox.sh ibox/README  rcsrcpd

# set the include path found by configure
INCLUDES = $(all_includes)
AM_CFLAGS = -D_REENTRANT
AM_CPPFLAGS = -DSYSCONFDIR=\"$(sysconfdir)\"
# the library search path.
srcpd_LDFLAGS = $(all_libraries) `./systemtest`
sysconf_DATA = srcpd.conf
man8_MANS = srcpd.8
