/* $Id$ */

#ifndef _LOOPBACK_H
#define _LOOPBACK_H

typedef struct _LOOPBACK_DATA {
    int number_ga;
    int number_gl;
    int number_fb;
} LOOPBACK_DATA;

int readconfig_loopback(xmlDocPtr doc, xmlNodePtr node, int busnumber);
int init_lineLoopback(char *);
int init_bus_Loopback(int );
int term_bus_Loopback(int );
int init_gl_Loopback(struct _GLSTATE *);
int init_ga_Loopback(struct _GASTATE *);
int getDescription_LOOPBACK(char *reply);
void* thr_sendrec_Loopback(void *);

#endif
