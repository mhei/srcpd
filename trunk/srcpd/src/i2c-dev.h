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

#define MAX_I2C_DEVICES      255
#define MAX_MULTIPELEX_BUSSES  8

typedef uint16_t I2C_VALUE; /* Currently we support I2C devices up to 16 bits */
typedef uint8_t  I2C_ADDR;  /* Currently we support 8 bit adresses on the i2c bus*/
typedef uint8_t  I2C_PORT;
typedef uint8_t  I2C_MUX_BUS;

typedef I2C_VALUE I2C_DEV_VALUES[MAX_I2C_DEVICES][MAX_MULTIPELEX_BUSSES];

typedef struct _I2CDEV_DATA {

	int number_ga;
	int multiplex_busses;
	int ga_hardware_inverters;
	int ga_reset_devices;    
 	int ga_min_active_time;

    I2C_DEV_VALUES i2c_values;
} I2CDEV_DATA;

int readconfig_I2C_DEV(xmlDocPtr doc, xmlNodePtr node, int busnumber);
int init_lineI2C_DEV(int );
int init_bus_I2C_DEV(int );
int term_bus_I2C_DEV(int );
int getDescription_I2C_DEV(char *reply);
void* thr_sendrec_I2C_DEV(void *);

// helper functions
void reset_ga(int busnumber, int busfd);
void select_bus(int mult_busnum, int busfd, int busnumber);

#endif
