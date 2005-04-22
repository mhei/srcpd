/* $Id$ */

/* DDL:  Busdriver connected with a booster only without any special hardware.
 *
 */

#include "stdincludes.h"

#include "config-srcpd.h"
#include "io.h"
#include "ddl.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-srv.h"
#include "srcp-info.h"
#include "srcp-error.h"

#define __DDL ((DDL_DATA*)busses[busnumber].driverdata)

/* +----------------------------------------------------------------------+ */
/* | DDL - Digital Direct for Linux                                       | */
/* +----------------------------------------------------------------------+ */
/* | Copyright (c) 2002 - 2003 Vogt IT                                    | */
/* +----------------------------------------------------------------------+ */
/* | This source file is subject of the GNU general public license 2,     | */
/* | that is bundled with this package in the file COPYING, and is        | */
/* | available at through the world-wide-web at                           | */
/* | http://www.gnu.org/licenses/gpl.txt                                  | */
/* | If you did not receive a copy of the PHP license and are unable to   | */
/* | obtain it through the world-wide-web, please send a note to          | */
/* | gpl-license@vogt-it.com so we can mail you a copy immediately.       | */
/* +----------------------------------------------------------------------+ */
/* | Authors:   Torsten Vogt vogt@vogt-it.com                             | */
/* |                                                                      | */
/* +----------------------------------------------------------------------+ */

/***************************************************************/
/* erddcd - Electric Railroad Direct Digital Command Daemon    */
/*    generates without any other hardware digital commands    */
/*    to control electric model railroads                      */
/*                                                             */
/* file: cycles.c                                              */
/* job : implements some cycles to generate base voltages on   */
/*       the track.                                            */
/*                                                             */
/* Torsten Vogt, march 1999                                    */
/*                                                             */
/* last changes:                                               */
/*               Torsten Vogt, december 2001                   */
/*               Torsten Vogt, september 2000                  */
/*                                                             */
/***************************************************************/

#include "config-srcpd.h"
#include "ddl_maerklin.h"
#include "ddl_nmra.h"

#define TRUE  1
#define FALSE 0

//#define MAXDATA      40
#define MAXDATA      20
static char idle_data[MAXDATA];
static char NMRA_idle_data[PKTSIZE];

/* ******** Q U E U E *****************/

#define QSIZE 2000

static pthread_mutex_t queue_mutex;   /* mutex to synchronize queue inserts */
static int queue_initialized = 0;
static int out=0, in=0;

typedef struct _tQData {
   int  packet_type;
   int  packet_size;
   char packet[PKTSIZE];
   int  addr;
} tQData ;
static tQData QData[QSIZE];

int monitor_NrOfQCmds() {
   if (in>=out) return in-out;
   return QSIZE-out+in;
}

void queue_init() {

   int error,i;

   error = pthread_mutex_init(&queue_mutex, NULL);
   if (error) {
      DBG(0, DBG_ERROR, "DDL Engine: cannot create mutex. Abort!");
      exit(1);
   }

   for (i=0; i<QSIZE; i++) {
      QData[i].packet_type=QNOVALIDPKT;
      QData[i].addr=0;
      memset(QData[i].packet, 0, PKTSIZE);
   }
   in  = 0;
   out = 0;

   queue_initialized = 1;
}

int queue_empty() {
   return (in==out);
}

void queue_add(int addr, char *packet, int packet_type, int packet_size) {

   if (!queue_initialized) queue_init();

   pthread_mutex_lock(&queue_mutex);
   memset(QData[in].packet,0,PKTSIZE);
   memcpy(QData[in].packet,packet,packet_size);
   QData[in].packet_type=packet_type;
   QData[in].packet_size=packet_size;
   QData[in].addr=addr;
   in++;
   if (in==QSIZE) in=0;
   pthread_mutex_unlock(&queue_mutex);
}

int queue_get(int *addr, char *packet, int *packet_size) {

   int rtc;

   if (!queue_initialized || queue_empty()) return QEMPTY;

   memcpy(packet,QData[out].packet,PKTSIZE);
   rtc=QData[out].packet_type;
   *packet_size=QData[out].packet_size;
   *addr=QData[out].addr;
   QData[out].packet_type=QNOVALIDPKT;
   out++;
   if (out==QSIZE) out=0;
   return rtc;
}


/* functions to open, initialize and close comport */

static struct termios maerklin_dev_termios;
static struct termios nmra_dev_termios;

#if linux
static struct serial_struct *serinfo_marklin=NULL;
static struct serial_struct *serinfo_nmradcc=NULL;

int init_serinfo(int fd, int divisor, struct serial_struct **serinfo) {
   if (*serinfo==NULL) {
      *serinfo=malloc(sizeof(struct serial_struct));
      if (!*serinfo) return -1;
   }
   if (ioctl(fd, TIOCGSERIAL, *serinfo) < 0) return -1;
   (*serinfo)->custom_divisor = divisor;
   (*serinfo)->flags = ASYNC_SPD_CUST | ASYNC_SKIP_TEST;
   return 0;
}

int set_customdivisor(int fd, struct serial_struct *serinfo) {
   if (ioctl(fd, TIOCSSERIAL, serinfo) < 0) return -1;
   return 0;
}

int reset_customdivisor(int fd) {
   struct serial_struct *serinfo = NULL;

   serinfo=malloc(sizeof(struct serial_struct));
   if (!serinfo) return -1;
   if (ioctl(fd, TIOCGSERIAL, serinfo) < 0) return -1;
   (serinfo)->custom_divisor = 0;
   (serinfo)->flags = 0;
   if (ioctl(fd, TIOCSSERIAL, serinfo) < 0) return -1;
   return 0;
}
#endif

