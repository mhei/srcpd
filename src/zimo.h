/* $Id$ */

#ifndef _ZIMO_H
#define _ZIMO_H

typedef struct _zimo_DATA {
    int number_ga;
    int number_gl;
    int number_fb;
} zimo_DATA;

void readconfig_zimo(xmlDocPtr doc, xmlNodePtr node, int busnumber);
int init_linezimo(char *);
int init_bus_zimo(int );
int term_bus_zimo(int );
int getDescription_zimo(char *reply);
void* thr_sendrec_zimo(void *);

#endif
