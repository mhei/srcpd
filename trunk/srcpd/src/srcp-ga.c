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
#include "srcp-ga.h"

volatile struct _GA ga[MAXGAS];   // soviele Generic Accessoires gibts
volatile struct _GA nga[50];      // max. 50 Änderungen puffern, neue Werte noch nicht gesendet
volatile struct _GA oga[50];      // manuelle Änderungen
volatile struct _GA tga[50];      // max. 50 Änderungen puffern, neue Werte sind aktiv, warten auf inaktiv

volatile int commands_ga  = 0;
volatile int sending_ga    = 0;

extern int working_server;
extern int NUMBER_GA;

/* setze den Schaltdekoder, einige wenige Prüfungen, max. 2/3 Sekunde warten */
int setGA(char *prot, int addr, int port, int aktion, long activetime)
{
  int i;
  int status;
  struct timeval akt_time;

  status = 0;
  syslog(LOG_INFO, "in setGA für %i", addr);
  if((addr > 0) && (addr <= NUMBER_GA))
  {
    for(i=0;i<1000;i++)          // warte auf Freigabe
    {
      if(sending_ga == 0)        // es wird nichts gesendet
        break;
      usleep(100);
    }

    for(i=0;i<50;i++)
    {
      if(nga[i].id == addr)    // alten Auftrag wieder löschen
      {
        nga[i].id = 0;
        break;
      }
    }
    if(i == 50)
    {
      for(i=0;i<50;i++)          // suche freien Platz in Liste
      {
        if(nga[i].id == 0)
          break;
      }
    }
  
    if(i < 50)
    {
      strcpy((void *) nga[i].prot, prot);
      nga[i].action     = aktion;
      nga[i].port       = port;
      nga[i].activetime = activetime;
      gettimeofday(&akt_time, NULL);
      nga[i].tv[port]   = akt_time;
      nga[i].id         = addr;
      commands_ga = 1;
      status = 1;
      syslog(LOG_INFO, "GA %i Port %i Action %i Zeit %ld auf Position %i", addr, port, aktion, activetime, i);
    }
  }
  return status;
}

int getGA(char *prot, int addr, struct _GA *a)
{
  if((addr > 0) && (addr <= NUMBER_GA))
  {
    *a = ga[addr];
    return 0;
  }
  else
  {
    return 1;
  }
}

int infoGA(struct _GA a, char *msg)
{
  sprintf(msg, "INFO GA %s %d %d %d %ld\n", a.prot, a.id, a.port, a.action, a.activetime);
  return 0;
}    

int cmpGA(struct _GA a, struct _GA b)
{
  return ((a.action == b.action) &&
      (a.port   == b.port));
}

void initGA()
{
  int i;
  for(i=0; i<MAXGAS;i++)
  {
    strcpy((void *) ga[i].prot, "M");
    ga[i].id = i;
  }
  for(i=0; i<50;i++)
  {
    strcpy((void *) nga[i].prot, "M");
    nga[i].id = 0;
    strcpy((void *) oga[i].prot, "M");
    oga[i].id = 0;
    strcpy((void *) tga[i].prot, "M");
    tga[i].id = 0;
  }
}
