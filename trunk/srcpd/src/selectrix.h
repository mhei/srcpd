/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Gerard van der Sel, 2005-2006.
 *
 */


#ifndef _Selectrix_H
#define _Selectrix_H

#include <libxml/tree.h>

#define CC2000_MODE            0x0001      //! CC2000 can programmmand has restricted bus space
#define SXread                 0x00        /* Lese befehl fur Selectrixbus */
#define SXwrite                0x80        /* Schreib befehl fur Selectrixbus */

typedef struct _SELECTRIX_DATA {
    int number_fb;
    int number_ga;
    int number_gl;
    int number_adres;
    int flags;
    SX_FB_ADRESSEN fb_adressen;
} SELECTRIX_DATA;

int readconfig_selectrix(xmlDocPtr doc, xmlNodePtr node, int busnumber);

int init_lineSelectrix(int bus);
int init_bus_Selectrix(int bus);
int term_bus_Selectrix(int bus);
int init_gl_Selectrix(struct _GLSTATE *gl);
int init_ga_Selectrix(struct _GASTATE *ga);
int init_fb_Selectrix(struct _FBSTATE *fb);
int getDescription_Selectrix(char *reply);
void* thr_sendrec_Selectrix(void *);

#endif
