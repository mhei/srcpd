/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/*
 * i2c-dev: Busdriver for i2c-dev-Interface of the Linux-Kernel
 * 					can be used to access hardware found on
 *					http://www.matronix.de/
 *
 *					2002,2003 by Manuel Borchers <manuel@matronix.de>
 *
 */
#ifdef linux

#include "stdincludes.h"
#include "i2c-dev.h"

#include <linux/i2c-dev.h>
// we have to use kernel-headers directly, sorry!

#ifndef I2C_SLAVE
#define I2C_SLAVE	0x0703
#warning "defined hardcoded I2C_SLAVE, due to an yet unknown problem with headers."
#endif

#include <math.h>
// needed for pow()

#include "config-srcpd.h"
#include "io.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-srv.h"
#include "srcp-info.h"


#define __i2cdev ((I2CDEV_DATA*)busses[busnumber].driverdata)

void readconfig_I2C_DEV(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
    xmlNodePtr child = node->children;
	
	busses[busnumber].type = SERVER_I2C_DEV;
    busses[busnumber].init_func = &init_bus_I2C_DEV;
    busses[busnumber].term_func = &term_bus_I2C_DEV;
    busses[busnumber].thr_func = &thr_sendrec_I2C_DEV;
    busses[busnumber].driverdata = malloc(sizeof(struct _I2CDEV_DATA));
    strcpy(busses[busnumber].description, "GA POWER DESCRIPTION");

    __i2cdev->number_fb = 0;
    __i2cdev->number_ga = 0;
	__i2cdev->first_ga_bus = 0;
	__i2cdev->last_ga_bus = 0;
	__i2cdev->port_swap = 0;
    __i2cdev->number_gl = 0;
    __i2cdev->ga_min_active_time = 75;

    while (child) {
	if (strncmp(child->name, "text", 4) == 0) {
	    child = child->next;
	    continue;
	}

	/*
	if (strcmp(child->name, "number_gl") == 0) {
	    char *txt =
		xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    __i2cdev->number_gl = atoi(txt);
	    free(txt);
	}
	*/
	
	if (strcmp(child->name, "first_ga_bus") == 0) {
	    char *txt =
		xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    __i2cdev->first_ga_bus = atoi(txt);
	    free(txt);
	}
	
	if (strcmp(child->name, "last_ga_bus") == 0) {
	    char *txt =
		xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    __i2cdev->last_ga_bus = atoi(txt);
	    free(txt);
	}
	
	if (strcmp(child->name, "port_swap") == 0) {
	    char *txt =
		xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    __i2cdev->port_swap = atoi(txt);
	    free(txt);
	}
	
	if (strcmp(child->name, "first_fb_bus") == 0) {
	    char *txt =
		xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    __i2cdev->first_ga_bus = atoi(txt);
	    free(txt);
	}
	
	if (strcmp(child->name, "last_fb_bus") == 0) {
	    char *txt =
		xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    __i2cdev->last_ga_bus = atoi(txt);
	    free(txt);
	}
	
	child = child->next;
    }				// while

	
	// check if config for GA is valid
	if ((__i2cdev->first_ga_bus < 1) && (__i2cdev->last_ga_bus < 1) && 
		(__i2cdev->last_ga_bus < __i2cdev->first_ga_bus)) 
	{
		__i2cdev->first_ga_bus = 0;
		__i2cdev->last_ga_bus = 0;
		__i2cdev->number_ga = 0;
	} else {
		__i2cdev->number_ga = 64 * (1 + __i2cdev->last_ga_bus - __i2cdev->first_ga_bus);
		
	}
	
	// check if config for FB is valid
	// FIXME: more complex check for overlaping with GA
	if ( (__i2cdev->first_fb_bus < 1) && (__i2cdev->last_fb_bus < 1) && 
		(__i2cdev->last_fb_bus < __i2cdev->first_fb_bus) ) 
	{
		__i2cdev->first_fb_bus = 0;
		__i2cdev->last_fb_bus = 0;
		__i2cdev->number_fb = 0;
	} else {
		__i2cdev->number_fb = 16 * (1 + __i2cdev->last_ga_bus - __i2cdev->last_ga_bus);
	}
	
	// init the stuff
    if (init_GA(busnumber, __i2cdev->number_ga)) {
		__i2cdev->number_ga = 0;
		DBG(busnumber, DBG_ERROR, "Can't create array for accessoires");
    }

	/*
    if (init_GL(busnumber, __i2cdev->number_gl)) {
		__i2cdev->number_gl = 0;
		DBG(busnumber, DBG_ERROR, "Can't create array for locomotivs");
    }
	*/
	
    if (init_FB(busnumber, __i2cdev->number_fb)) {
		__i2cdev->number_fb = 0;
		DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
    }
}


