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
#include <stdarg.h>

#define MAX_BUSSES             20         // max number of integrated busses in srcpd

#define SERVER_SERVER           0
#define SERVER_DDL              1         // srcpd arbeitet als DDL-Server
#define SERVER_M605X            2         // srcpd arbeitet als M605X-Server
#define SERVER_IB               3         // srcpd arbeitet als IB-Server
#define SERVER_LI100            4         // srcpd arbeitet als Lenz-Server
#define SERVER_LOOPBACK         5         // dummy driver, no real hardware
#define SERVER_S88              6         // S88 am Parallelport
#define SERVER_HSI_88	          7
#define SERVER_I2C_DEV          8         // srcpd arbeitet als I2C-DEV-Server
#define SERVER_ZIMO             9         // Zimo MX1

/* generic flags */
#define USE_WATCHDOG          0x0001      // use watchdog
#define AUTO_POWER_ON         0x0002      // start Power on startup
#define RESTORE_COM_SETTINGS  0x0004      // restore com-port settings after close

/* driver specific flags */
#define M6020_MODE            0x0100      // Subtyp zum M605X
#define FB_ORDER_0            0x0200      // feedback port 0 is bit 0
#define FB_16_PORTS           0x0400      // feedback-modul has 16 ports


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
  /* statistics */
  unsigned int bytes_recevied;
  unsigned int bytes_sent;
  unsigned int commands_processed;
  pthread_t pid;   // PID of the thread
  void *thr_func;  // addr of the thread function
  int (*init_func)(int); // addr of init function
  int (*term_func)(int); // addr of init function
  int watchdog;    // used to monitor the thread
  int power_state;
  int power_changed;
  struct timeval power_change_time;
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

#define DG_SESSION 1
#define DG_TIME 2
#define DG_GA 3
#define DG_GL 4
#define DG_FB 5
#define DG_SM 6
#define DG_LOCK 7
#define DG_DESCRIPTION 8
#define DG_SERVER 9
#define DG_POWER 10
int bus_has_devicegroup(int bus, int dg);

#define DGB_NONE 0
#define DBG_FATAL 1
#define DBG_ERROR 2
#define DBG_WARN 3
#define DBG_INFO 4
#define DBG_DEBUG 5


void DBG(int busnumber, int dbglevel, const char *fmt, ...);
#endif
