/* $Id$ */

#ifndef _LOOPBACK_H
#define _LOOPBACK_H

typedef struct _LOOPBACK_DATA {
    int number_ga;
    int number_gl;
    int number_fb;
} LOOPBACK_DATA;

int readconfig_LOOPBACK(xmlDocPtr doc, xmlNodePtr node, int busnumber);
int init_bus_LOOPBACK(int );
int term_bus_LOOPBACK(int );
int init_gl_LOOPBACK(struct _GLSTATE *);
int init_ga_LOOPBACK(struct _GASTATE *);
int getDescription_LOOPBACK(char *reply);
void* thr_sendrec_LOOPBACK(void *);

#endif