int init_lineI2C_DEV(int bus)
{
    int FD;

    if (busses[bus].debuglevel > 0) {
	DBG(bus, DBG_INFO, "Opening i2c-dev: %s", busses[bus].device);
    }

    FD = open(busses[bus].device, O_RDWR);

    if (FD <= 0) {
		DBG(bus, DBG_FATAL, "couldn't open device %s.",
	    	busses[bus].device);
		FD = -1;
    }

    return FD;

}

void reset_ga(int busnumber, int busfd) {
	// resets all GA devices on all multiplexed busses
	
	int i, multiplexer_adr;
	int first_ga_bus, last_ga_bus;
	char buf = 0xff;
	
	first_ga_bus = __i2cdev->first_ga_bus;
	last_ga_bus = __i2cdev->last_ga_bus;
	
	if (__i2cdev->number_ga > 0) {
		
		for (multiplexer_adr = first_ga_bus; 
				multiplexer_adr <= last_ga_bus; 
				multiplexer_adr++) 
		{
		
			select_bus(multiplexer_adr, busfd, busnumber);
			
			// first PCF 8574 P
			for (i = 64; i <= 78; i++) {
				ioctl(busfd, I2C_SLAVE, (i >> 1));
				writeByte(busnumber, buf, 1);
				i++;
			}

			// now PCF 8574 AP
			for (i = 112; i <= 126; i++) {
				ioctl(busfd, I2C_SLAVE, (i >> 1));
				writeByte(busnumber, buf, 1);
				i++;
			}
		
		}
		
		select_bus(0, busfd, busnumber);
		
	}
	
}

void select_bus(int mult_busnum, int busfd, int busnumber) {

	int addr, value=0;
	
	//addr = 224 + (2 * (int) (mult_busnum / 9));
	addr = 224;
	if (mult_busnum > 8) mult_busnum = 0;
	
	if (busses[busnumber].power_state == 1) value = 64;
	value = value | (mult_busnum % 9);
	
	ioctl(busfd, I2C_SLAVE, (addr >> 1));
	writeByte(busnumber, value, 1);
	
}

int term_bus_I2C_DEV(int bus)
{
    DBG(bus, DBG_INFO, "i2c-dev bus %d terminating", bus);
    close(busses[bus].fd);
    return 0;
}

/*
 *
 * Initializes the I2C-Bus and sets fd for the bus
 * If bus unavailible, fd = -1
 *
 */
int init_bus_I2C_DEV(int i)
{
	
	int old_power_state = busses[i].power_state;

    DBG(i, DBG_INFO, "i2c-dev init: bus #%d, debug %d", i,
		busses[i].debuglevel);
    if (busses[i].debuglevel < 6) {
		busses[i].fd = init_lineI2C_DEV(i);
		DBG(i, DBG_INFO, "i2c-dev init: reseting devices");
		
		// enable POWER for the bus, to reset the devices
		busses[i].power_state = 1;
		reset_ga(i, busses[i].fd);
		// reset to old state
		busses[i].power_state = old_power_state;
		select_bus(0, busses[i].fd, i);
		
    } else {
		busses[i].fd = -1;
    }
	
    DBG(i, DBG_INFO, "i2c-dev init done");
    return 0;
}

/*
 * 
 * The main worker-thread for the i2c-dev device
 * Enters an endless loop that waits for commands and
 * executes them.
 *
 * At the moment we only support GA-devices
 *
 */
