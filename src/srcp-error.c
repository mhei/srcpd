/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */
#ifndef SRCP_ERROR_H
#define SRCP_ERROR_H 1

int srcp_fmterror(int errno, char *msg) {
    switch (errno){
    case 200:  
	sprintf(msg, "200 OK"); break;
    default: return 1;
    }
    return 0;
}

#endif
