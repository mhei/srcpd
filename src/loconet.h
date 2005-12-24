/* $Id$ */

#ifndef _LOCONET_H
#define _LOCONET_H

typedef struct _LOCONET_DATA {
    int number_fb;
} LOCONET_DATA;

int readConfig_LOCONET(xmlDocPtr doc, xmlNodePtr node, int busnumber);
int init_bus_Loconet(int );
int term_bus_Loconet(int );
int getDescription_LOCONET(char *reply);
void* thr_sendrec_Loconet(void *);

#endif
