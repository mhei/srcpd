/***************************************************************************
                          config_srcpd.h  -  description
                             -------------------
    begin                : Wed Jul 4 2001
    copyright            : (C) 2001 by Dipl.-Ing. Frank Schmischke
    email                : frank.schmischke@t-online.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef _CONFIG_SRCPD_H
#define _CONFIG_SRCPD_H

#include <termios.h>
#include <sys/param.h>

#define MAX_BUSSES             20         // max number of integrated busses in srcpd

#define SERVER_SERVER           0
#define SERVER_DDL              1         // srcpd arbeitet als DDL-Server
#define SERVER_M605X            2         // srcpd arbeitet als M605X-Server
#define SERVER_IB               3         // srcpd arbeitet als IB-Server
#define SERVER_LI100            4         // srcpd arbeitet als Lenz-Server
#define SERVER_LOOPBACK         5         // dummy driver, no real hardware
#define SERVER_S88              6         // S88 am Parallelport
#define SERVER_HSI_88	          7

/* flags */
#define USE_WATCHDOG          0x0001      // use watchdog
#define M6020_MODE            0x0002      // Subtyp zum M605X
#define FB_ORDER_0            0x0010      // feedback port 0 is bit 0
#define FB_16_PORTS           0x0020      // feedback-modul has 16 ports
#define RESTORE_COM_SETTINGS  0x0100      // restore com-port settings after close

/* Busstruktur */
typedef struct _BUS
{
  int number;      // Nummer
  int debuglevel;  // testmodus
  int type;        // SERVER_IB, SERVER_M605X...

  char *device;    // Path_to_device
  speed_t baudrate;
    
  /* Now internally used data */
  int fd;          // file descriptor of device
  struct termios devicesettings; // Device Settings, if used
  pthread_t pid;   // PID of the thread
  void *thr_func;  // addr of the thread function
  int (*init_func)(int); // addr of init function
  int (*term_func)(int); // addr of init function
  int watchdog;    // used to monitor the thread
  int power_state;
  int power_changed;
  char power_msg[100];
  char description[100]; // bus description
  /* driver specific */
  void *driverdata;
  int flags;            // Watchdog
  int numberOfSM;       // maximumnumber for programing
} BUS;

extern struct _BUS busses[];
extern int num_busses;
extern char PIDFILE[MAXPATHLEN];

int readConfig(const char *filename);

#endif
