//
// C++ Interface: portio
//
// Description: 
//
//
// Author: Ing. Geard van der Sel
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef _PORTIO_H
#define _PORTIO_H

int open_port(long int bus);
void close_port(long int bus);
void write_port(long int bus, unsigned char b);
int check_port(long int bus);
int read_port(long int bus,  unsigned char *rr);


#endif

