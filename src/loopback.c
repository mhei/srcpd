/* $Id$ */

/* loopback: simple Busdriver without any hardware.
 *
 */
 
#include "stdincludes.h"

#include "config-srcpd.h"
#include "io.h"
#include "loopback.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-srv.h"

#define __loopback ((LOOPBACK_DATA*)busses[busnumber].driverdata)

void readconfig_loopback(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
  xmlNodePtr child = node->children;

  busses[busnumber].type = SERVER_LOOPBACK;
  busses[busnumber].init_func = &init_bus_Loopback;
  busses[busnumber].term_func = &term_bus_Loopback;
  busses[busnumber].thr_func = &thr_sendrec_Loopback;
  busses[busnumber].driverdata = malloc(sizeof(struct _LOOPBACK_DATA));
  strcpy(busses[busnumber].description, "GA GL FB POWER");

  __loopback->number_fb = 0;  /* max 31 */
  __loopback->number_ga = 256;
  __loopback->number_gl = 80;

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
      __loopback->number_fb = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "number_gl") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __loopback->number_gl = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "number_ga") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __loopback->number_ga = atoi(txt);
      free(txt);
    }
    child = child -> next;
  } // while

  if(init_GL(busnumber, __loopback->number_gl))
  {
    __loopback->number_gl = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for locomotivs");
  }
  if(init_FB(busnumber, __loopback->number_fb))
  {
    __loopback->number_fb = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
  }
}

int init_lineLoopback (char *name)
{
  int FD;
  FD = -1;
  return FD;
}

int term_bus_Loopback(int bus)
{
  DBG(bus, DBG_INFO, "loopback bus %d terminating", bus);
  return 0;
}

/* Initialisiere den Bus, signalisiere Fehler */
/* Einmal aufgerufen mit busnummer als einzigem Parameter */
/* return code wird ignoriert (vorerst) */
int init_bus_Loopback(int i)
{
  DBG(i, DBG_INFO,"loopback init: bus #%d, debug %d", i, busses[i].debuglevel);
  if(busses[i].debuglevel==0)
  {
   DBG(i, DBG_INFO, "loopback bus %d open device %s (not really!)", i, busses[i].device);
    busses[i].fd = init_lineLoopback(busses[i].device);
  }
  else
  {
    busses[i].fd = -1;
  }
  DBG(i, DBG_INFO, "loopback init done");
  return 0;
}

void* thr_sendrec_Loopback (void *v)
{
  struct _GLSTATE gltmp, glakt;
  struct _GASTATE gatmp;
  int addr;
  int bus = (int) v;
  
  DBG(bus, DBG_INFO, "loopback started, bus #%d, %s", bus, busses[bus].device);

  busses[bus].watchdog = 1;

  while (1) {
      if (!queue_GL_isempty(bus))  {
        unqueueNextGL(bus, &gltmp);
        addr = gltmp.id;
        getGL(bus, addr, &glakt);

        if (gltmp.direction == 2)  {
          gltmp.speed = 0;
          gltmp.direction = !glakt.direction;
        }
        setGL(bus, addr, gltmp, 1);
      }
      busses[bus].watchdog = 4;
      if (!queue_GA_isempty(bus)) {
          unqueueNextGA(bus, &gatmp);
          addr = gatmp.id;
          if(gatmp.action == 1) {
            gettimeofday(&gatmp.tv[gatmp.port], NULL);
          }
          setGA(bus, addr, gatmp, 1);
          busses[bus].watchdog = 6;
      }
      usleep(1000);
  }
}
