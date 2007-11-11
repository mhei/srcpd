/* $Id$ */

#ifndef _ZIMO_H
#define _ZIMO_H

typedef struct _zimo_DATA {
    int number_ga;
    int number_gl;
    int number_fb;
} zimo_DATA;

int readconfig_zimo(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber);
int init_bus_zimo(bus_t);
int term_bus_zimo(bus_t);
int getDescription_zimo(char *reply);
void* thr_sendrec_zimo(void *);

#endif