int setSerialMode(int busnumber, int mode) {

   switch (mode) {
      case SDM_MAERKLIN:
         if (__DDL -> SERIAL_DEVICE_MODE != SDM_MAERKLIN) {
            if (tcsetattr(busses[busnumber].fd,TCSANOW,&maerklin_dev_termios)!=0) {
               DBG(busnumber, DBG_ERROR,"   error setting serial device mode to marklin!");
               return -1;
            }
#if linux
            if (__DDL->IMPROVE_NMRADCC_TIMING) {
               if (set_customdivisor(busses[busnumber].fd, serinfo_marklin)!=0) {
                  DBG(busnumber, DBG_ERROR, "   cannot set custom divisor for maerklin of serial device!");
                  return -1;
               }
            }
#endif
            __DDL -> SERIAL_DEVICE_MODE = SDM_MAERKLIN;
         }
         break;
      case SDM_NMRA:
         if (__DDL -> SERIAL_DEVICE_MODE != SDM_NMRA) {
            if (tcsetattr(busses[busnumber].fd,TCSANOW,&nmra_dev_termios)!=0) {
               DBG(busnumber, DBG_ERROR,"   error setting serial device mode to NMRA!");
               return -1;
            }
#if linux
            if (__DDL -> IMPROVE_NMRADCC_TIMING) {
               if (set_customdivisor(busses[busnumber].fd, serinfo_nmradcc)!=0) {
                  DBG(busnumber, DBG_ERROR, "   cannot set custom divisor for nmra dcc of serial device!");
                  return -1;
               }
            }
#endif
            __DDL -> SERIAL_DEVICE_MODE = SDM_NMRA;
         }
         break;
      default:
         DBG(busnumber, DBG_ERROR,"   error setting serial device to unknown mode!");
         return -1;
   }

   return 0;
}

int init_lineDDL(int busnumber) {
   /* opens and initializes the selected comport */
   /* returns a file handle                      */

   int dev;

   dev = open(busses[busnumber].device, O_WRONLY);            /* open comport                 */
   if (dev<0) {
      DBG(busnumber, DBG_FATAL, "   device %s can not be opened for writing. Abort!", busses[busnumber].device);
      exit (1);                             /* theres no chance to continue */
   }

#if linux
   if (reset_customdivisor(dev)!=0) {
      DBG(busnumber, DBG_FATAL, "   error initializing device %s. Abort!", busses[busnumber].device);
      exit(1);
   }
#endif

   tcflush(dev,TCOFLUSH);
   tcflow(dev,TCOOFF);          /* suspend output */

   if (tcgetattr(dev, &maerklin_dev_termios)!=0) {
      DBG(busnumber, DBG_FATAL, "   error initializing device %s. Abort!", busses[busnumber].device);
      exit(1);
   }
   if (tcgetattr(dev, &nmra_dev_termios)!=0) {
      DBG(busnumber, DBG_FATAL, "   error initializing device %s. Abort!", busses[busnumber].device);
      exit(1);
   }

   /* init termios structur for maerklin mode */
   maerklin_dev_termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
   maerklin_dev_termios.c_oflag &= ~(OPOST);
   maerklin_dev_termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
   maerklin_dev_termios.c_cflag &= ~(CSIZE | PARENB);
   maerklin_dev_termios.c_cflag |= CS6;        /* 6 data bits      */
   cfsetospeed(&maerklin_dev_termios,B38400);  /* baud rate: 38400 */
   cfsetispeed(&maerklin_dev_termios,B38400);  /* baud rate: 38400 */

   /* init termios structur for nmra mode */
   nmra_dev_termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
   nmra_dev_termios.c_oflag &= ~(OPOST);
   nmra_dev_termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
   nmra_dev_termios.c_cflag &= ~(CSIZE | PARENB);
   nmra_dev_termios.c_cflag |= CS8;            /* 8 data bits      */
   if (__DDL->IMPROVE_NMRADCC_TIMING)
   {
      cfsetospeed(&nmra_dev_termios,B38400);      /* baud rate: 38400 */
	  cfsetispeed(&nmra_dev_termios,B38400);      /* baud rate: 38400 */
   }
   else
   {
      cfsetospeed(&nmra_dev_termios,B19200);      /* baud rate: 19200 */
	  cfsetospeed(&nmra_dev_termios,B19200);      /* baud rate: 19200 */
   }

#if linux
   // if IMPROVE_NMRADCC_TIMING is set, we have to initialize some
   // structures
   if (__DDL -> IMPROVE_NMRADCC_TIMING) {
      if (init_serinfo(dev,3,&serinfo_marklin)!=0) {
         DBG(busnumber, DBG_FATAL, "   error initializing device %s. Abort!", busses[busnumber].device);
         exit(1);
      }
      if (init_serinfo(dev,7,&serinfo_nmradcc)!=0) {
         DBG(busnumber, DBG_FATAL, "   error initializing device %s. Abort!",  busses[busnumber].device);
         exit(1);
      }
   }
#endif
   busses[busnumber].fd = dev; /* we need that value at the next step */
   /* setting serial device to default mode */
   if (!setSerialMode(busnumber,SDM_DEFAULT)==0) {
      DBG(busnumber, DBG_FATAL, "   error initializing device %s. Abort!", busses[busnumber].device);
      exit(1);
   }

   /* I don't know, why the following code is necessary, but without it,
      it don't work ;-( */
   tcflow(dev, TCOON);
   set_SerialLine(busnumber, SL_DTR,ON);
   write(dev,"TORSTENVOGT", 11);
   tcflush(dev, TCOFLUSH);

   /* now set some serial lines */
   tcflow(dev, TCOOFF);
   set_SerialLine(busnumber, SL_RTS,ON);  /* +12V for ever on RTS   */
   set_SerialLine(busnumber, SL_CTS,OFF); /* -12V for ever on CTS   */
   set_SerialLine(busnumber, SL_DTR,OFF); /* disable booster output */

   return dev;
}


/****** routines and data types for maerklin packet pool ******/

static pthread_mutex_t maerklin_pktpool_mutex = PTHREAD_MUTEX_INITIALIZER;
static int isMaerklinPackedPoolInitialized = FALSE;

static tMaerklinPacketPool MaerklinPacketPool;

