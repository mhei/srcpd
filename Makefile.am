####### kdevelop will overwrite this part!!! (begin)##########
sbin_PROGRAMS = srcpd
srcpd_SOURCES = config-srcpd.c srcpd.c srcp-fb-s88.c ib.c io.c \
		threads.c srcp-time.c srcp-srv.c srcp-power.c \
		srcp-gl.c srcp-ga.c srcp-fb.c srcp-fb-i8255.c \
		netserver.c m605x.c srcp-error.c srcp-descr.c \
		srcp-lock.c loopback.c

srcpd_LDADD   = -lpthread -lxml2


EXTRA_DIST = AUTHORS COPYING ChangeLog INSTALL README TODO srcpd.lsm \
	    srcpd.xml m605x.c m605x.h netserver.c netserver.h \
	    srcp-fb-i8255.c srcp-fb-i8255.h srcp-fb.c srcp-fb.h \
	    srcp-ga.c srcp-ga.h srcp-gl.c srcp-gl.h srcp-power.c \
	    srcp-power.h srcp-srv.c srcp-srv.h srcp-time.c srcp-time.h \
	    threads.c threads.h io.h io.c ib.c ib.h srcp-fb-s88.h \
	    srcp-fb-s88.c srcpd.c config-srcpd.c config-srcpd.h \
	    srcp-error.h srcp-error.h srcp-descr.c srcp-descr.h \
	    srcp-lock.c srcp-lock.h loopback.c loopback.h

####### kdevelop will overwrite this part!!! (end)############

# set the include path found by configure
INCLUDES= $(all_includes)

AM_CFLAGS = -D_REENTRANT

# the library search path.
srcpd_LDFLAGS = $(all_libraries) 
                                                                                
install-data-local:
	$(mkinstalldirs) $(prefix)/
	$(INSTALL_DATA) $(srcdir)/srcpd.xml $(sysconfdir)/
	$(INSTALL_DATA) $(srcdir)/srcpd.8 $(mandir)/man8

uninstall-local:
	-rm -f $(prefix)/etc/srcpd.xml

