/* $Id$ */

#ifndef _LOOPBACK_H
#define _LOOPBACK_H

typedef struct _LOOPBACK_DATA {
} LOOPBACK_DATA;

int init_lineLoopback(char *);
int init_bus_Loopback(int );
int term_bus_Loopback(int );
void* thr_sendrec_Loopback(void *);

#endif
