/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * This software is published under the restrictions of the 
 * GNU License Version2
 *
 * 04.07.2001 Frank Schmischke
 *            - Feld f|r Vormerkungen wurde auf 50 reduziert
 *            - Position im Feld f|r Vormerkung ist jetzt unabhngig von der
 *              Decoderadresse
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/time.h>

#include "config-srcpd.h"
#include "srcp-error.h"
#include "srcp-ga.h"

volatile struct _GA ga[MAX_BUSSES][MAXGAS];   // soviele Generic Accessoires gibts
volatile struct _GA nga[MAX_BUSSES][50];      // max. 50 Änderungen puffern, neue Werte noch nicht gesendet
volatile struct _GA oga[MAX_BUSSES][50];      // manuelle Änderungen
volatile struct _GA tga[MAX_BUSSES][50];      // max. 50 Änderungen puffern, neue Werte sind aktiv, warten auf inaktiv

/* setze den Schaltdekoder, einige wenige Prüfungen, max. 2/3 Sekunde warten */
int setGA(int bus, int addr, int port, int aktion, long activetime)
{
  int i;
  int status;
  struct timeval akt_time;

  status = 0;
  syslog(LOG_INFO, "in setGA für %i", addr);
  if((addr > 0) && (addr <= busses[bus].number_ga))
  {
    for(i=0;i<1000;i++)          // warte auf Freigabe
    {
      if(busses[bus].sending_ga == 0)        // es wird nichts gesendet
        break;
      usleep(100);
    }

    for(i=0;i<50;i++)
    {
      if(nga[bus][i].id == addr)    // alten Auftrag wieder löschen
      {
        nga[bus][i].id = 0;
        break;
      }
    }
    if(i == 50)
    {
      for(i=0;i<50;i++)          // suche freien Platz in Liste
      {
        if(nga[bus][i].id == 0)
          break;
      }
    }
  
    if(i < 50)
    {
      nga[bus][i].action     = aktion;
      nga[bus][i].port       = port;
      nga[bus][i].activetime = activetime;
      gettimeofday(&akt_time, NULL);
      nga[bus][i].tv[port]   = akt_time;
      nga[bus][i].id         = addr;
      busses[bus].command_ga = 1;
      status = 1;
      syslog(LOG_INFO, "GA %i Port %i Action %i Zeit %ld auf Position %i", addr, port, aktion, activetime, i);
    }
  }
  return status;
}

int getGA(int bus, int addr, struct _GA *a)
{
  if((addr > 0) && (addr <= busses[bus].number_ga))
  {
    *a = ga[bus][addr];
    return SRCP_OK;
  }
  else
  {
    return SRCP_NODATA;
  }
}

int infoGA(int bus, int addr, char *msg)
{
  if((addr > 0) && (addr <= busses[bus].number_ga))
  {
    sprintf(msg, "INFO %d GA %d %d %d %ld", bus, addr, ga[bus][addr].port, ga[bus][addr].action, ga[bus][addr].activetime);
    return SRCP_OK;
  } else {
    return SRCP_NODATA;
  }
  return 0;
}    

int cmpGA(struct _GA a, struct _GA b)
{
  return ((a.action == b.action) &&
      (a.port   == b.port));
}

int initGA()
{
  int bus, i;
  for(bus=0; bus<MAX_BUSSES; bus++) {
    for(i=0; i<MAXGAS;i++)
    {
      ga[bus][i].id = i;
    }
    for(i=0; i<50;i++)
    {
      nga[bus][i].id = 0;
      oga[bus][i].id = 0;
      tga[bus][i].id = 0;
    }
  }
  return SRCP_OK;
}
