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
 *					Copyright:		(c) 2002 by Manuel Borchers <webmaster@matronix.de>
 *
 */

#include "stdincludes"

#ifdef linux 
#include <linux/i2c-dev.h>
// we have to use kernel-headers directly, sorry!
#include <math.h>
// needed for pow()

#include <libxml/tree.h>

#include "config-srcpd.h"
#include "io.h"
#include "i2c-dev.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-srv.h"

#define __i2cdev ((I2CDEV_DATA*)busses[busnumber].driverdata) 

void readconfig_I2C_DEV(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
  xmlNodePtr child = node->children;

  busses[busnumber].type = SERVER_I2C_DEV;
  busses[busnumber].init_func = &init_bus_I2C_DEV;
  busses[busnumber].term_func = &term_bus_I2C_DEV;
  busses[busnumber].thr_func = &thr_sendrec_I2C_DEV;
  busses[busnumber].driverdata = malloc(sizeof(struct _I2CDEV_DATA));
	strcpy(busses[busnumber].description, "GA");
	
  __i2cdev->number_fb = 0;
  __i2cdev->number_ga = 64;
  __i2cdev->number_gl = 0;
	__i2cdev->ga_min_active_time = 75;
  
  while (child)
  {
    if(strncmp(child->name, "text", 4)==0)
    {
      child = child -> next;
      continue;
    }

    if (strcmp(child->name, "number_fb") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __i2cdev->number_fb = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "number_gl") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __i2cdev->number_gl = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "number_ga") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __i2cdev->number_ga = atoi(txt);
      free(txt);
    }
    child = child -> next;
  } // while

	if(init_GA(busnumber, __i2cdev->number_ga))
  {
    __i2cdev->number_ga = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for accessoires");
  }
	
	//fprintf(stdout, "number_ga = %d\n", __i2cdev->number_ga);
	
  if(init_GL(busnumber, __i2cdev->number_gl))
  {
    __i2cdev->number_gl = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for locomotivs");
  }
  if(init_FB(busnumber, __i2cdev->number_fb))
  {
    __i2cdev->number_fb = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
  }
}


int init_lineI2C_DEV (int bus)
{
  int FD;
	int i, end_addr, number_ga, temp;
	char buf=0xff;
  
	if (busses[bus].debuglevel > 0)
  {
    DBG(bus, DBG_INFO, "Opening i2c-dev: %s", busses[bus].device);
		//fprintf(stdout, "Opening i2c-dev: %s\n", busses[bus].device);
  }
	
	FD = open (busses[bus].device, O_RDWR);
	//fprintf(stdout, "FD = %d\n", FD);
	
	if (FD <= 0) {
		DBG(bus, DBG_FATAL, "couldn't open device %s.", busses[bus].device);
		//fprintf(stdout, "couldn't open device %s.\n", busses[bus].device);
		FD = -1;
	} else {
		
		// reset all GA devices
		number_ga = (((I2CDEV_DATA*)busses[bus].driverdata)->number_ga);
		
		// first PCF 8574 P
		temp = number_ga;
		if (temp > 32) 
			temp = 32;
		end_addr = 63 + 2 * temp;
		
		for(i=64; i<= end_addr; i++) {
			ioctl(FD, I2C_SLAVE, (i >> 1));
			write (FD, &buf, 1);
			i++;
		}
		
		// now PCF 8574 AP
		temp = number_ga;
		if(temp > 32) {
			end_addr = 112 + ( 2 * (temp - 32) );
			
			for (i=112;i<= end_addr; i++) {
				ioctl(FD, I2C_SLAVE, (i >> 1));
				write (FD, &buf, 1);
				i++;
			}
		}
		
	}
	
	return FD;
	
}

