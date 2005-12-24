/* $Id$ */

/* 
 * loconet: loconet/srcp gateway
 */

#include "stdincludes.h"

#include "config-srcpd.h"
#include "io.h"
#include "loconet.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-srv.h"
#include "srcp-info.h"
#include "srcp-error.h"

#define __loconet ((LOCONET_DATA*)busses[busnumber].driverdata)

static int init_gl_Loconet(struct _GLSTATE *);
static int init_ga_Loconet(struct _GASTATE *);

int readConfig_LOCONET(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
  xmlNodePtr child = node->children;

  busses[busnumber].type = SERVER_LOCONET;
  busses[busnumber].init_func = &init_bus_Loconet;
  busses[busnumber].term_func = &term_bus_Loconet;
  busses[busnumber].thr_func = &thr_sendrec_Loconet;
  busses[busnumber].init_gl_func = &init_gl_Loconet;
  busses[busnumber].init_ga_func = &init_ga_Loconet;
  busses[busnumber].driverdata = malloc(sizeof(struct _LOCONET_DATA));

  __loconet->number_fb = 2048; /* max addr for OPC_INPUT_REP (10+1 bit) */
  
  busses[ busnumber ].baudrate = B57600;
  strcpy(busses[busnumber].description, "FB POWER DESCRIPTION");

  while (child)
  {
    if (xmlStrncmp(child->name, BAD_CAST "text", 4) == 0)
    {
      child = child -> next;
      continue;
    }
    child = child -> next;
  } // while
  
  if (init_FB(busnumber, __loconet->number_fb))
  {
    DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
  }

  return(1);
}

static int init_lineLoconet (int bus)
{
  int fd;
  struct termios interface;

  fd = open( busses[ bus ].device, O_RDWR );
  if ( fd == -1 )
  {
    DBG(bus, DBG_ERROR, "Sorry, couldn't open device.\n");
    return 1;
  }
  busses[ bus ].fd = fd;
  tcgetattr( fd, &interface );
  interface.c_oflag = ONOCR;
  interface.c_cflag = CS8 | CRTSCTS | CSTOPB | CLOCAL | CREAD | HUPCL;
  interface.c_iflag = IGNBRK;
  interface.c_lflag = IEXTEN;
  cfsetispeed( &interface, busses[ bus ].baudrate );
  cfsetospeed( &interface, busses[ bus ].baudrate );
  interface.c_cc[ VMIN ] = 0;
  interface.c_cc[ VTIME ] = 1;
  tcsetattr( fd, TCSANOW, &interface );
  return 1;
}

int term_bus_Loconet(int bus)
{
  DBG(bus, DBG_INFO, "loconet bus %d terminating", bus);
  return 0;
}

/**
 * initGL: modifies the gl data used to initialize the device

 */
static int init_gl_Loconet(struct _GLSTATE *gl) {
  return SRCP_OK;
}

/**
 * initGA: modifies the ga data used to initialize the device

 */
static int init_ga_Loconet(struct _GASTATE *ga) {
    return SRCP_OK;
}

/* Initialisiere den Bus, signalisiere Fehler */
/* Einmal aufgerufen mit busnummer als einzigem Parameter */
/* return code wird ignoriert (vorerst) */
int init_bus_Loconet(int i)
{
  DBG(i, DBG_INFO, "loconet init: bus #%d, debug %d", i, busses[i].debuglevel);
  if(busses[i].debuglevel<=5)
  {
    DBG(i, DBG_INFO, "loconet bus %d open device %s", i, busses[i].device);
    init_lineLoconet(i);
  }
  else
  {
    busses[i].fd = -1;
  }
  DBG(i, DBG_INFO, "loconet bus %d init done", i);
  return 0;
}

static int ln_read(int fd, unsigned char *cmd, int len) {
    fd_set fds;
    struct timeval t = {0,0};
    int retval;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    retval = select(fd + 1, &fds, NULL, NULL, &t);
    if (retval > 0 && FD_ISSET(fd, &fds))	{
	/* read data from locobuffer */
	int total, pktlen;
	unsigned char c;
	ioctl(fd, FIONREAD, &total);
	/* read exactly one loconet packet; there must be at least one
	   character due to select call result */
	/* a valid loconet packet starts with a byte >= 0x080
	   and contains one or more bytes <0x80.
	*/
	do {
	    read(fd, &c, 1);
    	} while ( c < 0x80);
	switch (c & 0x06) {
	    case 0: pktlen = 2; break;
	    case 2: pktlen = 4; break;
	    case 4: pktlen = 6; break;
	    case 6: 
		read(fd, &pktlen, 1);
		break;
	}
	cmd[0] = c;
	read(fd, &cmd[1], pktlen - 1);
	retval = pktlen;
    }
    return retval;
}

static int ln_write(int fd, const char *cmd, int len) {
    int i;
    for(i=0; i<len; i++) {
	writeByte(fd, cmd[i], 1);
    }
    return 0;
}

static int ln_checksum(int len, char *buf) {
    int chksum = 0xff; 
    int i;
    for(i = 0; i < len; i++) {
        chksum ^= buf[i];
    }
    return chksum;
}

void* thr_sendrec_Loconet (void *v)
{
  int bus = (int) v;
  unsigned char ln_packet[128]; /* max length is coded with 7 bit */
  int ln_packetlen=2;
  unsigned int addr;
  int value;
  
  DBG(bus, DBG_INFO, "loconet started, bus #%d, %s", bus, busses[bus].device);

  while (1) {
    busses[bus].watchdog = 1;
    if( (ln_packetlen = ln_read(busses[bus].fd, ln_packet, sizeof(ln_packet)))>0) {
	/* process data packet from loconet*/
	DBG(bus, DBG_DEBUG, "got ln packet with 0x%x OPC %d bytes", ln_packet[0], ln_packetlen);
	switch (ln_packet[0]) {
	    case 0xb2: /* OPC_INPUT_REP */
		addr = ln_packet[1] | ((ln_packet[2] & 0xf) <<7);
		addr = addr  | (((ln_packet[2] & 0x2) >>1) << 8);
		value = (ln_packet[2] & 0x10) >> 4;
		DBG(bus, DBG_DEBUG, "OPC_INPUT_REP: %d: %d", addr, value);
		updateFB(bus, addr, value);
		break;
	}
    }
    if(busses[bus].power_changed==1) {
      char msg[110];
      ln_packet[0] = 0x83 - busses[bus].power_state;
      ln_packetlen = 2;
      busses[bus].power_changed = 0;
      infoPower(bus, msg);
      queueInfoMessage(msg);
    }
    if(busses[bus].power_state==0) {
          usleep(1000);
          continue;
    }
    ln_packet[ln_packetlen] = ln_checksum(ln_packetlen, ln_packet);
    ln_write(busses[bus].fd, ln_packet, ln_packetlen);
    usleep(1000);
  }
}
