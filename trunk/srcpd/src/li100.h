/***************************************************************************
                           li100.h  -  description
                             -------------------
    begin                : Thu Jan 22 2002
    copyright            : (C) 2002 by Dipl.-Ing. Frank Schmischke
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
#ifndef _LI100_H
#define _LI100_H

int init_lineLI100(char*);
void* thr_sendrecli100(void*);
void send_command_ga_li(int fd);
void send_command_gl_li(int fd);
void check_status_li(int fd);
int send_command(int fd, char *str);

#endif
