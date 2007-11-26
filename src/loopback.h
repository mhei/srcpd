/* $Id$ */

#ifndef _LOOPBACK_H
#define _LOOPBACK_H

typedef struct _LOOPBACK_DATA {
    int number_ga;
    int number_gl;
    int number_fb;
} LOOPBACK_DATA;

int readconfig_LOOPBACK(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber);
int init_bus_LOOPBACK(bus_t );
int term_bus_LOOPBACK(bus_t);
int init_gl_LOOPBACK(gl_state_t *);
int init_ga_LOOPBACK(ga_state_t *);
int getDescription_LOOPBACK(char *reply);
void* thr_sendrec_LOOPBACK(void *);

#endif