void init_MaerklinPacketPool(int busnumber) {
   int i,j;
   int error;

   if (!(__DDL -> ENABLED_PROTOCOLS & EP_MAERKLIN)) return;

   error = pthread_mutex_init(&maerklin_pktpool_mutex, NULL);
   if (error) {
      DBG(busnumber, DBG_ERROR, "cannot create mutex (maerklin packet pool). Abort!");
      exit(1);
   }

   pthread_mutex_lock(&maerklin_pktpool_mutex);

   for (i=0; i<=MAX_MARKLIN_ADDRESS; i++) {
      MaerklinPacketPool.knownAdresses[i]=0;
   }
   MaerklinPacketPool.NrOfKnownAdresses = 1;
   MaerklinPacketPool.knownAdresses[MaerklinPacketPool.NrOfKnownAdresses-1]=81;
   /* generate idle packet */
   for (i=0; i<4; i++) {
      MaerklinPacketPool.packets[81].packet[2*i]   = HI;
      MaerklinPacketPool.packets[81].packet[2*i+1] = LO;
      for (j=0; j<4; j++) {
         MaerklinPacketPool.packets[81].f_packets[j][2*i]   = HI;
         MaerklinPacketPool.packets[81].f_packets[j][2*i+1] = LO;
      }
   }
   for (i=4; i<9; i++) {
      MaerklinPacketPool.packets[81].packet[2*i]   = LO;
      MaerklinPacketPool.packets[81].packet[2*i+1] = LO;
      for (j=0; j<4; j++) {
         MaerklinPacketPool.packets[81].f_packets[j][2*i]   = LO;
         MaerklinPacketPool.packets[81].f_packets[j][2*i+1] = LO;
      }
   }
   isMaerklinPackedPoolInitialized = TRUE;

   pthread_mutex_unlock(&maerklin_pktpool_mutex);

   /* generate idle_data */
   for (i=0; i<MAXDATA; i++) idle_data[i]=0x55;      /* 0x55 = 01010101 */
   for (i=0; i<PKTSIZE; i++) NMRA_idle_data[i]=0x55;
   /* ATTENTION: if nmra dcc mode is activated idle_data[] and NMRA_idle_data
                 must be overidden from init_NMRAPacketPool().
                 This means, that init_NMRAPacketPool()
                 must be called after init_MaerklinPacketPool().
    */
}

char *get_maerklin_packet(int bus, int adr, int fx) {
   if (isMaerklinPackedPoolInitialized == FALSE) {
      init_MaerklinPacketPool(bus);
   }
   return MaerklinPacketPool.packets[adr].f_packets[fx];
}

void update_MaerklinPacketPool(int bus, int adr, char *sd_packet, char *f1, char *f2,
                                                         char *f3, char *f4) {

   int i, found;

   if (isMaerklinPackedPoolInitialized == FALSE) {
      init_MaerklinPacketPool(bus);
   }
   DBG(bus, DBG_INFO, "update MM Packet Pool: %d", adr);
   found = 0;
   for (i=0; i<MaerklinPacketPool.NrOfKnownAdresses && !found; i++)
      if (MaerklinPacketPool.knownAdresses[i]==adr) found=TRUE;

   pthread_mutex_lock(&maerklin_pktpool_mutex);
   memcpy(MaerklinPacketPool.packets[adr].packet,sd_packet,18);
   memcpy(MaerklinPacketPool.packets[adr].f_packets[0],f1,18);
   memcpy(MaerklinPacketPool.packets[adr].f_packets[1],f2,18);
   memcpy(MaerklinPacketPool.packets[adr].f_packets[2],f3,18);
   memcpy(MaerklinPacketPool.packets[adr].f_packets[3],f4,18);
   pthread_mutex_unlock(&maerklin_pktpool_mutex);

   if (MaerklinPacketPool.NrOfKnownAdresses==1 &&
       MaerklinPacketPool.knownAdresses[0]==81) {
      MaerklinPacketPool.NrOfKnownAdresses=0;
   }
   if (!found) {
     MaerklinPacketPool.knownAdresses[MaerklinPacketPool.NrOfKnownAdresses]=adr;
     MaerklinPacketPool.NrOfKnownAdresses++;
   }
}

/**********************************************************/

/****** routines and data types for NMRA packet pool ******/

static pthread_mutex_t nmra_pktpool_mutex = PTHREAD_MUTEX_INITIALIZER;
static int isNMRAPackedPoolInitialized = FALSE;
static tNMRAPacketPool NMRAPacketPool;

void init_NMRAPacketPool(int busnumber) {
   int i,j;
   int error;
   char idle_packet[] = "11111111111111101111111100000000001111111110";
   char idle_pktstr[PKTSIZE];

   if (!(__DDL->ENABLED_PROTOCOLS & EP_NMRADCC)) return;

   error = pthread_mutex_init(&nmra_pktpool_mutex, NULL);
   if (error) {
      DBG(busnumber, DBG_ERROR, "cannot create mutex (nmra packet pool). Abort!");
      exit(1);
   }

   pthread_mutex_lock(&nmra_pktpool_mutex);
   for (i=0; i<=MAX_NMRA_ADDRESS; i++) {
      NMRAPacketPool.knownAdresses[i]=0;
   }
   NMRAPacketPool.NrOfKnownAdresses = 0;
   pthread_mutex_unlock(&nmra_pktpool_mutex);

   isNMRAPackedPoolInitialized=TRUE;

   /* put idle packet in packet pool */
   j=translateBitstream2Packetstream(__DDL->NMRADCC_TR_V, idle_packet, idle_pktstr, FALSE);
   update_NMRAPacketPool(busnumber, 255, idle_pktstr, j, idle_pktstr, j);

   /* generate idle_data */
   for (i=0; i<MAXDATA; i++) idle_data[i]=idle_pktstr[i % j];
   for (i=(MAXDATA/j)*j; i<MAXDATA; i++) idle_data[i]=0xC6;
   memcpy(NMRA_idle_data,idle_pktstr,j);
}

