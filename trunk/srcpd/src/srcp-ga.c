/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * This software is published under the restrictions of the 
 * GNU License Version2
 *
 * Ostern 2002, Matthias Trute
 *            - echte Kommandoqueue anstelle der Vormerkungen.
 *
 * 04.07.2001 Frank Schmischke
 *            - Feld für Vormerkungen wurde auf 50 reduziert
 *            - Position im Feld für Vormerkung ist jetzt unabhängig von der
 *              Decoderadresse
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/time.h>
#include <pthread.h>

#include "config-srcpd.h"
#include "srcp-ga.h"
#include "srcp-error.h"

#include "m605x.h"

#define QUEUELEN 500

/* aktueller Stand */
volatile struct _GA ga[MAX_BUSSES][MAXGAS];   // aktueller Stand, mehr gibt es nicht

/* Kommandoqueues pro Bus */
static struct _GA queue[MAX_BUSSES][QUEUELEN];  // Kommandoqueue.
static pthread_mutex_t queue_mutex[MAX_BUSSES];
static int out[MAX_BUSSES], in[MAX_BUSSES];

/* internal functions */
static int queue_len(int bus);
static int queue_isfull(int bus);

/* Übernehme die neuen Angaben für die Lok, einige wenige Prüfungen */
int
queueGA(int bus, int addr, int port, int action, long int activetime)
{
  struct timeval akt_time;
  int number_ga;

  syslog(LOG_INFO, "setga für %i", addr);
  if(busses[bus].type == SERVER_M605X) {
    number_ga =   ( (M6051_DATA *) busses[bus].driverdata)  -> number_ga;
  } else {
    return SRCP_UNSUPPORTEDDEVICEGROUP;
  }
  if ((addr > 0) && (addr <= number_ga) )
  {
    while (queue_isfull(bus))
    {
      usleep(1000);
    }

    pthread_mutex_lock(&queue_mutex[bus]);
    queue[bus][in[bus]].action = action;
    queue[bus][in[bus]].port = port;
    queue[bus][in[bus]].activetime = activetime;
    gettimeofday(&akt_time, NULL);
    queue[bus][in[bus]].tv[port] = akt_time;
    queue[bus][in[bus]].id = addr;
    in[bus]++;
    if (in[bus] == QUEUELEN)
      in[bus] = 0;
  
    pthread_mutex_unlock(&queue_mutex[bus]);
  }
  else
  {
    return -1;
  }
  return SRCP_OK;
}

int
queue_GA_isempty(int bus)
{
  return (in[bus] == out[bus]);
}

static int
queue_len(int bus)
{
  if (in[bus] >= out[bus])
    return in[bus] - out[bus];
  else
    return QUEUELEN + in[bus] - out[bus];
}

/* maybe, 1 element in the queue cannot be used.. */
static int
queue_isfull(int bus)
{
  return queue_len(bus) >= QUEUELEN - 1;
}

/** liefert nächsten Eintrag und >=0, oder -1 */
int
getNextGA(int bus, struct _GA *ga)
{
  if (in[bus] == out[bus])
    return -1;
  *ga = queue[bus][out[bus]];
  return out[bus];
}

/** liefert nächsten Eintrag oder -1, setzt fifo pointer neu! */
int
unqueueNextGA(int bus, struct _GA *ga)
{
  if (in[bus] == out[bus])
    return -1;

  *ga = queue[bus][out[bus]];
  out[bus]++;
  if (out[bus] == QUEUELEN)
    out[bus] = 0;
  return out[bus];
}

int
getGA(int bus, int addr, struct _GA *l)
{
  int number_ga;
  if(busses[bus].type == SERVER_M605X) {
    number_ga =   ( (M6051_DATA *) busses[bus].driverdata)  -> number_ga;
  } else {
    return SRCP_UNSUPPORTEDDEVICEGROUP;
  }

  if((addr>0) && (addr <= number_ga))
  {
    *l = ga[bus][addr];
    return SRCP_OK;
  }
  else
  {
    return SRCP_NODATA;
  }
}

/**

*/
int
setGA(int bus, int addr, struct _GA l)
{
  int number_ga;
  if(busses[bus].type == SERVER_M605X) {
    number_ga =   ( (M6051_DATA *) busses[bus].driverdata)  -> number_ga;
  } else {
    return SRCP_UNSUPPORTEDDEVICEGROUP;
  }

  if((addr>0) && (addr <= number_ga))
  {
    ga[bus][addr] = l;
    gettimeofday(&ga[bus][addr].tv, NULL);
    return SRCP_OK;
  }
  else
  {
    return SRCP_NODATA;
  }
}

int
infoGA(int bus, int addr, char* msg)
{
  int number_ga;
  if(busses[bus].type == SERVER_M605X) {
    number_ga =   ( (M6051_DATA *) busses[bus].driverdata)  -> number_ga;
  } else {
    return SRCP_UNSUPPORTEDDEVICEGROUP;
  }

  if((addr>0) && (addr <= number_ga))
  {
    sprintf(msg, "GA %d %d %d %ld\n", ga[bus][addr].id, ga[bus][addr].port,
      ga[bus][addr].action, ga[bus][addr].activetime);
  }
  else
  {
    return SRCP_NODATA;
  }
  return SRCP_INFO;
}

int
cmpga(struct _GA a, struct _GA b)
{
  return ((a.action == b.action) && (a.port == b.port));
}

int
startup_GA(void)
{
  int bus;
  for(bus=0; bus<MAX_BUSSES; bus++)
  {
    pthread_mutex_init(&queue_mutex[bus], NULL);
    in[bus]=0;
    out[bus]=0;
  }
  return SRCP_OK;
}
