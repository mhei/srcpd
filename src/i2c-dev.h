/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef _I2C_DEV_H
#define _I2C_DEV_H

typedef struct _I2CDEV_DATA {
    int number_ga;
	int first_ga_bus;
	int last_ga_bus;
    int number_gl;
    int number_fb;
	int ga_min_active_time;
} I2CDEV_DATA;

void readconfig_I2C_DEV(xmlDocPtr doc, xmlNodePtr node, int busnumber);
int init_lineI2C_DEV(int );
int init_bus_I2C_DEV(int );
int term_bus_I2C_DEV(int );
int getDescription_I2C_DEV(char *reply);
void* thr_sendrec_I2C_DEV(void *);

// helper functions
void reset_ga(int busnumber, int busfd);

#endif
