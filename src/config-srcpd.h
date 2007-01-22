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

/***************************************************************************
 *                                                                         *
 *  Changes:                                                               *
 *    26.10.2006 Frank Schmischke                                          *
 *               split LI100 to LI100-SERIAL and LI100-USB                 *
 *                                                                         *
 *                                                                         *
 ***************************************************************************/
#ifndef _CONFIG_SRCPD_H
#define _CONFIG_SRCPD_H

#include <termios.h>
#include <sys/param.h>
#include <stdarg.h>

#include "srcp-gl.h"
#include "srcp-ga.h"

#define MAX_BUSSES             20         //! max number of integrated busses in srcpd
#define MAXSRCPLINELEN       1001         //! max number of bytes per line plus 0x00

#define SERVER_SERVER           0
#define SERVER_DDL              1         // srcpd-bus works as DDL-server
#define SERVER_M605X            2         // srcpd-bus works as M605X-server
#define SERVER_IB               3         // srcpd-bus works as IB-server
#define SERVER_LI100_SERIAL     4         // srcpd-bus works as Lenz-server with serial interface
#define SERVER_LI100_USB        5         // srcpd-bus works as Lenz-server with usb-interface
#define SERVER_LOOPBACK         6         // srcpd-bus is dummy driver, no real hardware
#define SERVER_S88              7         // srcpd-bus works as S88 at parallelport
#define SERVER_HSI_88           8         // srcpd-bus works as HSI88-server
#define SERVER_I2C_DEV          9         // srcpd-bus works as I2C-DEV-server
#define SERVER_ZIMO            10         // srcpd-bus works as Zimo MX1
#define SERVER_SELECTRIX       11         // srcpd-bus works as Selectrix-server
#define SERVER_LOCONET         12         // srcpd-bus works as Loconet Gateway

/* generic flags */
#define USE_WATCHDOG          0x0001      // use watchdog
#define AUTO_POWER_ON         0x0002      // start Power on startup
#define RESTORE_COM_SETTINGS  0x0004      // restore com-port settings after close

/* driver specific flags */
#define FB_ORDER_0            0x0200      // feedback port 0 is bit 0
#define FB_16_PORTS           0x0400      // feedback-modul has 16 ports
#define FB_4_PORTS            0x0800      // used for Lenz, sening 2x4 ports instead 8 at once

/* useful constants */
#define TRUE (1==1)
#define FALSE (1==0)


/* Busstruktur */
typedef struct _BUS
{
  int number;      //! busnumber
  int debuglevel;  //! verbosity level of syslog
  int type;        //! which bustype
  char description[100]; //! bus description

  char *device;    //! Path to device, if not null
  /** statistics */
  unsigned int bytes_recevied;
  unsigned int bytes_sent;
  unsigned int commands_processed;

  /* serial device parameters */
  speed_t baudrate; //!
  struct termios devicesettings; //! save Device Settings, if used

  /** Now internally used data */
  int fd;                                    //! file descriptor of device

  pthread_t pid;                             //! PID of the thread
  pthread_t pidtimer;                        //! PID of the timer thread
  void *thr_func;                            //! addr of the thread function
  void *thr_timer;                           //! addr of the timer thread
  void (*sig_reader) (long int);             //! addr of the reader (signal based)
  /* Definition of thread synchronisation                                              */
  pthread_mutex_t transmit_mutex;
  pthread_cond_t transmit_cond;

  long int (*init_func)(long int);           //! addr of init function
  long int (*term_func)(long int);           //! addr of init function
  long int (*init_gl_func) ( struct _GLSTATE *);  //! called to check default init
  long int (*init_ga_func) ( struct _GASTATE *);  //! called to check default init
  long int (*init_fb_func) (long int busnumber, int addr, const char protocolb, int index);  //! called to check default init

  int watchdog;                              //! used to monitor the thread
  /* Power management on the track */
  int power_state;
  int power_changed;
  struct timeval power_change_time;
  char power_msg[100];
  /* driver specific */
  void *driverdata;       //! pointer to driverspecific data
  int flags;              //! Watchdog flag
} BUS;

extern struct _BUS busses[];
extern int num_busses;

int readConfig(char *filename);

void suspendThread(long int busnumber);
void resumeThread(long int busnumber);

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
int bus_has_devicegroup(long int bus, int dg);

#define DBG_NONE 0
#define DBG_FATAL 1
#define DBG_ERROR 2
#define DBG_WARN 3
#define DBG_INFO 4
#define DBG_DEBUG 5

void DBG(long int busnumber, int dbglevel, const char *fmt, ...);
#endif
