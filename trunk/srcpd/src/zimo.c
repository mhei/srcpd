/* $Id$ */

/* zimo: Zimo MX1 Treiber
 *
 */

#include "stdincludes.h"

#include "config-srcpd.h"
#include "io.h"
#include "zimo.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-srv.h"
#include "srcp-info.h"

#define __zimo ((zimo_DATA*)busses[busnumber].driverdata)

int readconfig_zimo(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
  xmlNodePtr child = node->children;

  busses[busnumber].type = SERVER_ZIMO;
  busses[busnumber].init_func = &init_bus_zimo;
  busses[busnumber].term_func = &term_bus_zimo;
  busses[busnumber].thr_func = &thr_sendrec_zimo;
  busses[busnumber].driverdata = malloc(sizeof(struct _zimo_DATA));
  strcpy(busses[busnumber].description, "GA GL POWER LOCK DESCRIPTION");

  __zimo->number_fb = 0;  /* max 31 */
  __zimo->number_ga = 256;
  __zimo->number_gl = 80;

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
      __zimo->number_fb = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "p_time") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
     set_min_time(busnumber, atoi(txt));
      free(txt);
    }

    if (strcmp(child->name, "number_gl") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __zimo->number_gl = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "number_ga") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __zimo->number_ga = atoi(txt);
      free(txt);
    }
    child = child -> next;
  } // while

  if(init_GL(busnumber, __zimo->number_gl))
  {
    __zimo->number_gl = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for locomotivs");
  }

  if(init_GA(busnumber, __zimo->number_ga))
  {
    __zimo->number_ga = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for accessoires");
  }

  if(init_FB(busnumber, __zimo->number_fb))
  {
    __zimo->number_fb = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
  }
  return(1);
}

int init_linezimo (char *name)
{
    int fd;
  struct termios interface;

  printf("try opening serial line %s for 9600 baud\n", name);
  fd = open(name, O_RDWR);
  if (fd == -1)
  {
    printf("dammit, couldn't open device.\n");
  }
  else
  {
    tcgetattr(fd, &interface);
    interface.c_oflag = ONOCR;
    interface.c_cflag = CS8 | CRTSCTS | CSTOPB | CLOCAL | CREAD | HUPCL;
    interface.c_iflag = IGNBRK;
    interface.c_lflag = IEXTEN;
    cfsetispeed(&interface, B9600);
    cfsetospeed(&interface, B9600);
    interface.c_cc[VMIN] = 0;
    interface.c_cc[VTIME] = 1;
    tcsetattr(fd, TCSANOW, &interface);
  }
  return fd;
}

int term_bus_zimo(int bus)
{
  DBG(bus, DBG_INFO, "zimo bus %d terminating", bus);
  return 0;
}

/* Initialisiere den Bus, signalisiere Fehler */
/* Einmal aufgerufen mit busnummer als einzigem Parameter */
/* return code wird ignoriert (vorerst) */
int init_bus_zimo(int i)
{
  DBG(i, DBG_INFO,"zimo init: bus #%d, debug %d, device %s", i, busses[i].debuglevel, busses[i].device);
  busses[i].fd = init_linezimo(busses[i].device);
  DBG(i, DBG_INFO, "zimo init done");
  return 0;
}

void* thr_sendrec_zimo (void *v)
{
  struct _GLSTATE gltmp, glakt;
  struct _GASTATE gatmp, gaakt;
  int addr, temp, i;
  int bus = (int) v;
  char msg[20];
  DBG(bus, DBG_INFO, "zimo started, bus #%d, %s", bus, busses[bus].device);

  busses[bus].watchdog = 1;
  while (1) {
    if(busses[bus].power_changed==1) {
      sprintf(msg, "S%c%c", (busses[bus].power_state) ? 'E' : 'A', 13);
      writeString(bus, msg, 0);
      busses[bus].power_changed = 0;
      infoPower(bus, msg);
      queueInfoMessage(msg);
    }
    sprintf(msg, "Z%c", 13);
    writeString(bus, msg, 0);
    ioctl(busses[bus].fd, FIONREAD, &temp);
    for(i=0; i<temp; i++) {
  char rr;
  readByte(bus, 0, &rr);
  msg[i]=rr;
  msg[i+1]=0x00;
    }
    DBG(bus, DBG_DEBUG, "status response: %s ", msg);

    if(busses[bus].power_state==0) {
          usleep(1000);
          continue;
    }

    if (!queue_GL_isempty(bus))  {
        char databyte1,  databyte2, databyte3;

        unqueueNextGL(bus, &gltmp);
        addr = gltmp.id;
        getGL(bus, addr, &glakt);
        databyte1 = (gltmp.direction?0:32);
        databyte1 |= (gltmp.funcs & 0x01) ? 16 : 0;
        if(glakt.n_fs == 128)
          databyte1 |= 12;
        if(glakt.n_fs == 28)
          databyte1 |= 8;
        if(glakt.n_fs == 14)
          databyte1 |= 4;
        databyte2 = gltmp.funcs >> 1;
        databyte3 = 0x00;

        sprintf(msg, "F%c%02X%02X%02X%02X%02X%c", glakt.protocol, addr, gltmp.speed, databyte1, databyte2, databyte3, 13);
        DBG(bus, DBG_DEBUG, "%s", msg);
        writeString(bus, msg, 0);
        ioctl(busses[bus].fd, FIONREAD, &temp);
        while (temp > 0)  {
            char rr;
          readByte(bus, 0, &rr);
          ioctl(busses[bus].fd, FIONREAD, &temp);
          DBG(bus, DBG_INFO, "ignoring unread byte: %d (%c)", rr, rr);
       }
       setGL(bus, addr, gltmp);
    }
    busses[bus].watchdog = 4;
    if (!queue_GA_isempty(bus)) {
          unsigned char databyte, address;
          unqueueNextGA(bus, &gatmp);
          addr = gatmp.id;
          getGA(bus, addr, &gaakt);
          // "M(N|M|Z)<addr><databyte>#13"
          databyte = (addr % 4)+1 + (gatmp.action?8:0);
          address  = addr / 4 + 1;
          DBG(bus,DBG_INFO, "Protocol= %c, modul=%d, databyte=%d: ein/aus=%d subadresse=%d port=%d ", gaakt.protocol, address, databyte,
            gatmp.action, (addr % 4)+1, gatmp.port);
          sprintf(msg, "M%c%02X%02X%c", gaakt.protocol, address, databyte, 13);
          DBG(bus, DBG_INFO, "%s", msg);
          writeString(bus, msg, 0);
        ioctl(busses[bus].fd, FIONREAD, &temp);
        while (temp > 0)  {
            char rr;
          readByte(bus, 0, &rr);
          ioctl(busses[bus].fd, FIONREAD, &temp);
          DBG(bus, DBG_INFO, "ignoring unread byte: %d (%c)", rr, rr);
       }
          setGA(bus, addr, gatmp);
          busses[bus].watchdog = 6;
      check_reset_fb(bus);
    }

    usleep(1000);
  }
}
