/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * This software is published under the restrictions of the 
 * GNU License Version2
 *
 */

#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "srcp-ga.h"

volatile struct _GA ga_mm[MAXGAS];  /* soviele Generic Accessoires gibts             */
volatile struct _GA nga_mm[MAXGAS]; /* soviele Generic Accessoires gibts, neue Werte */


/* setze den Schaltdekoder, einige wenige Prüfungen, max. 2/3 Sekunde warten */
int setGA(char *prot, int addr, int port, int aktion, long activetime) {
    int i;
    struct timezone dummy;
    /* Only Marklin Protocol is relevant, P only if inside the address space */
    /* P is considered the same as M */
    if( (strncasecmp(prot, "M", 1)==0 || strncasecmp(prot, "P", 1)==0) && addr>0 && addr<MAXGAS) {
	i=0;
	do {
	    if(nga_mm[addr].id) {
		i++;
		usleep(1000);
	    }
	    if(nga_mm[addr].id==0) {
		strcpy((void *) nga_mm[addr].prot, prot);
		nga_mm[addr].action     = aktion;
		nga_mm[addr].port       = port;
		nga_mm[addr].activetime = activetime;
		gettimeofday(&nga_mm[addr].tv[port], &dummy);
		nga_mm[addr].id         = addr;
		break;
	    }
	} while(i<660);
	return 1;
    }
    return 0;
}


int getGA(char *prot, int addr, struct _GA *a) {
    if(strncasecmp(prot, "M", 1)==0 && addr>0 && addr<MAXGAS) {
	*a = ga_mm[addr];
	return 0;
    } else {
	return 1;
    }
}

int infoGA(struct _GA a, char *msg) {
    sprintf(msg, "INFO GA %s %d %d %d %ld\n", a.prot, a.id, a.port, a.action, a.activetime);
    return 0;
}    

int cmpGA(struct _GA a, struct _GA b) {
    return ((a.action == b.action) &&
	    (a.port   == b.port)   
	    );
}

void initGA() {
    int i;
    for(i=0; i<MAXGAS;i++) {
	strcpy((void *) ga_mm[i].prot, "M");
	ga_mm[i].id = i;
    }

}
