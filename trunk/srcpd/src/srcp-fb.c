/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#include <stdio.h>
#include <string.h>
#include "srcp-fb.h"
#include "srcp-fb-m6051.h"
#include "srcp-fb-i8255.h"

void getFBall(const char *proto, char *reply) {
    int i;
    if(strncmp(proto, "M6051", 5)==0) {
      strcpy(reply, "INFO FB M6051 * ");   // und hier wird in d das Ergebnis aufgebaut
        for(i=1; i<=getPortCount_M6051(); i++) {
           char cc[5];
           sprintf(cc, "%d", getFB_M6051(i));
           strcat(reply, cc);
        }
        strcat(reply, "\n");
    } else if (strncmp(proto, "i8255", 5)==0) {
      strcpy(reply, "INFO FB I8255 * ");   // und hier wird in d das Ergebnis aufgebaut
      for(i=1; i<=getPortCount_I8255(); i++) {
        char cc[5];
        sprintf(cc, "%d", getFB_I8255(i));
        strcat(reply, cc);
      }
    } else {
	sprintf(reply, "INFO -1");
    }
    strcat(reply, "\n");
}

int getFBone(const char *proto, int port) {
    int result;
    if(strncmp(proto, "M6051", 5)==0) {
	result = getFB_M6051(port);
    } else if(strncmp(proto, "I8255", 5)==0){
	result = getFB_I8255(port);
    } else {
	result = -1;
    }
    return result;
}

void infoFB(const char *proto, int port, char *msg) {
    int state = getFBone(proto, port);
    if(state>=0) {
	sprintf(msg, "INFO FB %s %d %d\n", proto, port, state);
    } else {
	sprintf(msg, "INFO -2\n");
    }
}

void initFB() {

}
