/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
/* $Id$ */
/*
 * Hauptinclude Datei fuer alle Sourcefiles
 *
 * V1.0 01/05/03 MAM
 *
 */

#ifndef _SRCP_MASTER_INCLUDE_H
#define  _SRCP_MASTER_INCLUDE_H

#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
	#include <string.h>
#else
	#ifdef HAVE_STRINGS_H
		#include <strings.h>
	#else
		#warning Your system does not support either string.h or strings.h
	#endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#else
#warning Your system does not support unistd.h 
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#else
#warning Your system does not have syslog.h, you may run into difficulties
#define openlog(a,b,c) 
#define syslog(a,b,(c)) fprintf(stderr,b,c)
#define closelog()
#endif

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#else
#error "This program heavenly depends on pthreads, you can't continue!"
#endif

#ifdef HAVE_SOCKET
#include <sys/socket.h>
#else 
#error "You don't have a socket function, that's really bad!"
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#else
#ifdef HAVE_SYS_TERMIO_H
#include <sys/termio.h>
#warning Your system only supports termio instead of termios. Prepare of a major rewrite of some parts of the code
#else
#error "Your system neither has termio or termios. You are on your own."
#endif
#endif

#include <stdarg.h>                                 
#include <sys/ioctl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <grp.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#else
#warning Your system does not have password support
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <sys/time.h>
#include <sys/wait.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#endif         /* _SRCP_MASTER_INCLUDE_H */