void update_NMRAPacketPool(int bus, int adr, char *packet, int packet_size,
                                    char *fx_packet, int fx_packet_size) {

   int i, found;

   if (isNMRAPackedPoolInitialized == FALSE) {
      init_NMRAPacketPool(bus);
   }

   found = 0;
   for (i=0; i<=NMRAPacketPool.NrOfKnownAdresses && !found; i++)
      if (NMRAPacketPool.knownAdresses[i]==adr) found=TRUE;

   pthread_mutex_lock(&nmra_pktpool_mutex);
   memcpy(NMRAPacketPool.packets[adr].packet,packet,packet_size);
   NMRAPacketPool.packets[adr].packet_size=packet_size;
   memcpy(NMRAPacketPool.packets[adr].fx_packet,fx_packet,fx_packet_size);
   NMRAPacketPool.packets[adr].fx_packet_size=fx_packet_size;
   pthread_mutex_unlock(&nmra_pktpool_mutex);

   if (NMRAPacketPool.NrOfKnownAdresses==1 &&
       NMRAPacketPool.knownAdresses[0]==255) {
      NMRAPacketPool.NrOfKnownAdresses=0;
   }
   if (!found) {
      NMRAPacketPool.knownAdresses[NMRAPacketPool.NrOfKnownAdresses]=adr;
      NMRAPacketPool.NrOfKnownAdresses++;
   }
}
/* argument for nanosleep to do non-busy waiting */
static struct timespec rqtp_sleep  = { 0, 2500000 }; /* ==> non-busy waiting */

/* arguments for nanosleep and marklin loco decoders (19KHz) */
static struct timespec rqtp_btw19K = { 0, 1250000 }; /* ==> busy waiting     */
static struct timespec rqtp_end19K = { 0, 1700000 }; /* ==> busy waiting     */

/* arguments for nanosleep and marklin solenoids/func decoders (38KHz) */
static struct timespec rqtp_btw38K = { 0,  625000 }; /* ==> busy waiting     */
static struct timespec rqtp_end38K = { 0,  850000 }; /* ==> busy waiting     */

static struct timespec rmtp;

void waitUARTempty_COMMON(busnumber) {
   int result;
   do {         /* wait until UART is empty */
#if linux
      ioctl(busses[busnumber].fd,TIOCSERGETLSR,&result);
#else
      ioctl(busses[busnumber].fd,TCSADRAIN,&result);
#endif
   } while(!result);
}

void waitUARTempty_COMMON_USLEEPPATCH(busnumber) {
   int result;
   do {         /* wait until UART is empty */
#if linux
      ioctl(busses[busnumber].fd,TIOCSERGETLSR,&result);
#else
      ioctl(busses[busnumber].fd,TCSADRAIN,&result);
#endif
      usleep(__DDL->WAITUART_USLEEP_USEC);
   } while(!result);
}

/* new Version of waitUARTempty() for a clean NMRA-DCC signal */
/* from Harald Barth */
#define SLEEPFACTOR 48000l                /* used in waitUARTempty() */
#define NUMBUFFERBYTES 1024               /* used in waitUARTempty() */
static struct timespec sleeptime = {0,2000000L}; /* used in waitUARTempty() */

void waitUARTempty_CLEANNMRADCC(int busnumber) {
   int outbytes;

   /* look how many bytes are in UART's outbuffer */
   ioctl(busses[busnumber].fd,TIOCOUTQ, &outbytes);

   if (outbytes > NUMBUFFERBYTES) {
      /* calculate sleeptime */
      sleeptime.tv_sec = outbytes / SLEEPFACTOR;
      sleeptime.tv_nsec = (outbytes % SLEEPFACTOR)*(1000000000l/SLEEPFACTOR);

      nanosleep(&sleeptime,NULL);
   }
}

static struct timeval tv_short_detected = {0, 0};
static struct timeval tv_short_now = {0, 0};
static time_t short_detected=0;
static time_t short_now=0;
static struct timezone tz_short;

int checkRingIndicator(int busnumber) {
   int result, arg;
   result = ioctl(busses[busnumber].fd, TIOCMGET, &arg);
   if (result>=0) {
      if (arg&TIOCM_RI) {
         DBG(busnumber, DBG_INFO, "   ring indicator alert. power on tracks stopped!");
         return 1;
      }
      return 0;
   }
   else {
      DBG(busnumber, DBG_ERROR, "   ioctl() call failed. power on tracks stopped!");
      return 1;
   }
}

int checkShortcut(int busnumber) {
   int result, arg;
   result = ioctl(busses[busnumber].fd, TIOCMGET, &arg);
   if (result>=0) {
      if ( ((arg&TIOCM_DSR)&&(!__DDL->DSR_INVERSE)) ||
           ((~arg&TIOCM_DSR)&&(__DDL->DSR_INVERSE))) {
         if (short_detected==0) {
            gettimeofday(&tv_short_detected,&tz_short);
            short_detected=tv_short_detected.tv_sec*1000000 +
                           tv_short_detected.tv_usec;
         }
         gettimeofday(&tv_short_now,&tz_short);
         short_now=tv_short_now.tv_sec*1000000+tv_short_now.tv_usec;
         if (__DDL->SHORTCUTDELAY<=(short_now-short_detected)) {
            DBG(busnumber, DBG_INFO, "   shortcut detected. power on tracks stopped!");
            return 1;
         }
      }
      else {
         short_detected=0;
         return 0;
      }
   }
   else {
      DBG(busnumber, DBG_INFO, "   ioctl() call failed. power on tracks stopped!");
      return 1;
   }
   return 0;
}

