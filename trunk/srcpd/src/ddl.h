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

#include <libxml/tree.h> /*xmlDocPtr, xmlNodePtr*/

#include "config-srcpd.h"
#include "netservice.h"

#define QSIZE       2000
#define PKTSIZE     40
#define MAXDATA     20

typedef struct _tQData {
    int  packet_type;
    int  packet_size;
    char packet[PKTSIZE];
    int  addr;
} tQData ;

#define MAX_MARKLIN_ADDRESS 256
#define MAX_NMRA_ADDRESS 10367 /* idle-addr + 127 basic addr's + 10239 long's */

/* data types for maerklin packet pool */
typedef struct _tMaerklinPacket {
   char      packet[18];
   char      f_packets[4][18];
} tMaerklinPacket;

typedef struct _tMaerklinPacketPool {
   tMaerklinPacket packets[MAX_MARKLIN_ADDRESS+1];
   int             knownAddresses[MAX_MARKLIN_ADDRESS+1];
   int             NrOfKnownAddresses;
} tMaerklinPacketPool;

/* data types for NMRA packet pool */
typedef struct _tNMRAPacket {
   char      packet[PKTSIZE];
   int       packet_size;
   char      fx_packet[PKTSIZE];
   int       fx_packet_size;
} tNMRAPacket;

typedef struct _tNMRAPacketPool {
   tNMRAPacket     packets[MAX_NMRA_ADDRESS+1];
   int             knownAddresses[MAX_NMRA_ADDRESS+1];
   int             NrOfKnownAddresses;
} tNMRAPacketPool;

typedef struct _DDL_DATA {
    int number_gl;
    int number_ga;

    int        SERIAL_DEVICE_MODE;
    int        RI_CHECK;                  /* ring indicator checking      */
    int        CHECKSHORT;                /* default no shortcut checking */
    int        DSR_INVERSE;               /* controls how DSR is used to  */
                                          /* check shorts                 */
    time_t     SHORTCUTDELAY;             /* usecs shortcut delay         */
    int        NMRADCC_TR_V;              /* version of the nmra dcc      */
                                          /* translation routine (1 or 2) */
    int        ENABLED_PROTOCOLS;         /* enabled p's                  */
    int        IMPROVE_NMRADCC_TIMING;    /* NMRA DCC: improve timing     */

    int        WAITUART_USLEEP_PATCH;     /* enable/disbable usleep patch */
    int        WAITUART_USLEEP_USEC;      /* usecs for usleep patch       */

    struct termios maerklin_dev_termios;
    struct termios nmra_dev_termios;

#if linux
    struct serial_struct *serinfo_marklin;
    struct serial_struct *serinfo_nmradcc;
#endif
    pthread_mutex_t queue_mutex;   /* mutex to synchronize queue inserts */
    int queue_initialized;
    int queue_out, queue_in;
    tQData QData[QSIZE];

    time_t short_detected;

    pthread_t refresh_ptid;
    struct _thr_param refresh_param;

    unsigned char idle_data[MAXDATA];
    char NMRA_idle_data[PKTSIZE];

    int last_refreshed_maerklin_loco;
    int last_refreshed_maerklin_fx;
    int last_refreshed_nmra_loco;
    int last_refreshed_nmra_fx;
    int maerklin_refresh;

    struct timespec rmtp;   /* Do we really nead rmtp? */

    pthread_mutex_t nmra_pktpool_mutex;
    tNMRAPacketPool NMRAPacketPool;

    pthread_mutex_t maerklin_pktpool_mutex;
    tMaerklinPacketPool MaerklinPacketPool;
    int oslevel; /* 0: ancient linux 2.4, 1 linux 2.6 */

} DDL_DATA;

int readconfig_DDL(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber);
int init_lineDDL(bus_t busnumber);
int init_bus_DDL(bus_t busnumber);
int init_gl_DDL(gl_state_t *);
int init_ga_DDL(ga_state_t *);
int getDescription_DDL(char *reply);
void* thr_sendrec_DDL(void *);

#define  EP_MAERKLIN  1                  /* maerklin protocol */
#define  EP_NMRADCC   2                  /* nmra dcc protocol */

/* constants for setSerialMode() */
#define SDM_NOTINITIALIZED -1
#define SDM_DEFAULT         1  /* must be one of the following values */
#define SDM_MAERKLIN        0
#define SDM_NMRA            1
int setSerialMode(bus_t busnumber, int mode);

#define QEMPTY      -1
#define QNOVALIDPKT 0
#define QM1LOCOPKT  1
#define QM2LOCOPKT  2
#define QM2FXPKT    3
#define QM1FUNCPKT  4
#define QM1SOLEPKT  5
#define QNBLOCOPKT  6
#define QNBACCPKT   7

void queue_init(bus_t busnumber);
int  queue_empty(bus_t busnumber);
void queue_add(bus_t busnumber, int addr, char * const packet, int packet_type, int packet_size);
int  queue_get(bus_t busnumber, int *addr, char *packet, int *packet_size);

#define ADDR14BIT_OFFSET 128   /* internal offset of the long addresses */


void init_MaerklinPacketPool(bus_t busnumber);
char *get_maerklin_packet(bus_t busnumber, int adr, int fx);
void update_MaerklinPacketPool(bus_t busnumber, int adr,
        char const * const sd_packet, char const * const f1,
        char const * const f2, char const * const f3, char const * const f4);
void init_NMRAPacketPool(bus_t busnumber);
void update_NMRAPacketPool(bus_t busnumber, int adr,
        char const * const packet, int packet_size,
        char const * const fx_packet, int fx_packet_size);

void (*waitUARTempty)(bus_t busnumber);
int checkRingIndicator(bus_t busnumber);
int checkShortcut(bus_t busnumber);
void send_packet(bus_t busnumber, int addr, char *packet,
        int packet_size, int packet_type, int refresh);
void improve_nmradcc_write(bus_t busnumber, char *packet, int packet_size);
void refresh_loco(bus_t busnumber);
long int compute_delta(struct timeval tv1, struct timeval tv2);
void set_SerialLine(bus_t busnumber, int line, int mode);
int check_lines(bus_t busnumber);
void set_lines_off(bus_t busnumber);
void *thr_refresh_cycle(void *v);

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
