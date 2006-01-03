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
*           can be used to access hardware found on
*         http://www.matronix.de/
*
*         2002-2005 by Manuel Borchers <manuel@matronix.de>
*
*/
#ifdef linux

#include "stdincludes.h"
#include "i2c-dev.h"

#include <linux/i2c-dev.h>
// we have to use kernel-headers directly, sorry!

#ifndef I2C_SLAVE
#define I2C_SLAVE 0x0703
#warning "defined hardcoded I2C_SLAVE, due to an yet unknown problem with headers."
#endif

#include "config-srcpd.h"
#include "io.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
/*#include "srcp-gl.h"*/
#include "srcp-power.h"
#include "srcp-srv.h"
#include "srcp-info.h"
#include "srcp-error.h"

#define __i2cdev ((I2CDEV_DATA*)busses[busnumber].driverdata)


static int write_PCF8574(long int bus, int addr, __u8 byte)
{

  int busfd = busses[bus].fd;
  int ret;

  ret = ioctl(busfd, I2C_SLAVE, addr);

  if (ret < 0) {
    DBG(bus, DBG_INFO, "Couldn't access address %d (%s)",
        addr, strerror(errno));
    return (ret);
  }

//  ret = i2c_smbus_write_byte(busfd, byte);

  if (ret < 0) {
    DBG(bus, DBG_INFO, "Couldn't send byte %d to address %d (%s)",
        byte, addr, strerror(errno));
    return (ret);
  }

  DBG(bus, DBG_DEBUG, "Sent byte %d to address %d", byte, addr);
  return (0);

}


/*  Currently feedback is not supported */
/*
static int read_PCF8574(long int bus, int addr, __u8 *byte)
{
    int busfd = busses[bus].fd;
    int ret;

    ret = ioctl(busfd, I2C_SLAVE, addr);
    if (ret < 0) {
      DBG(bus, DBG_INFO, "Can't access adress %d (%s)",
          addr, strerror(errno));
      return (ret);
    }

    ret = i2c_smbus_read_byte(busfd);
    if (ret < 0) {
      DBG(bus, DBG_INFO, "Can't read byte from adress %d (%s)",
          addr, strerror(errno));
      return (ret);
    }
    *byte = 0xFF & (ret);
    DBG(bus, DBG_DEBUG, "Read byte %d from adress %d",
        *byte, addr);
    return (ret);
}
*/

/* Write value to a i2c device, determine i2c device by adress */

static int write_i2c_dev(long int bus, int addr, I2C_VALUE value)
{
    if (((addr >=32) && (addr < 39)) ||  /* Currently we handle only PCF8574 */
        ((addr >=56) && (addr < 63))) {
    return (write_PCF8574(bus, addr, 0xFF & value));
    }

    DBG(bus, DBG_ERROR, "Unsupported i2c device at address %d", addr);
    return (-1);
}

/*  Handle set command of GA */

static int handle_i2c_set_ga(long int bus, struct _GASTATE *gatmp)
{
    I2C_ADDR    i2c_addr;
  I2C_VALUE   i2c_val;
  I2C_VALUE   value;
    I2C_PORT    port;
    I2C_MUX_BUS mult_busnum;
    I2CDEV_DATA *data = (I2CDEV_DATA *)busses[bus].driverdata;
    int addr;
    int reset_old_value;
    int ga_min_active_time    = data->ga_min_active_time;
  int ga_hardware_inverters = data->ga_hardware_inverters;

  addr  = gatmp->id;
  port  = gatmp->port;
  value = gatmp->action;

  mult_busnum = (addr / 256) + 1;
  select_bus(mult_busnum, busses[bus].fd, bus);

  i2c_addr = (addr % 256);
/*  gettimeofday(gatmp->tv[0], NULL); */

  if (gatmp->activetime >= 0) {
    gatmp->activetime = (gatmp->activetime > ga_min_active_time) ?
                      gatmp->activetime :  ga_min_active_time;
    reset_old_value = 1;
  } else {
    gatmp->activetime = ga_min_active_time; // always wait minimum time
    reset_old_value = 0;
  }

  DBG(bus, DBG_DEBUG, "i2c_addr = %d on multiplexed bus #%d",
        i2c_addr, mult_busnum);
  // port: 0     - direct write of value to device
  //       other - select portpins directly, value = {0,1}

  if (port == 0) {

    // write directly to device

    if (ga_hardware_inverters == 1) {
      i2c_val = ~value;
      } else {
      i2c_val = value;
    }

  } else {

    // set bit for selected port to 1
    I2C_VALUE pin_bit = 1 << (port - 1);
        value = (value & 1);
        i2c_val = data->i2c_values[i2c_addr][mult_busnum-1];

    if (ga_hardware_inverters == 1) {
          value = (~value) & 1;
        }

        if (value != 0) {     /* Set bit */
      i2c_val |= pin_bit;
        }
        else {                /* Reset bit */
      i2c_val &= (~pin_bit);
        }

    }

  if (write_i2c_dev(bus, i2c_addr, i2c_val) < 0) {
      DBG(bus, DBG_ERROR, "Device not found at address %d", addr);
      return (-1);
    }

  usleep(1000 * (unsigned long) gatmp->activetime);

  if (reset_old_value == 1) {

    if (write_i2c_dev(bus, i2c_addr, data->i2c_values[i2c_addr][mult_busnum-1]) < 0) {
      DBG(bus, DBG_ERROR, "Device not found at address %d", addr);
          return (-1);
        }

    usleep(1000 * (unsigned long) gatmp->activetime);

  } else {
    data->i2c_values[i2c_addr][mult_busnum-1] = i2c_val;
    }

    return (0);
}