void send_packet(int busnumber, int addr, char *packet, int packet_size, int packet_type, int refresh) {

   int i,laps;

   waitUARTempty(busnumber);

   switch (packet_type) {
      case QM1LOCOPKT:
      case QM2LOCOPKT:
         if (setSerialMode(busnumber,SDM_MAERKLIN)<0) return;
         if (refresh) laps=2; else laps=4;    // YYTV 9
         for (i=0; i<laps; i++) {
            nanosleep(&rqtp_end19K, &rmtp);
            write(busses[busnumber].fd,packet,18);
            waitUARTempty(busnumber);
            nanosleep(&rqtp_btw19K, &rmtp);
            write(busses[busnumber].fd,packet,18);
            waitUARTempty(busnumber);
         }
         break;
      case QM2FXPKT:
         if (setSerialMode(busnumber,SDM_MAERKLIN)<0) return;
         if (refresh) laps=2; else laps=3;   // YYTV 6
         for (i=0; i<laps; i++) {
            nanosleep(&rqtp_end19K, &rmtp);
            write(busses[busnumber].fd,packet,18);
            waitUARTempty(busnumber);
            nanosleep(&rqtp_btw19K, &rmtp);
            write(busses[busnumber].fd,packet,18);
            waitUARTempty(busnumber);
         }
         break;
      case QM1FUNCPKT:
      case QM1SOLEPKT:
         if (setSerialMode(busnumber,SDM_MAERKLIN)<0) return;
         for (i=0; i<2; i++) {
           nanosleep(&rqtp_end38K, &rmtp);
           write(busses[busnumber].fd,packet,9);
           waitUARTempty(busnumber);
           nanosleep(&rqtp_btw38K, &rmtp);
           write(busses[busnumber].fd,packet,9);
           waitUARTempty(busnumber);
         }
         break;
      case QNBLOCOPKT:
      case QNBACCPKT:
         if (setSerialMode(busnumber,SDM_NMRA)<0) return;
         write(busses[busnumber].fd,packet,packet_size);
         waitUARTempty(busnumber);
         write(busses[busnumber].fd,NMRA_idle_data,13);
         waitUARTempty(busnumber);
         // nanosleep(&rqtp_btw38K, &rmtp);
         write(busses[busnumber].fd,packet,packet_size);
         // waitUARTempty();
         break;
   }
   if (__DDL->ENABLED_PROTOCOLS & EP_MAERKLIN)
      nanosleep(&rqtp_end38K, &rmtp);
   if (setSerialMode(busnumber,SDM_DEFAULT)<0) return;
}

static int last_refreshed_loco      = 0;
static int last_refreshed_fx        = -1;
static int last_refreshed_nmra_loco = 0;
static int nmra_fx_refresh          = -1;
static int maerklin_refresh         = 0;

void refresh_loco(int busnumber) {

    int adr;

    if (maerklin_refresh) {
       adr = MaerklinPacketPool.knownAdresses[last_refreshed_loco];
       tcflush(busses[busnumber].fd, TCOFLUSH);
       if (last_refreshed_fx<0)
          send_packet(busnumber, adr,MaerklinPacketPool.packets[adr].packet,
                      18,QM2LOCOPKT,TRUE);
       else
          send_packet(busnumber, adr,MaerklinPacketPool.packets[adr].f_packets[last_refreshed_fx],18,QM2FXPKT,TRUE);
       last_refreshed_fx++;
       if (last_refreshed_fx==4) {
          last_refreshed_fx=-1;
          last_refreshed_loco++;
          if (last_refreshed_loco>=MaerklinPacketPool.NrOfKnownAdresses) {
             last_refreshed_loco=0;
          }
       }
    }
    else {
       adr = NMRAPacketPool.knownAdresses[last_refreshed_nmra_loco];
       if (adr>=0) {
          if (nmra_fx_refresh<0) {
             send_packet(busnumber, adr,NMRAPacketPool.packets[adr].packet,
                       NMRAPacketPool.packets[adr].packet_size,QNBLOCOPKT,TRUE);
             nmra_fx_refresh=0;
          }
          else {
             send_packet(busnumber, adr,NMRAPacketPool.packets[adr].fx_packet,
                    NMRAPacketPool.packets[adr].fx_packet_size,QNBLOCOPKT,TRUE);
             nmra_fx_refresh=1;
          }
       }
       if (nmra_fx_refresh==1) {
          last_refreshed_nmra_loco++;
          nmra_fx_refresh=-1;
          if (last_refreshed_nmra_loco>=NMRAPacketPool.NrOfKnownAdresses) {
             last_refreshed_nmra_loco=0;
          }
       }
    }
    if (__DDL->ENABLED_PROTOCOLS == (EP_MAERKLIN | EP_NMRADCC))
       maerklin_refresh = !maerklin_refresh;
}

long int compute_delta(struct timeval tv1, struct timeval tv2) {

   long int delta_sec;
   long int delta_usec;

   delta_sec = tv2.tv_sec - tv1.tv_sec;
   if (delta_sec == 0) {
      delta_usec = tv2.tv_usec - tv1.tv_usec;
   }
   else {
      if (delta_sec == 1)
         delta_usec=tv2.tv_usec+(1000000-tv1.tv_usec);
      else
         delta_usec=1000000*(delta_sec-1)+tv2.tv_usec+(1000000-tv1.tv_usec);
   }
   return delta_usec;
}

void set_SerialLine(int busnumber, int line, int mode) {
   int result, arg;
   result = ioctl(busses[busnumber].fd, TIOCMGET, &arg);
   if (result>=0) {
      switch(line) {
         case SL_DTR: if (mode==OFF) arg &= ~TIOCM_DTR;
                      if (mode==ON)  arg |= TIOCM_DTR;
                      break;
         case SL_DSR: if (mode==OFF) arg &= ~TIOCM_DSR;
                      if (mode==ON)  arg |= TIOCM_DSR;
                      break;
         case SL_RI : if (mode==OFF) arg &= ~TIOCM_RI;
                      if (mode==ON)  arg |= TIOCM_RI;
                      break;
         case SL_RTS: if (mode==OFF) arg &= ~TIOCM_RTS;
                      if (mode==ON)  arg |= TIOCM_RTS;
                      break;
         case SL_CTS: if (mode==OFF) arg &= ~TIOCM_CTS;
                      if (mode==ON)  arg |= TIOCM_CTS;
                      break;
      }
      result = ioctl(busses[busnumber].fd, TIOCMSET, &arg);
   }
   if (result<0) {
      DBG(busnumber, DBG_ERROR, "   ioctl() call failed. Serial line not set! (%d: %d)", line, mode);
   }
}


/* ************************************************ */

void start_voltage(int busnumber) {
   int error;

   if (__DDL->started_thread_flag==0) {
      error = pthread_create(&(__DDL->ptid), NULL,  thr_refresh_cycle, (void*) busnumber);
	  __DDL->started_thread_flag=1;
      if (error) {
          DBG(busnumber, DBG_FATAL,"cannot create thread: refresh_cycle. Abort!");
          exit(1);
      }
   }
   DBG(busnumber, DBG_INFO, "nrefresh cycle started.");
}

void stop_voltage(int busnumber) {
  if (__DDL->started_thread_flag!=0) {
     cancel_refresh_cycle(busnumber);
  }
  DBG(busnumber, DBG_INFO, "refresh cycle canceled.");
}

