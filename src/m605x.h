/* cvs: $Id$             */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _M605X_H
#define _M605X_H

#include <libxml/tree.h>

typedef struct _M6051_DATA {
    int number_fb;
    int number_ga;
    int number_gl;
    int cmd32_pending;
    int flags;
    unsigned int ga_min_active_time;
    unsigned int pause_between_cmd;
    unsigned int pause_between_bytes;
} M6051_DATA;

int readconfig_m605x(xmlDocPtr doc, xmlNodePtr node, int busnumber);

int init_line6051(int bus);
int init_bus_M6051(int bus);
int term_bus_M6051(int bus);
int init_gl_M6051(struct _GLSTATE *gl);
int init_ga_M6051(struct _GASTATE *ga);
int getDescription_M6051(char *reply);
void* thr_sendrec_M6051(void *);

#endif