int readconfig_I2C_DEV(xmlDocPtr doc, xmlNodePtr node, long int busnumber)
{
  busses[busnumber].type = SERVER_I2C_DEV;
  busses[busnumber].init_func = &init_bus_I2C_DEV;
  busses[busnumber].term_func = &term_bus_I2C_DEV;
  busses[busnumber].thr_func = &thr_sendrec_I2C_DEV;
  busses[busnumber].driverdata = malloc(sizeof(I2CDEV_DATA));
  strcpy(busses[busnumber].description, "GA POWER DESCRIPTION");

  __i2cdev->number_ga = 0;
  __i2cdev->multiplex_busses = 0;
  __i2cdev->ga_hardware_inverters = 0;
  __i2cdev->ga_reset_devices = 1;
  __i2cdev->ga_min_active_time = 75;

  xmlNodePtr child = node->children;
  xmlChar *txt = NULL;

  while (child) {

    if (xmlStrncmp(child->name, BAD_CAST "text", 4) == 0) {
      child = child->next;
      continue;
    }

    /*
    if (xmlStrcmp(child->name, BAD_CAST "number_gl") == 0) {
        txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
        if (txt != NULL) {
            __i2cdev->number_gl = atoi((char*) txt);
            xmlFree(txt);
        }
    }
    */

    if (xmlStrcmp(child->name, BAD_CAST "multiplex_busses") == 0) {
        txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
        if (txt != NULL) {
            __i2cdev->multiplex_busses = atoi((char*) txt);
            xmlFree(txt);
        }
    }

    if (xmlStrcmp(child->name, BAD_CAST "ga_hardware_inverters") == 0) {
        txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
        if (txt != NULL) {
            __i2cdev->ga_hardware_inverters = atoi((char*) txt);
            xmlFree(txt);
        }
    }

    if (xmlStrcmp(child->name, BAD_CAST "ga_reset_device") == 0) {
        txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
        if (txt != NULL) {
            __i2cdev->ga_reset_devices = atoi((char*) txt);
            xmlFree(txt);
        }
    }

    child = child->next;
  }

  // init the stuff

  __i2cdev->number_ga = 256 * (__i2cdev->multiplex_busses);

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
  /*
  if (init_FB(busnumber, __i2cdev->number_ga)) {
    __i2cdev->number_ga = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
  }
  */
  return(1);
}


int init_lineI2C_DEV(long int bus)
{
  int FD;

  if (busses[bus].debuglevel > 0) {
    DBG(bus, DBG_INFO, "Opening i2c-dev: %s", busses[bus].device);
  }

  FD = open(busses[bus].device, O_RDWR);

  if (FD <= 0) {
    DBG(bus, DBG_FATAL, "Couldn't open device %s.",
        busses[bus].device);
    FD = -1;
  }

  return FD;

}

