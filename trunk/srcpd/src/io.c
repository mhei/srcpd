/***************************************************************************
                          io.c  -  description
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
#include <termios.h>
#include <unistd.h>

#include "io.h"

int readByte(int FD, unsigned char *the_byte)
{
	int i;
#ifdef TESTMODE
	if(testmode)
	{	
		i = 1;
		*the_byte = 0;
	}
	else
	{
		i = read(FD, the_byte, 1);
		if(i == 1)
			syslog(LOG_INFO, "Byte gelesen : 0x%02x", *the_byte);
		else
			syslog(LOG_INFO, "kein Byte empfangen");
	}
#else
	i = read(FD, the_byte, 1);
#endif
	return (i == 1 ? 0 : -1);
}

void writeByte(int FD, unsigned char *b, unsigned long usecs)
{
#ifdef TESTMODE
	syslog(LOG_INFO, "Byte geschrieben : 0x%02x", *b);
#endif
	write(FD, b, 1);
	tcflush(FD, TCOFLUSH);
	usleep(usecs);
}
