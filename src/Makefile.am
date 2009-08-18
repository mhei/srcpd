# Makefile.am for srcpd
# version: $Revision$
# last update: $Date$

sbin_PROGRAMS = srcpd

srcpd_SOURCES = \
	clientservice.c \
	config-srcpd.c \
	dcc-address.c \
	io.c \
	loopback.c \
	netservice.c \
	portio.c \
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
	ttycygwin.c 

noinst_HEADERS = \
	clientservice.h \
	config-srcpd.h \
	dcc-address.h \
	io.h \
	loopback.h \
	netservice.h \
	portio.h \
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
	ttycygwin.h

if USE_LOCONET
srcpd_SOURCES += loconet.c loconet.h
endif

if USE_DDL
srcpd_SOURCES +=ddl.c ddl_maerklin.c ddl_nmra.c   ddl.h ddl_maerklin.h ddl_nmra.h
endif

if USE_HSI88
srcpd_SOURCES += hsi-88.c hsi-88.h
endif

if USE_DDL88
srcpd_SOURCES += ddl-s88.c ddl-s88.h
endif

# tricky: li100-main includes li100.c twice for both USB and RS232 support
if USE_LENZ
srcpd_SOURCES += li100-main.c li100.h
endif

if USE_SELECTRIX
srcpd_SOURCES +=selectrix.c selectrix.h
endif

if USE_M605X
srcpd_SOURCES +=m605x.c m605x.h
endif

if USE_ZIMO
srcpd_SOURCES +=zimo.c zimo.h
endif

if USE_IB
srcpd_SOURCES +=ib.c ib.h
endif

if USE_I2C
srcpd_SOURCES +=i2c-dev.c i2c-dev.h
endif


DEFS = -D_REENTRANT -DSYSCONFDIR=\"$(sysconfdir)\" @DEFS@

