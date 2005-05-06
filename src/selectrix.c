/* cvs: $Id$             */

/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 1, 2005. (c) Gerard van der Sel, 2005-2006.
 *
 * Version 0.1: Locomotiv ansteurung
 * Version 0.0: Umgezetster file M605X
 */


/* Die Konfiguration des seriellen Ports von M6050emu (D. Schaefer)   */
/* wenngleich etwas verändert, mea culpa..                            */

#include "stdincludes.h"
#include "config-srcpd.h"
#include "io.h"
#include "selectrix.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-info.h"
#include "srcp-srv.h"
#include "srcp-error.h"

/* Macro definition  */
#define __selectrix ((SELECTRIX_DATA*)busses[busnumber].driverdata)

/** readconfig_selectrix: liest den Teilbaum der xml Configuration und parametriert
     den busspezifischen Datenteil, wird von register_bus() aufgerufen */

int readconfig_selectrix(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
  xmlNodePtr child = node->children;
  busses[busnumber].type = SERVER_SELECTRIX;
  busses[busnumber].init_func = &init_bus_SELECTRIX;
  busses[busnumber].term_func = &term_bus_SELECTRIX;
  busses[busnumber].thr_func = &thr_sendrec_SELECTRIX;
  busses[busnumber].init_gl_func = &init_gl_SELECTRIX;
  busses[busnumber].init_ga_func = &init_ga_SELECTRIX;
  busses[busnumber].driverdata = malloc(sizeof(struct _SELECTRIX_DATA));
  busses[ busnumber ].baudrate = B9600;
  __selectrix->number_fb = 1;
  __selectrix->number_ga = 1;
  __selectrix->number_gl = 1; 
  __selectrix->number_adres = 112;  /* total of fb + ga + gl max 112 oder 104 */
  __selectrix->flags = 0;
  strcpy(busses[busnumber].description, "GA GL FB POWER LOCK DESCRIPTION");

  while (child)
  {
    if(strncmp(child->name, "text", 4)==0)
    {
      child = child -> next;
      continue;
    }

    if (strcmp(child->name, "number_fb") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __m6051->number_fb = atoi(txt);
      free(txt);
    }

    if (strcmp(child->name, "number_gl") == 0)
    {
      char *txt =
      xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __m6051->number_gl = atoi(txt);
      free(txt);
    }

    if (strcmp(child->name, "number_ga") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __m6051->number_ga = atoi(txt);
      free(txt);
    }

    if (strcmp(child->name, "mode_cc2000") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      if (strcmp(txt, "yes") == 0)
      {
        __SELECTRIX->flags |= CC2000_MODE;
        __selectrix->number_adres = 104;     /* Last 8 adresses for the CC2000 */
        strcpy(busses[busnumber].description, "GA GL FB SM POWER LOCK DESCRIPTION");
      }
      free(txt);
    }

    child = child->next;
  }
  /* Initialise the array's if there are not to much adresses used */
  if ((__selectrix->number_fb + __selectrix->number_ga + __selectrix->number_gl) >  __selectrix->number_adres )
  {
    if(init_GA(busnumber, __selectrix->number_ga))
    {
      __selectrix->number_ga = 0;
      DBG(busnumber, DBG_ERROR, "Can't create array for assesoirs");
    }

    if(init_GL(busnumber, __selectrix->number_gl))
    {
      __selectrix->number_gl = 0;
      DBG(busnumber, DBG_ERROR, "Can't create array for locomotivs");
    }
    if(init_FB(busnumber, __selectrix->number_fb * 8))
    {
      __selectrix->number_fb = 0;
      DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
    }
  }
  else
  {
    __selectrix->number_ga = 0;
    __selectrix->number_gl = 0;
    __selectrix->number_fb = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for locomotivs, assesoirs and feedback");
  }
  return 0;
}


/*******************************************************
 *     SERIELLE SCHNITTSTELLE KONFIGURIEREN
 *******************************************************
 */
static int init_lineSelectrix(int bus) {
  int FD;
  struct termios interface;

  if (busses[bus].debuglevel>0)
  {
    DBG(bus, DBG_INFO, "Opening aelectrix: %s", busses[bus].device);
  }
  if ((FD = open(busses[bus].device, O_RDWR | O_NONBLOCK)) == -1)
  {
    DBG(bus, DBG_FATAL, "couldn't open device %s.", busses[bus].device);
    return -1;
  }
  tcgetattr(FD, &interface);
#ifdef linux
  interface.c_cflag = CS8 | CRTSCTS | CREAD | CSTOPB;
  interface.c_oflag = ONOCR | ONLRET;
  interface.c_oflag &= ~(OLCUC | ONLCR | OCRNL);
  interface.c_iflag = IGNBRK | IGNPAR;
  interface.c_iflag &= ~(ISTRIP | IXON | IXOFF | IXANY);
  interface.c_lflag = NOFLSH | IEXTEN;
  interface.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | TOSTOP | PENDIN);

  cfsetospeed(&interface, busses[busnumber].baudrate );
  cfsetispeed(&interface, busses[busnumber].baudrate );
#else
  cfmakeraw(&interface);
  interface.c_ispeed = interface.c_ospeed = busses[busnumber].baudrate ;
  interface.c_cflag = CREAD  | HUPCL | CS8 | CSTOPB | CRTSCTS;
#endif
  tcsetattr(FD, TCSANOW, &interface);
   DBG(bus, DBG_INFO, "Opening Selectrix succeeded FD=%d", FD);
  return FD;
}