int term_bus_I2C_DEV(int bus)
{
	
	//fprintf(stdout, "i2c-dev bus %d terminating\n", bus);
  DBG(bus, DBG_INFO, "i2c-dev bus %d terminating", bus);
	
	close (busses[bus].fd);
	
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
	
	//fprintf(stdout, "i2c-dev init: bus #%d, debug %d\n", i, busses[i].debuglevel);
	
  DBG(i, DBG_INFO,"i2c-dev init: bus #%d, debug %d", i, busses[i].debuglevel);
  if(busses[i].debuglevel < 6)
  {
    busses[i].fd = init_lineI2C_DEV(i);
  }
  else
  {
    busses[i].fd = -1;
  }
	
	//fprintf(stdout, "i2c-dev init done\n");
	
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
void* thr_sendrec_I2C_DEV (void *v)
{
  //struct _GLSTATE gltmp, glakt;
  struct _GASTATE gatmp;
  int addr, port, value;
  int bus = (int) v;
	int ga_min_active_time = ( (I2CDEV_DATA *) busses[bus].driverdata)  ->ga_min_active_time;
	
	int i2c_addr, i2c_base_addr;
	char i2c_val, i2c_oldval;
	
	int no_off = 0;
  
	//fprintf(stdout, "i2c-dev started, bus #%d, %s\n", bus, busses[bus].device); 
	
  DBG(bus, DBG_INFO, "i2c-dev started, bus #%d, %s", bus, busses[bus].device);

  busses[bus].watchdog = 1;

	//fprintf (stdout, "Entering i2c-dev worker thread...\n");
	
  while (1) {

			/*		
      if (!queue_GL_isempty(bus))  {

        unqueueNextGL(bus, &gltmp);
				
        addr = gltmp.id;
        getGL(bus, addr, &glakt);

        if (gltmp.direction == 2)  {
          gltmp.speed = 0;
          gltmp.direction = !glakt.direction;
        }
        setGL(bus, addr, gltmp, 0);

      }
			*/
			
			busses[bus].watchdog = 4;
			
			
      if (!queue_GA_isempty(bus)) {
          unqueueNextGA(bus, &gatmp);
          addr = gatmp.id;
					port = gatmp.port;
					value = gatmp.action;
					//i2c_val = 255;
					//i2c_oldval = 255;
					
					// address-calculation
					// we will need more complex address-calculation later
					// for a multiplexed I2C-Bus...
					// at the moment, we only have one bus, so just deal with
					// the different types of PCF 8574 chips
					
					// PCF 8574 P
					if (addr < 33)
						i2c_base_addr = 64;
					// PFC 8574 AP
					if (addr >= 33)
						i2c_base_addr = 112;
					
					i2c_addr = i2c_base_addr + 2 * ((int) ((addr-1) / 4));
					
					// calculate bit-value from command
					i2c_val  = (char) pow ( 2, (((addr-1) % 4) * 2 + port) );
					
					// select the device
					ioctl( busses[bus].fd, I2C_SLAVE, (i2c_addr >> 1) );
					// read old value
					readByte(bus, 1, &i2c_oldval);
					
					
          if(gatmp.action == 1) {
            gettimeofday(&gatmp.tv[gatmp.port], NULL);
            setGA(bus, addr, gatmp, 0);
						
						if (gatmp.activetime >= 0) {
							gatmp.activetime = (gatmp.activetime > ga_min_active_time) ?
                        gatmp.activetime : ga_min_active_time;
							gatmp.action = 0;
						} else { 
							gatmp.activetime = ga_min_active_time;  // always wait minimum time
							no_off = 1;
						}
						
						// calcutlate new value for the device
						i2c_val = (i2c_oldval & (255-i2c_val));

						// write it
						writeByte(bus, &i2c_val, 0);
						// wait
						usleep(1000 * (unsigned long) gatmp.activetime);
						
						// FIXME: Ausschaltzeitpunkt setzen!
						// write old value back, if wait-time != -1
						if ( no_off == 0 ) {
							writeByte(bus, &i2c_oldval, 0);
						}

					}
					
					// FIXME: nur ausführen, wenn nich schon oben ausgeführt
					if ( (gatmp.action == 0) ) {
						gettimeofday(&gatmp.tv[gatmp.port], NULL);
            setGA(bus, addr, gatmp, 0);
						
						if (gatmp.activetime >= 0) {
							gatmp.activetime = (gatmp.activetime > ga_min_active_time) ?
                        gatmp.activetime : ga_min_active_time ;
						} else { 
							gatmp.activetime = ga_min_active_time;  // always wait minimum time
						}

						i2c_val = (i2c_oldval | i2c_val);

						writeByte(bus, &i2c_val, 0);
						usleep(1000 * (unsigned long) gatmp.activetime);
						
					}
					
          setGA(bus, addr, gatmp, 0);
          busses[bus].watchdog = 6;
      }
			
      usleep(1000);
  }
}
#endif
