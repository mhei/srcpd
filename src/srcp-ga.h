/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCPGA_H_
#define _SRCPGA_H_

#include <sys/time.h>

#define MAXGAS 1024
#define MAXGAPORT 2

/* Schaltdekoder */
struct _GA
{
  char prot[5];         /* Protokoll      */
  int id;               /* Der Identifier */
  int port;             /* Portnummer     */
  int action;           /* 0,1,2,3...     */
  long activetime;      /* Aktivierungszeit in msec bis das 32 Kommando kommen soll */
  struct timeval tv[MAXGAPORT]; /* Zeitpunkt der letzten Aktivierungen, ein Wert pro Port   */
  struct timeval t;     // Auschaltzeitpunkt
};

void initGA(void);
int setGA(char *prot, int addr, int port, int aktion, long activetime);
int getGA(char *prot, int addr, struct _GA *a);
int infoGA(struct _GA a, char *msg);
int cmpGA(struct _GA a, struct _GA b);

#endif
