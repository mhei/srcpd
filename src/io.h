/***************************************************************************
                          io.h  -  description
                             -------------------
    begin                : Wed Jul 4 2001
    copyright            : (C) 2001 by Dipl.-Ing. Frank Schmischke
    email                : frank.schmischke@t-online.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef _IO_H
#define _IO_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

int  readByte(int FD, int wait, unsigned char *the_byte);
void writeByte(int FD, unsigned char *the_byte, unsigned long msec);

void restore_comport(int bus);
void save_comport(int bus);
void close_comport(int bus);

#endif