int init_bus_Selectrix(int bus) {

  DBG(bus, DBG_INFO," Selectrix  init: debug %d", busses[bus].debuglevel);
  if(busses[bus].debuglevel<=DBG_DEBUG)
  {
    busses[bus].fd = init_lineSelectrix(bus);
  }
  else
  {
    busses[bus].fd = -1;
  }
  DBG(bus, DBG_INFO, "Selectrix init done, fd=%d",  busses[bus].fd);
  DBG(bus, DBG_INFO, "Selectrix: %s",busses[bus].description);
  DBG(bus, DBG_INFO, "Selectrix flags: %d", busses[bus].flags & AUTO_POWER_ON);
  return 0;
}

int term_bus_Selectrix(int bus)
{
  DBG(bus, DBG_INFO, "Selectrix bus term done, fd=%d",  busses[bus].fd);
  return 0;
}

/**
 * initGL: modifies the gl data used to initialize the device
 *
 */
int init_gl_Selectrix(struct _GLSTATE *gl)
{
  if( gl -> protocol != 'S' ) 
    return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
  return ( gl->n_fs == 31) ? SRCP_OK : SRCP_WRONGVALUE;
}

/**
 * initGA: modifies the ga data used to initialize the device

 */
int init_ga_Selectrix(struct _GASTATE *ga) {
  if( ga->protocol != 'S')
    return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
  return SRCP_OK;
}

int readSXbus(int bus, int SXadres)
{
  unsigned char rr;
  int temp;

  /* Make sure the connection is empty */
  ioctl(busses[bus].fd, FIONREAD, &temp);
  while (temp > 0)
  {
    readByte(bus, 0, &rr);
    ioctl(busses[bus].fd, FIONREAD, &temp);
    DBG(bus, DBG_INFO, "ignoring unread byte: %d ", rr);
  }
  /* write SXadres und read commando */
  writeByte(bus, SXread + SXadres, 0);
  /* extra byte for power to receive data */
  writeByte(bus, 0x5A, 0);
  /* receive data */
  readByte(bus, 0, &rr);
  return rr;
}

void writeSXbus(int bus, int SXadres, int SXdata)
{
  /*  */
  writeByte(bus, SXwrite + SXadres, 0);
  /*  */
  writeByte(bus, SXdata, 0);
}

void* thr_sendrec_Selectrix(void *v)
{
  unsigned char SendByte;
  int addr, temp, number_fb;
  char c;
  unsigned char rr;
  struct _GLSTATE gltmp, glakt;
  struct _GASTATE gatmp;
  int bus = (int) v;
  number_fb = ((SELECTRIX_DATA *)busses[bus].driverdata)->number_fb;

  busses[bus].watchdog = 1;
  DBG(bus, DBG_INFO, "Selectrix on bus %d thread started, fd=%d",  bus, busses[bus].fd);
  ioctl(busses[bus].fd, FIONREAD, &temp);
  while (temp > 0)
  {
    readByte(bus, 0, &rr);
    ioctl(busses[bus].fd, FIONREAD, &temp);
    DBG(bus, DBG_INFO, "ignoring unread byte: %d ", rr);
  }
  while (1)
  {
    busses[bus].watchdog = 1;
    /* Start/Stop */
    if ((busses[bus].power_changed)&&(((SELECTRIX_DATA *)busses[bus].driverdata)->flags== CC2000_MODE)
    {
      char msg[1000];
      busses[bus].watchdog = 2;
      SendByte = (busses[bus].power_state) ? 96 : 97;
      writeSXbus(bus, SendByte);
      busses[bus].power_changed = 0;
      infoPower(bus, msg);
      queueInfoMessage(msg);
    }
    /* do nothing, if power off */
    busses[bus].watchdog = 3;
    if(busses[bus].power_state==0)
    {
      busses[bus].watchdog = 4;
      usleep(1000);
      continue;
    }
    /* Lokdecoder */
    busses[bus].watchdog = 5;
    if (!queue_GL_isempty(bus))
    {
      busses[bus].watchdog = 6;
      unqueueNextGL(bus, &gltmp);
      addr = gltmp.id;
      getGL(bus, addr, &glakt);
      if (gltmp.direction == 2)
      {
        gltmp.speed = 0;
        gltmp.direction = !glakt.direction;
      }
      /* Send adresse fur locomotiv */
      /* Geschwindigkeit, Direction, Licht und Function setzen,  */
      c = gltmp.speed + ((gltmp.direction ) ? 0x20 : 0) + ((gltmp.funcs & 0x01) ? 0x40 : 0) + ((gltmp.funcs & 0x02) ? 0x80 : 0);
      writeSXbus(bus, addr, c);
      setGL(bus, addr, gltmp);
    }
    busses[bus].watchdog = 7;
    /* Antriebe (Weichen und Signale) */
    if (!queue_GA_isempty(bus))
    {
      busses[bus].watchdog = 8;
      unqueueNextGA(bus, &gatmp);
      addr = gatmp.id;
      if (gatmp.action == 1)
      {
        setGA(bus, addr, gatmp);
        c = 33 + (gatmp.port ? 0 : 1);
        SendByte = gatmp.id;
        writeByte(bus, c, 0);
        writeByte(bus, SendByte, 0);
      }
    }
    busses[bus].watchdog = 9;
    /* Zuruck meldungen */
    if ( (number_fb>0) 
    {
      busses[bus].watchdog = 10;
      /* Calculate the modul adres */
      /* Lese der SXbus */
      rr = readSXbus(bus, modul);
      setFBmodul(bus, modul, rr);
    }
    busses[bus].watchdog = 11;
  }
}