void reset_ga(long int busnumber, int busfd) {

  // reset ga devices to values stored in data->i2c_values

  I2CDEV_DATA *data = (I2CDEV_DATA *)busses[busnumber].driverdata;
  int i, multiplexer_adr;
  int multiplex_busses;

  multiplex_busses = data->multiplex_busses;

  if (multiplex_busses > 0) {

    for (multiplexer_adr = 1; multiplexer_adr <= multiplex_busses; multiplexer_adr++)
    {

      select_bus(multiplexer_adr, busfd, busnumber);

      // first PCF 8574 P
      for (i = 32; i <= 39; i++) {
                write_PCF8574(busnumber, i,
          data->i2c_values[i][multiplexer_adr-1]);
      }

      // now PCF 8574 AP
      for (i = 56; i <= 63; i++) {
        write_PCF8574(busnumber, i,
          data->i2c_values[i][multiplexer_adr-1]);
      }
    }

    select_bus(0, busfd, busnumber);
  }
}

void select_bus(int mult_busnum, int busfd, long int busnumber) {

  int addr, value=0;

  //addr = 224 + (2 * (int) (mult_busnum / 9));
  addr = 224;
  if (mult_busnum > 8) mult_busnum = 0;

  if (busses[busnumber].power_state == 1) value = 64;
  value = value | (mult_busnum % 9);
  write_PCF8574(busnumber, (addr >> 1), value);
/*  ioctl(busfd, I2C_SLAVE, (addr >> 1));
  writeByte(busnumber, value, 1);*/
}

long int term_bus_I2C_DEV(long int bus)
{
  DBG(bus, DBG_INFO, "i2c-dev bus #%ld terminating"), bus;
  close(busses[bus].fd);
  return 0;
}

/*
*
* Initializes the I2C-Bus and sets fd for the bus
* If bus unavailible, fd = -1
*
*/
long int init_bus_I2C_DEV(long int i)
{

  I2CDEV_DATA *data = (I2CDEV_DATA *)busses[i].driverdata;
  int ga_hardware_inverters;
  int j, multiplexer_adr;
  int multiplex_busses;
  char buf;

  DBG(i, DBG_INFO, "i2c-dev init: bus #%ld, debug %d", i,
  busses[i].debuglevel);


  // init the hardware interface
  if (busses[i].debuglevel < 6) {
    busses[i].fd = init_lineI2C_DEV(i);
  } else {
    busses[i].fd = -1;
  }


  // init software
  ga_hardware_inverters = data->ga_hardware_inverters;
  multiplex_busses = data->multiplex_busses;

  memset(data->i2c_values, 0, sizeof ( I2C_DEV_VALUES ));

  if(ga_hardware_inverters == 1) {
    buf = 0xFF;
  } else {
    buf = 0x00;
  }

  // preload data->i2c_values

  if (multiplex_busses > 0) {

    for (multiplexer_adr = 1; multiplexer_adr <= multiplex_busses; multiplexer_adr++)
    {
      // first PCF 8574 P
      for (j = 32; j <= 39; j++) {
        data->i2c_values[j][multiplexer_adr-1] = buf;
      }

      // now PCF 8574 AP
      for (j = 56; j <= 63; j++) {
        data->i2c_values[j][multiplexer_adr-1] = buf;
      }
    }

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
    char msg[1000];

  long int bus = (long int) v;

  struct _GASTATE gatmp;

  I2CDEV_DATA *data = busses[bus].driverdata;

  int ga_reset_devices = data->ga_reset_devices;


  DBG(bus, DBG_INFO, "i2c-dev started, bus #%ld, %s", bus,
      busses[bus].device);

  busses[bus].watchdog = 1;


  // command processing starts here

    while (1) {

    // process POWER changes
    if (busses[bus].power_changed == 1) {

      // dummy select, power state is directly read by select_bus()
      select_bus(0, busses[bus].fd, bus);
      busses[bus].power_changed = 0;
      infoPower(bus, msg);
      queueInfoMessage(msg);

      if((ga_reset_devices == 1) && (busses[bus].power_state == 1)) {
        reset_ga(bus, busses[bus].fd);
      }

    }

    // do nothing, if power off
    if(busses[bus].power_state==0) {
      usleep(1000);
      continue;
    }

    busses[bus].watchdog = 4;

    // process GA commands
    if (!queue_GA_isempty(bus)) {
      unqueueNextGA(bus, &gatmp);
      handle_i2c_set_ga(bus, &gatmp);
      setGA(bus, gatmp.id, gatmp);
      select_bus(0, busses[bus].fd, bus);
      busses[bus].watchdog = 6;
    }
    usleep(1000);
  }
}
#endif
