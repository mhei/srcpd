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
#include "srcp-info.h"
#include "srcp-error.h"

#define __loopback ((LOOPBACK_DATA*)busses[busnumber].driverdata)

int readconfig_loopback(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
  xmlNodePtr child = node->children;

  busses[busnumber].type = SERVER_LOOPBACK;
  busses[busnumber].init_func = &init_bus_Loopback;
  busses[busnumber].term_func = &term_bus_Loopback;
  busses[busnumber].thr_func = &thr_sendrec_Loopback;
  busses[busnumber].init_gl_func = &init_gl_Loopback;
  busses[busnumber].init_ga_func = &init_ga_Loopback;

  busses[busnumber].driverdata = malloc(sizeof(struct _LOOPBACK_DATA));
  strcpy(busses[busnumber].description, "GA GL FB POWER LOCK DESCRIPTION");

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

  if(init_GA(busnumber, __loopback->number_ga))
  {
    __loopback->number_ga = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for accessoires");
  }

  if(init_FB(busnumber, __loopback->number_fb))
  {
    __loopback->number_fb = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
  }
  return(1);
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

/**
 * initGL: modifies the gl data used to initialize the device

 */
int init_gl_Loopback(struct _GLSTATE *gl) {
    switch(gl->protocol) {
	case 'L':
	case 'P':
	    return SRCP_OK;
	    break;
	case 'M':
	  switch(gl->protocolversion) {
	    case 1:
		return ( gl -> n_fs == 14) ? SRCP_OK : SRCP_WRONGVALUE;
		break;
	    case 2:
		return ( (gl -> n_fs == 14) ||
		         (gl -> n_fs == 27) ||
			 (gl -> n_fs == 28) ) ? SRCP_OK : SRCP_WRONGVALUE;
		break;
    	  }
	  return SRCP_WRONGVALUE;
          break;
	case 'N':
          switch(gl->protocolversion) {
	    case 1:
		return ( (gl -> n_fs == 14) ||
			 (gl -> n_fs == 128) ) ? SRCP_OK : SRCP_WRONGVALUE;
		break;
	    case 2:
		return ( (gl -> n_fs == 14) ||
			 (gl -> n_fs == 128) ) ? SRCP_OK : SRCP_WRONGVALUE;
		break;
    	  }
	  return SRCP_WRONGVALUE;
          break;
       }
  return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
}

/**
 * initGA: modifies the ga data used to initialize the device

 */
int init_ga_Loopback(struct _GASTATE *ga) {
  if( (ga -> protocol == 'M') ||  (ga -> protocol == 'N') ||  (ga -> protocol == 'P') )
      return SRCP_OK;
  return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
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

    if (!queue_GL_isempty(bus))  {
        unqueueNextGL(bus, &gltmp);
        addr = gltmp.id;
        getGL(bus, addr, &glakt);

        if (gltmp.direction == 2)  {
          gltmp.speed = 0;
          gltmp.direction = !glakt.direction;
        }
        setGL(bus, addr, gltmp);
    }
    busses[bus].watchdog = 4;
    if (!queue_GA_isempty(bus)) {
          unqueueNextGA(bus, &gatmp);
          addr = gatmp.id;
          if(gatmp.action == 1) {
            gettimeofday(&gatmp.tv[gatmp.port], NULL);
          }
          setGA(bus, addr, gatmp);
          busses[bus].watchdog = 6;
    }
    usleep(1000);
  }
}