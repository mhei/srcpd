/* $Id$ */

#ifndef _DDL_H
#define _DDL_H
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <termios.h>
#include <signal.h>

#include <pthread.h>
#include <sched.h>

#if linux
#include <sys/io.h>
#include <linux/serial.h>
#endif

#include <libxml/tree.h>

#include "config-srcpd.h"


typedef struct _DDL_DATA {
    int number_gl;
    int number_ga;
    int SERIAL_DEVICE_MODE;
    int        RI_CHECK;              /* ring indicator checking      */
    int        CHECKSHORT;            /* default no shortcut checking */
    int        DSR_INVERSE;           /* controls how DSR is used to  */
                                          /* check shorts                 */
    time_t     SHORTCUTDELAY;             /* usecs shortcut delay         */
    int        NMRADCC_TR_V;              /* version of the nmra dcc      */
                                          /* translation routine (1 or 2) */
    int        ENABLED_PROTOCOLS; /* enabled p's */
    int        IMPROVE_NMRADCC_TIMING;    /* NMRA DCC: improve timing     */

    int        WAITUART_USLEEP_PATCH;     /* enable/disbable usleep patch */
    int        WAITUART_USLEEP_USEC;      /* usecs for usleep patch       */

    struct timeval tv1, tv2;
    struct timezone tz;
    pthread_t ptid;

} DDL_DATA;

int readconfig_DDL(xmlDocPtr doc, xmlNodePtr node, int busnumber);
int init_lineDDL(int );
int init_bus_DDL(int );
int term_bus_DDL(int );
int init_gl_DDL(struct _GLSTATE *);
int init_ga_DDL(struct _GASTATE *);
int getDescription_DDL(char *reply);
void* thr_sendrec_DDL(void *);

#define  EP_MAERKLIN  1                  /* maerklin protocol            */
#define  EP_NMRADCC   2                  /* nmra dcc protocol            */

/* constants for setSerialMode() */
#define SDM_NOTINITIALIZED -1
#define SDM_DEFAULT         1  /* must be one of the following values */
#define SDM_MAERKLIN        0
#define SDM_NMRA            1
int setSerialMode(int busnumber, int mode);

#define PKTSIZE     40

#define QEMPTY      -1
#define QNOVALIDPKT 0
#define QM1LOCOPKT  1
#define QM2LOCOPKT  2
#define QM2FXPKT    3
#define QM1FUNCPKT  4
#define QM1SOLEPKT  5
#define QNBLOCOPKT  6
#define QNBACCPKT   7

void queue_init();
void queue_add(int addr, char *packet, int packet_type, int packet_size);
int  queue_get(int *addr, char *packet, int *packet_size);

int monitor_NrOfQCmds();

#define MAX_MARKLIN_ADDRESS 256
#define MAX_NMRA_ADDRESS 10367 /* idle-addr + 127 basic addr's + 10239 long's */

#define ADDR14BIT_OFFSET 128   /* internal offset of the long addresses       */

typedef struct _tMaerklinPacket {
   char      packet[18];
   char      f_packets[4][18];
} tMaerklinPacket;

typedef struct _tMaerklinPacketPool {
   tMaerklinPacket packets[MAX_MARKLIN_ADDRESS+1];
   int             knownAdresses[MAX_MARKLIN_ADDRESS+1];
   int             NrOfKnownAdresses;
} tMaerklinPacketPool;


typedef struct _tNMRAPacket {
   char      packet[PKTSIZE];
   int       packet_size;
   char      fx_packet[PKTSIZE];
   int       fx_packet_size;
} tNMRAPacket;

typedef struct _tNMRAPacketPool {
   tNMRAPacket     packets[MAX_NMRA_ADDRESS+1];
   int             knownAdresses[MAX_NMRA_ADDRESS+1];
   int             NrOfKnownAdresses;
} tNMRAPacketPool;


void init_MaerklinPacketPool(int busnumber);
char *get_maerklin_packet(int bus, int adr, int fx);
void update_MaerklinPacketPool(int bus, int adr, char *sd_packet, char *f1, char *f2,
                                                         char *f3, char *f4);
void init_NMRAPacketPool(int busnumber);
void update_NMRAPacketPool(int bus, int adr, char *packet, int packet_size,
                                    char *fx_packet, int fx_packet_size);

void *thr_refresh_cycle(void *v);
void cancel_refresh_cycle(int bus);

void (*waitUARTempty)(int);
void set_SerialLine(int bus, int line, int mode);

/* serial line modes: */
#define ON  1
#define OFF 0

/* serial lines */
#define SL_RTS  4
#define SL_CTS  5
#define SL_DSR  6
#define SL_DTR 20
#define SL_RI  22

#endif
