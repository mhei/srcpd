/* cvs: $Id$             */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _M605X_H
#define _M605X_H

typedef struct _M6051_DATA {
    int cmd32_pending;
} M6051_DATA;

int init_line6051(char *);
int init_bus_M6051(int );
void* thr_sendrec6051(void *);

#endif