void reset(int busnumber) {
   if (__DDL->started_thread_flag!=0) {
      cancel_refresh_cycle(busnumber);
   }
   init_MaerklinPacketPool(busnumber);
   init_NMRAPacketPool(busnumber);
   DBG(busnumber, DBG_INFO, "ddl bus reset done.\n");
}

void cancel_refresh_cycle(int busnumber) {
   /* store thread return value here */
   void* pThreadReturn;
   /* send cancel to refresh cycle */
   pthread_cancel(__DDL->ptid);
   /* wait until the refresh cycle has terminated */
   pthread_join(__DDL->ptid,&pThreadReturn);
   /* set interface lines to the off state */
   tcflush(busses[busnumber].fd, TCOFLUSH);
   tcflow(busses[busnumber].fd, TCOOFF);
   set_SerialLine(busnumber, SL_DTR,OFF);
   /* clear thread struct */
   //__DDL->ptid=(pthread_t)NULL;
   __DDL->started_thread_flag = 0;
   pthread_exit(__DDL->ptid);
}


void mkclean(busnumber) {
   tcflush(busses[busnumber].fd, TCOFLUSH);
   tcflow(busses[busnumber].fd, TCOOFF);
   set_SerialLine(busnumber,SL_DTR,OFF);
   //__DDL->ptid=(pthread_t)NULL;
   __DDL->started_thread_flag = 0;
   pthread_exit(__DDL->ptid);
}

void *thr_refresh_cycle(void *v) {

   int l;
   struct sched_param sparam;
   int packet_size;
   int packet_type;
   char packet[PKTSIZE];
   int addr;
   int busnumber = (int) v;;

   // set the best waitUARTempty-Routine
   waitUARTempty = waitUARTempty_COMMON;
   if (__DDL->WAITUART_USLEEP_PATCH) {
      waitUARTempty = waitUARTempty_COMMON_USLEEPPATCH;
   }
   if ((__DDL->ENABLED_PROTOCOLS & EP_NMRADCC) && !(__DDL -> ENABLED_PROTOCOLS & EP_MAERKLIN)){
      waitUARTempty = waitUARTempty_CLEANNMRADCC;
   }

   if (__DDL->ENABLED_PROTOCOLS & EP_MAERKLIN)
      maerklin_refresh=1;
   else
      maerklin_refresh=0;

   if (isMaerklinPackedPoolInitialized == FALSE) {
      init_MaerklinPacketPool(busnumber);
   }

   if (isNMRAPackedPoolInitialized == FALSE) {
      init_NMRAPacketPool(busnumber);
   }

   sparam.sched_priority=10;
   sched_setscheduler(0,SCHED_FIFO,&sparam);
   tcflow(busses[busnumber].fd, TCOON);
   set_SerialLine(busnumber, SL_DTR,ON);

   short_detected=0;

   l=0;
   gettimeofday(& (__DDL->tv1), & (__DDL->tz));
   for (;;) {
      write(busses[busnumber].fd,idle_data,MAXDATA);
      packet_type=queue_get(&addr,packet,&packet_size);/*now,look at commands*/
      if (packet_type>QNOVALIDPKT) {
         tcflush(busses[busnumber].fd, TCOFLUSH);
         while (packet_type>QNOVALIDPKT) {
             if (__DDL->CHECKSHORT) if (checkShortcut(busnumber)==1) {mkclean(busnumber);return NULL;}
             if (__DDL->RI_CHECK) if (checkRingIndicator(busnumber)==1){mkclean(busnumber);return NULL;}
             send_packet(busnumber, addr,packet,packet_size,packet_type,FALSE);
             if (__DDL->ENABLED_PROTOCOLS==(EP_MAERKLIN | EP_NMRADCC))
                write(busses[busnumber].fd,NMRA_idle_data,13);
             packet_type=queue_get(&addr,packet,&packet_size);
         }
      } else {                          /* no commands? Then we do a refresh */
         if (__DDL->CHECKSHORT) if (checkShortcut(busnumber)==1) {mkclean(busnumber);return NULL;}
         if (__DDL->RI_CHECK)   if (checkRingIndicator(busnumber)==1) {mkclean(busnumber);return NULL;}
         gettimeofday(&(__DDL->tv2), &(__DDL->tz));
         if (compute_delta(__DDL->tv1,__DDL->tv2)>100000) {        /* but not every time! */
            refresh_loco(busnumber);
            gettimeofday(&(__DDL->tv1), &(__DDL->tz));
         } else {
            nanosleep(&rqtp_sleep, &rmtp);
         }
      }
   }

   return NULL;
}

int init_gl_DDL(struct _GLSTATE *gl) {
    switch(gl->protocol) {
	    case 'M': /* Motorola Codes */
		return ( gl -> protocolversion>0 && gl->protocolversion<=5) ? SRCP_OK : SRCP_WRONGVALUE;
		break;
	    case 'N':
		return ( gl -> protocolversion>0 && gl->protocolversion<=5) ? SRCP_OK : SRCP_WRONGVALUE;
		break;
    }
    return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
}

int init_ga_DDL(struct _GASTATE *ga) {
    switch(ga->protocol) {
	    case 'M': /* Motorola Codes */
		return SRCP_OK;
		break;
	    case 'N':
		return SRCP_OK;
		break;
    }
    return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
}


