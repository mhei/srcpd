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

int  readByte(int bus, int wait, unsigned char *the_byte);
void writeByte(int bus, unsigned char the_byte, unsigned long msec);
void writeString(int bus, unsigned char *the_string, unsigned long msecs);

void restore_comport(int bus);
void save_comport(int bus);
void close_comport(int bus);

int socket_readline(int Socket, char *line, int len);
int socket_writereply(int Socket, const char *line);

#endif