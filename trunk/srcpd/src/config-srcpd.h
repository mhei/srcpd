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

#define MAX_BUSSES        20        // Anzahl der im srcpd integrierten Busse

#define SERVER_DDL        1         // srcpd arbeitet als DDL-Server
#define SERVER_M605X      2         // srcpd arbeitet als M605X-Server
#define SERVER_IB         3         // srcpd arbeitet als IB-Server
#define SERVER_LI100      4         // srcpd arbeitet als Lenz-Server
#define SERVER_LOOPBACK   5	    // dummy driver, no real hardware

/* flags */
#define USE_WATCHDOG      1         // use watchdog
#define M6020_MODE        2         // Subtyp zum M605X

/* device flags */
#define RESTORE_COM_SETTINGS 1      //

/* Busstruktur */
typedef struct _BUS {
    int number;      // Nummer
    int debuglevel;  // testmodus
    int type;        // SERVER_IB, SERVER_M605X...
    int number_ga;   // 0 oder Anzahl
    int number_gl;   // 0 oder Anzahl
    int number_fb;   // 0 oder Anzahl
    int flags;       // M6020, Watchdog

    int deviceflags; // restore com port
    char *device;    // Path_to_device
    speed_t baud;
    
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
    int sending_ga;
    int command_ga;
    int sending_gl;
    int command_gl;
    int cmd32_pending;
} BUS;

extern struct _BUS busses[];
extern int num_busses;
extern char PIDFILE[MAXPATHLEN];

void readConfig(void);

#endif