int readconfig_DDL(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
  xmlNodePtr child = node->children;

  busses[busnumber].type = SERVER_DDL;
  busses[busnumber].init_func = &init_bus_DDL;
  busses[busnumber].term_func = &term_bus_DDL;
  busses[busnumber].init_gl_func = &init_gl_DDL;
  busses[busnumber].init_ga_func = &init_ga_DDL;

  busses[busnumber].thr_func = &thr_sendrec_DDL;

  busses[busnumber].driverdata = malloc(sizeof(struct _DDL_DATA));
  strcpy(busses[busnumber].description, "GA GL POWER LOCK DESCRIPTION");

  __DDL -> number_gl = 81;
  __DDL -> number_ga = 324;
  __DDL -> RI_CHECK = FALSE;              /* ring indicator checking      */
  __DDL -> CHECKSHORT = FALSE;            /* default no shortcut checking */
  __DDL -> DSR_INVERSE = FALSE;           /* controls how DSR is used to  */
                                          /* check shorts                 */
  __DDL -> SHORTCUTDELAY = 0;             /* usecs shortcut delay         */
  __DDL -> NMRADCC_TR_V = 3;              /* version of the nmra dcc      */
                                          /* translation routine (1 or 2) */
  __DDL -> ENABLED_PROTOCOLS = (EP_MAERKLIN | EP_NMRADCC); /* enabled p's */
  __DDL -> IMPROVE_NMRADCC_TIMING = 0;    /* NMRA DCC: improve timing     */

  __DDL -> WAITUART_USLEEP_PATCH = 0;     /* enable/disbable usleep patch */
  __DDL -> WAITUART_USLEEP_USEC = 0;      /* usecs for usleep patch       */

  __DDL -> SERIAL_DEVICE_MODE = SDM_NOTINITIALIZED;
  while (child)
  {
    if(strncmp(child->name, "text", 4)==0)
    {
      child = child -> next;
      continue;
    }
    if (strcmp(child->name, "number_gl") == 0)
    {
      char *txt =
      xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __DDL->number_gl = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "number_ga") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __DDL->number_ga = atoi(txt);
      free(txt);
    }

    if (strcmp(child->name, "enable_ringindicator_checking") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __DDL->RI_CHECK = (strcmp(txt, "yes") == 0) ? TRUE:FALSE;
      free(txt);
    }

    if (strcmp(child->name, "enable_checkshort_checking") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __DDL->CHECKSHORT = (strcmp(txt, "yes") == 0) ? TRUE:FALSE;
      free(txt);
    }
    if (strcmp(child->name, "inverse_dsr_handling") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __DDL->DSR_INVERSE = (strcmp(txt, "yes") == 0) ? TRUE:FALSE;
      free(txt);
    }

    if (strcmp(child->name, "enable_maerklin") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      if (strcmp(txt, "yes") == 0)
      {
        __DDL->ENABLED_PROTOCOLS |= EP_MAERKLIN;
      } else {
        __DDL->ENABLED_PROTOCOLS &= ~EP_MAERKLIN;
      }
      free(txt);
    }

    if (strcmp(child->name, "enable_nmradcc") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      if (strcmp(txt, "yes") == 0)
      {
        __DDL->ENABLED_PROTOCOLS |= EP_NMRADCC;
      } else {
        __DDL->ENABLED_PROTOCOLS &= ~EP_NMRADCC;
      }
      free(txt);
    }

    if (strcmp(child->name, "improve_nmradcc_timing") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __DDL->IMPROVE_NMRADCC_TIMING = (strcmp(txt, "yes") == 0) ? TRUE:FALSE;
      free(txt);
    }

    if (strcmp(child->name, "shortcut_failure_delay") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __DDL->SHORTCUTDELAY = atoi(txt);
      free(txt);
    }

    if (strcmp(child->name, "nmradcc_translation_routine") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __DDL->NMRADCC_TR_V = atoi(txt);
      free(txt);
    }

    if (strcmp(child->name, "enable_ulseep_patch") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __DDL->WAITUART_USLEEP_PATCH = (strcmp(txt, "yes") == 0) ? TRUE:FALSE;
      free(txt);
    }

    if (strcmp(child->name, "usleep_usec") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __DDL->WAITUART_USLEEP_USEC = atoi(txt);
      free(txt);
    }

    child = child -> next;
  } // while
  if(init_GA(busnumber, __DDL->number_ga))
  {
    __DDL->number_ga = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for assesoirs");
  }

  if(init_GL(busnumber, __DDL->number_gl))
  {
    __DDL->number_gl = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for locomotivs");
  }
  return(1);
}

int term_bus_DDL(int bus)
{
  DBG(bus, DBG_INFO, "DDL bus %d terminating", bus);
  return 0;
}

/* Initialisiere den Bus, signalisiere Fehler */
/* Einmal aufgerufen mit busnummer als einzigem Parameter */
/* return code wird ignoriert (vorerst) */
int init_bus_DDL(int i)
{
  DBG(i, DBG_INFO,"DDL init: bus #%d, debug %d", i, busses[i].debuglevel);
  busses[i].fd = init_lineDDL(i);
  init_MaerklinPacketPool(i);
  init_NMRAPacketPool(i);
  DBG(i, DBG_INFO, "DDL init done");
  return 0;
}