void *thr_sendrec_I2C_DEV(void *v)
{
    //struct _GLSTATE gltmp, glakt;
    struct _GASTATE gatmp;
    int addr, port, value;
    int bus = (int) v;
    int ga_min_active_time =
		((I2CDEV_DATA *) busses[bus].driverdata)->ga_min_active_time;
	int port_swap =
		((I2CDEV_DATA *) busses[bus].driverdata)->port_swap;

    int i2c_addr = 0, i2c_base_addr = 0;
    char i2c_val = 0, i2c_oldval = 0;
	
	int mult_busnum;

    DBG(bus, DBG_INFO, "i2c-dev started, bus #%d, %s", bus,
	busses[bus].device);

    busses[bus].watchdog = 1;
	
	// process POWER changes
	if (busses[bus].power_changed == 1) {
		// something happend!
		
		char msg[1000];
		
		// dummy select to change power state
		select_bus(0, busses[bus].fd, bus);
		busses[bus].power_changed = 0;
		infoPower(bus, msg);
		queueInfoMessage(msg);
	}
	
    while (1) {
        // do nothing, if power off
        if(busses[bus].power_state==0) {
	      usleep(1000);
              continue;
        }

	busses[bus].watchdog = 4;

	// process GA commands
	if (!queue_GA_isempty(bus)) {
	    unqueueNextGA(bus, &gatmp);
	    addr = gatmp.id;
	    port = gatmp.port;
	    value = gatmp.action;
		
		// some error-corrections
		if (port > 1) port = 1;
		if (value > 1) value = 1;
		
		// port-swap feature
		if (port_swap) {
			if (port == 1) port = 0; else port = 1;
		}
		
	    // address-calculation
		
		// calc and select multiplexed bus
	    mult_busnum = 1 + (addr / 65);
		select_bus(mult_busnum, busses[bus].fd, bus);

	    // PCF 8574 P
	    if ((addr % 65) < 33)
			i2c_base_addr = 64;
	    // PFC 8574 AP
	    if ((addr % 65) >= 33)
			i2c_base_addr = 112;
		
		
		// first project all higher multiplexed busses to the first bus
		i2c_addr = addr - (64 * (mult_busnum-1));
		// now project all PCF-8574 AP addresses to PCF-8574 P addresses
		i2c_addr = i2c_addr - ( (int)(i2c_addr/33) * 32 );
		// now calc the address offset
		i2c_addr = 2 * ( (int)((i2c_addr-1)/4) );
		// last step: add the base-address
		i2c_addr = i2c_base_addr + i2c_addr;
		
		DBG(bus, DBG_DEBUG, "i2c_base_addr = %d, i2c_addr = %d on multiplexed bus #%d", 
			i2c_base_addr, i2c_addr, mult_busnum);

	    // calculate bit-value from command
		i2c_val = (char) pow(2, ((((addr - ((char)(addr/32)) * 32) - 1) % 4) * 2 + port));

	    // select the device
	    ioctl(busses[bus].fd, I2C_SLAVE, (i2c_addr >> 1));
	    // read old value
	    readByte(bus, 1, &i2c_oldval);
		//read(busses[bus].fd, &i2c_oldval, 1);

	    if (gatmp.action == 1) {
			gettimeofday(&gatmp.tv[gatmp.port], NULL);
			setGA(bus, addr, gatmp);

			if (gatmp.activetime >= 0) {
				gatmp.activetime =
					(gatmp.activetime >
					ga_min_active_time) ? gatmp.activetime : ga_min_active_time;
				gatmp.action = 0;
			} else {
				gatmp.activetime = ga_min_active_time;	// always wait minimum time
			}

			// calcutlate new value for the device
			i2c_val = (i2c_oldval & (255 - i2c_val));

			// write it
			writeByte(bus, i2c_val, 0);
			// wait
			usleep(1000 * (unsigned long) gatmp.activetime);

	    }

	    if ((gatmp.action == 0)) {
			gettimeofday(&gatmp.tv[gatmp.port], NULL);
			setGA(bus, addr, gatmp);

			if (gatmp.activetime >= 0) {
				gatmp.activetime =
					(gatmp.activetime >
					ga_min_active_time) ? gatmp.activetime : ga_min_active_time;
			} else {
				gatmp.activetime = ga_min_active_time;	// always wait minimum time
			}

			i2c_val = (i2c_oldval | i2c_val);

			writeByte(bus, i2c_val, 0);
			usleep(1000 * (unsigned long) gatmp.activetime);

	    }

		select_bus(0, busses[bus].fd, bus);
	    busses[bus].watchdog = 6;
	}

	usleep(1000);
    }
}
#endif
