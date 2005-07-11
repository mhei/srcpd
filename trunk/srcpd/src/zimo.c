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
#include <sys/time.h>
#include <time.h>

#define __zimo ((zimo_DATA*)busses[busnumber].driverdata)

static long tdiff (struct timeval t1, struct timeval t2) {
    return (t2.tv_sec*1000+t2.tv_usec/1000-t1.tv_sec*1000-t1.tv_usec/1000) ;
}

int readanswer(int bus, char cmd, char* buf, int maxbuflen, long timeout_ms){
 int i,status,cnt=0;
 char c,lc=13;
 struct timeval ts,tn;
 
 gettimeofday(&ts,NULL);
 while(1) {
  status = ioctl(busses[bus].fd, FIONREAD, &i);
  if(status<0) return -1;
  if(i)
  {readByte(bus,0,&c);
   DBG(bus, DBG_INFO, "zimo read %02X", c);
   if((lc==10 || lc==13) && c==cmd) cnt=1;
   if(cnt)
   {if(c==10 || c==13) return cnt;
    if(cnt>maxbuflen) return -2;
    buf[cnt-1]=c;
    cnt++;
   }
   //DBG(bus, DBG_INFO, "%ld", tdiff(ts,tn));
   gettimeofday(&tn,NULL);
   if(tdiff(ts,tn)>timeout_ms) return 0;
   lc=c;
  } else
   usleep(1);
 }
}

int readconfig_zimo(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
  xmlNodePtr child = node->children;

  busses[busnumber].type = SERVER_ZIMO;
  busses[busnumber].init_func = &init_bus_zimo;
  busses[busnumber].term_func = &term_bus_zimo;
  busses[busnumber].thr_func = &thr_sendrec_zimo;
  busses[busnumber].driverdata = malloc(sizeof(struct _zimo_DATA));
  strcpy(busses[busnumber].description, "GL POWER LOCK DESCRIPTION");

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
  // TODO: struct _GASTATE gatmp, gaakt;
  int addr, temp, i;
  int bus = (int) v;
  char msg[20];
  char rr;
  char databyte1,  databyte2, databyte3;
  // TODO: unsigned int error, cv, val;
  // TODO: unsigned char databyte, address;
  DBG(bus, DBG_INFO, "zimo started, bus #%d, %s", bus, busses[bus].device);

  busses[bus].watchdog = 1;
  while (1)
  {if(busses[bus].power_changed==1)
   {sprintf(msg, "S%c%c", (busses[bus].power_state) ? 'E' : 'A', 13);
    writeString(bus, msg, 0);
    busses[bus].power_changed = 0;
    infoPower(bus, msg);
    queueInfoMessage(msg);
   }
   if(!queue_GL_isempty(bus))
   {unqueueNextGL(bus, &gltmp);
    addr = gltmp.id;
    getGL(bus, addr, &glakt);
    databyte1 = (gltmp.direction?0:32);
    databyte1 |= (gltmp.funcs & 0x01) ? 16 : 0;
    if(glakt.n_fs == 128) databyte1 |= 12;
    if(glakt.n_fs == 28) databyte1 |= 8;
    if(glakt.n_fs == 14) databyte1 |= 4;
    databyte2 = gltmp.funcs >> 1;
    databyte3 = 0x00;
    if(addr>128)
    {sprintf(msg,"E%04X%c",addr,13);
     DBG(bus, DBG_INFO, "%s", msg);
     writeString(bus, msg, 0);
     addr=0;
     i=readanswer(bus,'E',msg,20,40);
     DBG(bus, DBG_INFO, "readed %d", i);
     switch(i)
     {case 8:sscanf(&msg[1],"%02X",&addr);
             break;
      case 10:sscanf(&msg[3],"%02X",&addr);
             break;
     }
    }
    if(addr)
    {sprintf(msg, "F%c%02X%02X%02X%02X%02X%c", glakt.protocol, addr, gltmp.speed, databyte1, databyte2, databyte3, 13);
     DBG(bus, DBG_INFO, "%s", msg);
     writeString(bus, msg, 0);
     ioctl(busses[bus].fd, FIONREAD, &temp);
     while(temp > 0)
     {readByte(bus, 0, &rr);
      ioctl(busses[bus].fd, FIONREAD, &temp);
      DBG(bus, DBG_INFO, "ignoring unread byte: %d (%c)", rr, rr);
     }
     setGL(bus, addr, gltmp);
    }
   }
   busses[bus].watchdog = 4;
   usleep(10);
  }
}