void* thr_sendrec_DDL (void *v)
{
  struct _GLSTATE gltmp;
  struct _GASTATE gatmp;
  int addr;
  int bus = (int) v;
  char msg[110];

  DBG(bus, DBG_INFO, "DDL started, bus #%d, %s", bus, busses[bus].device);

  busses[bus].watchdog = 1;

  while (1) {
    if(short_detected>0) {
        busses[bus].power_changed=1;
        strcpy(busses[bus].power_msg, "SHORT DETECTED");
        infoPower(bus, msg);
        queueInfoMessage(msg);
    }
    if(busses[bus].power_changed==1) {

      if (busses[bus].power_state==0)
        stop_voltage(bus);
      if (busses[bus].power_state==1)
        start_voltage(bus);

      busses[bus].power_changed = 0;
      infoPower(bus, msg);
      queueInfoMessage(msg);

    }
    if(busses[bus].power_state==0) {
          usleep(1000);
          continue;
    }

    if (!queue_GL_isempty(bus))  {
	char p;
	int pv;
	int speed;
	int direction;

        unqueueNextGL(bus, &gltmp);
	p = gltmp.protocol;
	pv = gltmp.protocolversion; // need to compute from the n_fs and n_func parameters
	addr = gltmp.id;
	speed = gltmp.speed;
	direction = gltmp.direction;
	DBG(bus, DBG_DEBUG, "next command: %c (%x) %d %d", p, p, pv, addr);
	switch(p) {
	    case 'M': /* Motorola Codes */
		if(speed==1) speed++; /* Never send FS1 */
                if(direction == 2) speed = 0;
		switch(pv) {
		    case 1: comp_maerklin_1(bus, addr, gltmp.direction, speed, gltmp.funcs & 0x01);
			    break;
		    case 2: comp_maerklin_2(bus, addr, gltmp.direction, speed, gltmp.funcs & 0x01,
			    ( (gltmp.funcs>>1) & 0x01), ( (gltmp.funcs>>2) & 0x01), ( (gltmp.funcs>>3) & 0x01),
			    ( (gltmp.funcs>>4) & 0x01));break;
		    case 3: comp_maerklin_3(bus, addr, gltmp.direction, speed, gltmp.funcs & 0x01,
			    ( (gltmp.funcs>>1) & 0x01), ( (gltmp.funcs>>2) & 0x01), ( (gltmp.funcs>>3) & 0x01),
			    ( (gltmp.funcs>>4) & 0x01));break;
		    case 4: comp_maerklin_4(bus, addr, gltmp.direction, speed, gltmp.funcs & 0x01,
			    ( (gltmp.funcs>>1) & 0x01), ( (gltmp.funcs>>2) & 0x01), ( (gltmp.funcs>>3) & 0x01),
			    ( (gltmp.funcs>>4) & 0x01));break;
		    case 5: comp_maerklin_5(bus, addr, gltmp.direction, speed, gltmp.funcs & 0x01,
			    ( (gltmp.funcs>>1) & 0x01), ( (gltmp.funcs>>2) & 0x01), ( (gltmp.funcs>>3) & 0x01),
			    ( (gltmp.funcs>>4) & 0x01));break;
		}
                break;
	    case 'N': /* NMRA / DCC Codes */
		if(speed==1)speed++;
	        switch (pv) {
	           case 1:
	               if (direction != 2)
    		          comp_nmra_baseline(bus, addr, direction, speed);
	               else
    		          comp_nmra_baseline(bus, addr, 0, 1);  // emergency halt: FS 1
	               break;

    		  case 2:
	               if (direction != 2)
    		          comp_nmra_f4b7s28(bus, addr,direction,speed, gltmp.funcs & 0x01,
			  ( (gltmp.funcs>>1) & 0x01), ( (gltmp.funcs>>2) & 0x01), ( (gltmp.funcs>>3) & 0x01),
    			  ( (gltmp.funcs>>4) & 0x01));
            		else  // emergency halt: FS 1
                	    comp_nmra_f4b7s28(bus, addr,0,1,gltmp.funcs & 0x01,
			    ( (gltmp.funcs>>1) & 0x01), ( (gltmp.funcs>>2) & 0x01), ( (gltmp.funcs>>3) & 0x01),
			    ( (gltmp.funcs>>4) & 0x01));
	               break;

    		  case 3:
	               if (direction != 2)
    		          comp_nmra_f4b7s128(bus, addr,direction,speed,gltmp.funcs & 0x01,
			    ( (gltmp.funcs>>1) & 0x01), ( (gltmp.funcs>>2) & 0x01), ( (gltmp.funcs>>3) & 0x01),
			    ( (gltmp.funcs>>4) & 0x01));
            		else  // emergency halt: FS 1
                	  comp_nmra_f4b7s128(bus, addr,0,1, gltmp.funcs & 0x01,
			    ( (gltmp.funcs>>1) & 0x01), ( (gltmp.funcs>>2) & 0x01), ( (gltmp.funcs>>3) & 0x01),
			    ( (gltmp.funcs>>4) & 0x01));

	               break;
    		  case 4:
	               if (direction != 2)
    		          comp_nmra_f4b14s28(bus, addr,direction,speed,gltmp.funcs & 0x01,
			    ( (gltmp.funcs>>1) & 0x01), ( (gltmp.funcs>>2) & 0x01), ( (gltmp.funcs>>3) & 0x01),
			    ( (gltmp.funcs>>4) & 0x01));

            		else  // emergency halt: FS 1
                	  comp_nmra_f4b14s28(bus, addr,0,1,gltmp.funcs & 0x01,
			    ( (gltmp.funcs>>1) & 0x01), ( (gltmp.funcs>>2) & 0x01), ( (gltmp.funcs>>3) & 0x01),
			    ( (gltmp.funcs>>4) & 0x01));
	               break;
    		  case 5:
	               if (direction != 2)
    		          comp_nmra_f4b14s128(bus, addr,direction,speed,gltmp.funcs & 0x01,
			    ( (gltmp.funcs>>1) & 0x01), ( (gltmp.funcs>>2) & 0x01), ( (gltmp.funcs>>3) & 0x01),
			    ( (gltmp.funcs>>4) & 0x01));

            		else  // emergency halt: FS 1
                	  comp_nmra_f4b14s128(bus, addr,0,1,gltmp.funcs & 0x01,
			    ( (gltmp.funcs>>1) & 0x01), ( (gltmp.funcs>>2) & 0x01), ( (gltmp.funcs>>3) & 0x01),
			    ( (gltmp.funcs>>4) & 0x01));

	               break;
		}
	    }
        setGL(bus, addr, gltmp);
    }
    busses[bus].watchdog = 4;
    if (!queue_GA_isempty(bus)) {
    	char p;
	unqueueNextGA(bus, &gatmp);
        addr = gatmp.id;
	p = gatmp.protocol;
	DBG(bus, DBG_DEBUG, "next GA command: %c (%x) %d", p, p, addr);
	switch(p) {
	    case 'M': /* Motorola Codes */
		comp_maerklin_ms(bus, addr, gatmp.port, gatmp.action);
		break;
	    case 'N':
		comp_nmra_accessory(bus, addr, gatmp.port, gatmp.action);
		break;
          }
          setGA(bus, addr, gatmp);
          busses[bus].watchdog = 5;
          if(gatmp.activetime>=0) {
            usleep(1000L * (unsigned long) gatmp.activetime);
            gatmp.action = 0;
            DBG(bus, DBG_DEBUG, "delayed GA command: %c (%x) %d", p, p, addr);
	    switch(p) {
	     case 'M': /* Motorola Codes */
		comp_maerklin_ms(bus, addr, gatmp.port, gatmp.action);
		break;
	     case 'N':
		comp_nmra_accessory(bus, addr, gatmp.port, gatmp.action);
		break;
            }
            setGA(bus, addr, gatmp);
      }

    }
    usleep(1000);
  }
}
