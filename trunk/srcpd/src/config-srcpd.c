/* cvs: $Id$             */

/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * 04.07.2001 Frank Schmischke
 *            Einführung der Konfigurationsdatei
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config-srcpd.h"

/* Folgende Werte lassen sich z.T. verändern, siehe Kommandozeilen/Configdatei */
/* unveränderliche sind mit const markiert                                     */

int debuglevel = 1; /* stellt die Geschwätzigkeit          */

#ifdef TESTMODE
int testmode    = 0;
#endif

/* Anschlußport */
/* globale Informationen für den Netzwerkanschluß */
int CMDPORT      = 12345;          /* default command port         */
int FEEDBACKPORT = 12346;          /* default feedback port        */
int INFOPORT     = 12347;          /* default info port            */

/* Willkommensmeldung */
const char *WELCOME_MSG = "srcpd V0.1; SRCP 0.7.3; Juli 2001; WILLKOMMEN\n";

/* Anschlußport */
char *DEV_COMPORT="/dev/ttyS0";

int working_server  = SERVER_M605X; // Einstellung in welchem Modus srcpd arbeitet
int NUMBER_FB       = 2;            /* Anzahl der Feebackmodule     */
int NUMBER_GA       = 256;          // Anzahl der generischen Accessoires
int NUMBER_GL       = 80;           // Anzahl an Lokadressen
int M6020MODE       = 0;            /* die 6020 mag keine erweiterten Befehle */
int use_i8255       = 0;            // soll auch I8255 genutzt werden ?
int use_watchdog    = 0;            // soll der interne Wachhund genutzt werden ?
int restore_com_parms = 0;          // sollen die com-port Einstellungen gesichert werden?

// Lesen der Konfigurationsdatei /usr/local/etc/srcpd.conf bzw. /etc/srcpd.conf
void readConfig()
{
  FILE *fp;
  int i;
  unsigned char cwert;
  char *offset;
  char buffer[5000];

  fp = fopen("/usr/local/etc/srcpd.conf", "r");
  if(fp == 0)
    fp = fopen("/usr/etc/srcpd.conf", "r");
  if(fp == 0)
    fp = fopen("/etc/srcpd.conf", "r");
  if(fp != 0)
  {
    while(feof(fp)==0)
    {
      i = 0;
      cwert = 2;
      while((cwert != 0x0A) && (i < 4999))
      {
        cwert = fgetc(fp);
        if(cwert != 0x0A)
          buffer[i++] = cwert;
      }
      buffer[i] = '\000';
      if(buffer[0] != '#')
      {
        if(strncmp(buffer, "baseport=", 9) == 0)
        {
          offset = &buffer[9];
          CMDPORT = atoi(offset);
          FEEDBACKPORT = CMDPORT + 1;
          INFOPORT     = CMDPORT + 2;
        }
        if(strncmp(buffer, "comport=", 8) == 0)
        {
          offset = &buffer[8];
          strcpy(DEV_COMPORT, offset);
        }
        if(strncmp(buffer, "FB-modules=", 11) == 0)
        {
          offset = &buffer[11];
          NUMBER_FB = atoi(offset);
        }
        if(strncmp(buffer, "maximum_adress_for_locomotiv=", 29) == 0)
        {
          offset = &buffer[29];
          NUMBER_GL = atoi(offset);
        }
        if(strncmp(buffer, "maximum_adress_for_accessoires=", 31) == 0)
        {
          offset = &buffer[31];
          NUMBER_GA = atoi(offset);
        }
        if(strncmp(buffer, "use_i8255=", 10) == 0)
        {
          offset = &buffer[10];
          if(strcmp(offset, "yes") == 0)
            use_i8255 = 1;
        }
        if(strncmp(buffer, "use_watchdog=", 13) == 0)
        {
          offset = &buffer[13];
          if(strcmp(offset, "yes") == 0)
            use_watchdog = 1;
        }
        if(strncmp(buffer, "server=", 7) == 0)
        {
          offset = &buffer[7];
//          if(strcmp(offset, "DDL") == 0)
//            working_server = SERVER_DDL;
          if(strcmp(offset, "M605X") == 0)
            working_server = SERVER_M605X;
          if(strcmp(offset, "IB") == 0)
            working_server = SERVER_IB;
//          if(strcmp(offset, "LI100") == 0)
//            working_server = SERVER_LI100;
        }
        if(strncmp(buffer, "use_M6020=", 10) == 0)
        {
          offset = &buffer[10];
          if(strcmp(offset, "yes") == 0)
            M6020MODE = 1;
        }
        if(strncmp(buffer, "restore_com_parms=", 18) == 0)
        {
          offset = &buffer[18];
          if(strcmp(offset, "yes") == 0)
            restore_com_parms = 1;
        }

      }
    }
    fclose(fp);
  }
}
