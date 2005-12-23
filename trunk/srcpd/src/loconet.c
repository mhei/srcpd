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
  
  busses[ busnumber ].baudrate = B57600;

  busses[busnumber].driverdata = malloc(sizeof(struct _LOCONET_DATA));
  strcpy(busses[busnumber].description, "GA FB POWER LOCK DESCRIPTION");

  while (child)
  {
    if (xmlStrncmp(child->name, (const xmlChar *) "text", 4) == 0)
    {
      child = child -> next;
      continue;
    }
    child = child -> next;
  } // while

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
  if(busses[i].debuglevel==0)
  {
   DBG(i, DBG_INFO, "loconet bus %d open device %s (not really!)", i, busses[i].device);
    busses[i].fd = init_lineLoconet(i);
  }
  else
  {
    busses[i].fd = -1;
  }
  DBG(i, DBG_INFO, "loconet bus %d init done", i);
  return 0;
}

void* thr_sendrec_Loconet (void *v)
{
  int bus = (int) v;

  DBG(bus, DBG_INFO, "loconet started, bus #%d, %s", bus, busses[bus].device);

  while (1) {
    /* read all data from locobuffer (one message per cycle)
     * process it and send one message to locobuffer (if ready)
    */
  busses[bus].watchdog = 1;
    if(busses[bus].power_changed==1) {
      char msg[110];
      busses[bus].power_changed = 0;
      infoPower(bus, msg);
      queueInfoMessage(msg);
    }
    if(busses[bus].power_state==0) {
          usleep(1000);
          continue;
    }
    usleep(1000);
  }
}
