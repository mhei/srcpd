/* cvs: $Id$             */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _M605X_H_
#define _M605X_H_

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>

#include "config.h"

/* Folgende Werte lassen sich z.T. ver‰ndern, siehe Kommandozeilen/Configdatei */
/* unver‰nderliche sind mit const markiert                                     */

/* Anschluﬂport */
extern char* DEV_M6051;

extern int        NUMBER_FB;              /* Anzahl der Feebackmodule     */
extern int        M6020MODE;              /* die 6020 mag keine erweiterten Befehle */

/* das Programm arbeitet mit vielen Threads. und in der C-Fibel steht was
 * von volatile, schadet vermutlich nicht.
 */
extern int io_thread_running; /* eine Art Wachhund, siehe main()     */
extern int server_shutdown_state; /* wird gesetzt und beendet den Server */
extern int server_reset_state; /* Reset des Servers, unimplemented    */
extern int power_state; /* liegt Spannung am GLeis?            */

#endif
