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
  int id;               /* Der Identifier */
  int port;             /* Portnummer     */
  int action;           /* 0,1,2,3...     */
  long activetime;      /* Aktivierungszeit in msec bis das 32 Kommando kommen soll */
  struct timeval tv[MAXGAPORT]; /* Zeitpunkt der letzten Aktivierungen, ein Wert pro Port   */
  struct timeval t;     // Auschaltzeitpunkt
  int locked_by;     /* Session ID */
};


extern volatile struct _GA ga[MAX_BUSSES][MAXGAS];   // soviele Generic Accessoires gibts
extern volatile struct _GA nga[MAX_BUSSES][50];      // max. 50 Änderungen puffern, neue Werte noch nicht gesendet
extern volatile struct _GA oga[MAX_BUSSES][50];      // manuelle Änderungen
extern volatile struct _GA tga[MAX_BUSSES][50];      // max. 50 Änderungen puffern, neue Werte sind aktiv, warten auf inaktiv

int initGA(int sessionid);
int setGA(int sessionid, int bus, int addr, int port, int aktion, long activetime);
int getGA(int bus, int addr, struct _GA *a);
int infoGA(struct _GA a, char *msg);
int cmpGA(struct _GA a, struct _GA b);

/* used internally */
int updateGA(int bus, int addr, int port, int state);
#endif
