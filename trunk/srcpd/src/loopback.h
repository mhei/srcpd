/* $Id$ */

#ifndef _LOOPBACK_H
#define _LOOPBACK_H

typedef struct _LOOPBACK_DATA {
    int number_ga;
    int number_gl;
    int number_fb;
} LOOPBACK_DATA;

int readconfig_LOOPBACK(xmlDocPtr doc, xmlNodePtr node, long int busnumber);
long int init_bus_LOOPBACK(long );
long int term_bus_LOOPBACK(long );
long int init_gl_LOOPBACK(struct _GLSTATE *);
long int init_ga_LOOPBACK(struct _GASTATE *);
int getDescription_LOOPBACK(char *reply);
void* thr_sendrec_LOOPBACK(void *);

#endif
