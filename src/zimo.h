/* $Id$ */

#ifndef _ZIMO_H
#define _ZIMO_H

typedef struct _zimo_DATA {
    int number_ga;
    int number_gl;
    int number_fb;
} zimo_DATA;

int readconfig_zimo(xmlDocPtr doc, xmlNodePtr node, long int busnumber);
int init_linezimo(char *);
long int init_bus_zimo(long int);
long int term_bus_zimo(long int);
int getDescription_zimo(char *reply);
void* thr_sendrec_zimo(void *);

#endif
