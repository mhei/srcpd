/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "srcp-gl.h"

volatile struct _GL gl_mm[MAXGLS];  /* aktueller Stand, mehr gibt es nicht */
volatile struct _GL ngl_mm[MAXGLS]; /* neuer, evt geaenderter Wert         */

/* Übernehme die neuen Angaben für die Lok, einige wenige Prüfungen */
void setGL(char *prot, int addr, int dir, int speed, int maxspeed, int f, 
	   int n_fkt, int f1, int f2, int f3, int f4) {
    int i, n_fs;
    struct timezone dummy;
    n_fs = 14;
    if(strncasecmp(prot, "M3", 2) == 0){
	n_fs = 28; 
    } else if(strncasecmp(prot, "M5", 2) == 0) {
	n_fs = 27;
    }else if(strncasecmp(prot, "N1", 2) == 0) {
	n_fs = 28;
    }else if(strncasecmp(prot, "N2", 2) == 0) {
	n_fs = 128;
    }else if(strncasecmp(prot, "N3", 2) == 0) {
	n_fs = 28;
    }else if(strncasecmp(prot, "N4", 2) == 0) {
	n_fs = 128;
    }
    
    /* Daten einfüllen, aber nur, wenn id == 0!!, darauf warten wir max. 1 Sekunde */
    if( (strncasecmp(prot, "M", 1)==0 || strncasecmp(prot, "PS", 2)==0) && addr>0 && addr<MAXGLS) {
	i=0;
	do {
	    if(ngl_mm[addr].id) {
		i++;
		usleep(1000);
	    }
	    if(ngl_mm[addr].id==0) {
		strcpy((void*)ngl_mm[addr].prot, prot);
		ngl_mm[addr].speed     = speed;
		ngl_mm[addr].maxspeed  = maxspeed;
		ngl_mm[addr].direction = dir;
		ngl_mm[addr].n_fkt     = n_fkt;
		ngl_mm[addr].flags     = f1 + 2*f2 + 4*f3 + 8*f4 + 16*f; /* n_fkt ignorieren wir einfach mal */
		ngl_mm[addr].n_fs      = n_fs;
		gettimeofday(&ngl_mm[addr].tv, &dummy);
		/* und ab, fungiert als Semaphore, ist vielleicht nicht 100% ;=) */
		ngl_mm[addr].id = addr;
		break;
	    }
	} while(i<660);
    }
}

int getGL(char *prot, int addr, struct _GL *l) {
    if(strncasecmp(prot, "M", 1)==0 || strncasecmp(prot, "PS", 2)==0) {
        if(addr>0 && addr<MAXGLS) {
    	    *l = gl_mm[addr];
	    return 1;
	} else {
	    return 0;
	}
    } else {
	return 0;
    }

}

void infoGL(struct _GL gl, char* msg) {
    sprintf(msg, "INFO GL %s %d %d %d %d %d %d %d %d %d %d\n", 
	    gl.prot, gl.id, gl.direction, gl.speed, gl.maxspeed, 
	    (gl.flags & 0x10)?1:0, 
	    4, 
	    (gl.flags & 0x01)?1:0,
	    (gl.flags & 0x02)?1:0,
	    (gl.flags & 0x04)?1:0,
	    (gl.flags & 0x08)?1:0);
}

int cmpGL(struct _GL a, struct _GL b) {
    return ((a.direction == b.direction) &&
	    (a.speed     == b.speed)     &&
	    (a.maxspeed  == b.maxspeed)  &&
	    (a.n_fkt     == b.n_fkt)     &&
	    (a.n_fs      == b.n_fs)      &&
	    (a.flags     == b.flags)
	    );
}

void initGL(void) {
    int i;
    for(i=0; i<MAXGLS;i++) {
	strcpy( (void *)ngl_mm[i].prot, "PS");
	strcpy( (void *)gl_mm[i].prot, "PS");
	ngl_mm[i].direction = 1;
	gl_mm[i].direction = 1;
	gl_mm[i].id = i;
    }
}
