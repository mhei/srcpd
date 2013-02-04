/* $Id: dccar.c 2012-02-19 15:00 achim_marikar $ */

/*
 *  Copyright 2012-2013 Achim Marikar <amarikar@users.sourceforge.net>
 *  Copyright 2003-2007 Matthias Trute <mtrute@users.sourceforge.net>
 *  Copyright 2004 Frank Schimschke <schmischi@users.sourceforge.net>
 *  Copyright 2005 Johann Vieselthaler <tpdap@users.sourceforge.net>
 *  Copyright 2009 Joerg Rottland <rottland@users.sourceforge.net>
 *  Copyright 2005-2009 Guido Scholz <gscholz@users.sourceforge.net>
 * 
 *  This file is part of srcpd.
 * 
 *  srcpd is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 * 
 *  srcpd is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with srcpd.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * dccar: dc-car pc-sender hardware
 */

#include <errno.h>

#include "config-srcpd.h"
#include "dccar.h"
#include <fcntl.h>
#include "toolbox.h"
#include "srcp-gl.h"
#include "srcp-sm.h"
#include "srcp-power.h"
#include "srcp-server.h"
#include "srcp-info.h"
#include "srcp-error.h"
#include "syslogmessage.h"
#include <math.h>

#define __dccar ((DCCAR_DATA*)buses[busnumber].driverdata)

int readconfig_DCCAR(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber)
{
	buses[busnumber].driverdata = malloc(sizeof(struct _DCCAR_DATA));
	
	if (buses[busnumber].driverdata == NULL) {
		syslog_bus(busnumber, DBG_ERROR,
			"Memory allocation error in module '%s'.", node->name);
		return 0;
	}
	
	buses[busnumber].type = SERVER_DCCAR;
	buses[busnumber].init_func = &init_bus_DCCAR;
	buses[busnumber].thr_func = &thr_sendrec_DCCAR;
	buses[busnumber].init_gl_func = &init_gl_DCCAR;
	strcpy(buses[busnumber].description,
		"GL POWER LOCK DESCRIPTION");

	__dccar->mode = DCCAR;
	__dccar->pause_between_cmd = 10;
 
	xmlNodePtr child = node->children;
	xmlChar *txt = NULL;
 
	while (child != NULL) {
		if ((xmlStrncmp(child->name, BAD_CAST "text", 4) == 0) ||
			(xmlStrncmp(child->name, BAD_CAST "comment", 7) == 0)) {
		/* just do nothing, it is only formatting text or a comment */
	}

	else if (xmlStrcmp(child->name, BAD_CAST "number_gl") == 0) {
		txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
		if (txt != NULL) {
			__dccar->number_gl = atoi((char *) txt);
			xmlFree(txt);
		}
	}

	else if (xmlStrcmp(child->name, BAD_CAST "pause_between_commands") == 0) {
		txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
		if (txt != NULL) {
			__dccar->pause_between_cmd = atoi((char *) txt);
			xmlFree(txt);
		}
	}

	else if (xmlStrcmp(child->name, BAD_CAST "mode") == 0) {
		txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
		if (txt != NULL) {
			if (xmlStrcmp(txt, BAD_CAST "dccar") == 0)
				__dccar->mode = DCCAR;
			else if (xmlStrcmp(txt, BAD_CAST "infracar") == 0)						 
				__dccar->mode = INFRACAR;
				xmlFree(txt);
		}
	}

	else
		syslog_bus(busnumber, DBG_WARN,
			"WARNING, unknown tag found: \"%s\"!\n",
			child->name);;
		child = child->next;
	}

	if (__dccar->mode == DCCAR && __dccar->number_gl > 1023)
		__dccar->number_gl = 1023;
	else if (__dccar->mode == INFRACAR && __dccar->number_gl > 4096)
		__dccar->number_gl = 4096;

	if (init_GL(busnumber, __dccar->number_gl)) {
		__dccar->number_gl = 0;
		syslog_bus(busnumber, DBG_ERROR,
			"Can't create array for locomotives");
	}

	return (1);
}

int init_linedccar(bus_t busnumber, char *name)
{
	struct termios interface;
	syslog_bus(busnumber, DBG_INFO, "Try opening serial line %s \n", name);
	fd = open(name, O_RDWR);
	if (fd == -1) {
		syslog_bus(busnumber, DBG_ERROR, "Open serial line failed: %s (errno = %d).\n", strerror(errno), errno);
	}
	else {
		tcgetattr(fd, &interface);

		if (__dccar->mode == DCCAR) {		/* dc-car: 9600 bps, 8 bit, odd parity, 2 stopbits */
			interface.c_cflag = PARENB | PARODD | CSTOPB | CSIZE | CS8 | CLOCAL;
			cfsetispeed(&interface, B9600);
			cfsetospeed(&interface, B9600);
		}
		else {									/* infracar: 2400 bps, 8 Bit, no parity, 1 stopbits */
			interface.c_cflag = CSIZE | CS8 | CLOCAL;
			cfsetispeed(&interface, B2400);
			cfsetospeed(&interface, B2400);	
		}
		
		interface.c_cc[VMIN] = 0;
		interface.c_cc[VTIME] = 1;
		tcsetattr(fd, TCSANOW, &interface);
	}
	return fd;
}

/* Initialisiere den Bus, signalisiere Fehler */
/* Einmal aufgerufen mit busnummer als einzigem Parameter */
/* return code wird ignoriert (vorerst) */
int init_bus_DCCAR(bus_t busnumber)
{
	static char *protocols = "MN";
	buses[busnumber].protocols = protocols;
	syslog_bus(busnumber, DBG_INFO, "DC-Car init: debug %d, device %s",
		buses[busnumber].debuglevel, buses[busnumber].device.file.path);
	buses[busnumber].device.file.fd =
		init_linedccar(busnumber, buses[busnumber].device.file.path);
	syslog_bus(busnumber, DBG_INFO, "DC-Car init done");
	return 0;
}

/**
 * cacheInitGL: modifies the gl data used to initialize the device
 **/
int init_gl_DCCAR(gl_state_t * gl)
{
	/* TODO instead of setting mode (dccar or infracar) in config file, the mode could be individual specified by protocol of car to enable mixed mode.*/
	switch (gl->protocol) {
		case 'L':
		case 'S':             
		case 'P':
		case 'M':
		case 'N':
		default:
			return (gl->n_fs == 28) ? SRCP_OK : SRCP_WRONGVALUE;
		break;
	}
	return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
}

static void handle_power_command(bus_t busnumber)
{
	buses[busnumber].power_changed = 0;
	char msg[110];

	infoPower(busnumber, msg);
	enqueueInfoMessage(msg);
	buses[busnumber].watchdog++;
	
	if ( buses[busnumber].power_state == 0 && __dccar->mode == DCCAR ) {
		unsigned char command[] = {240,240,0};
		write_command(command, sizeof(command), fd, busnumber);
	}
}

void set_address_bytes(unsigned char command[3], int id) {
	command[0] = (unsigned char) ( id % 64 );			/* byte 1, address low */
	command[1] = (unsigned char) ( id / 64 ) + 64;	/* byte 2, address high, add 64 for speed and functions F1-F6, 96 for functions F0, F7-F11 and 112 for functions F12 and greater */
}

void write_command(unsigned char *command, int length, int fd, bus_t busnumber)
{
	if (!write(fd,command,length))
		syslog_bus(busnumber, DBG_ERROR, "could not send data to device");
	/* the cars need a short delay between two commands */
	usleep(__dccar->pause_between_cmd * 1000);
}

static void handle_gl_command(bus_t busnumber)
{
	gl_state_t gltmp, glakt;
	int addr;

	/* byte 1: address low, byte 2: address high, byte 3: speed/functions (see http://www.dc-car.de/DC-Car_PC_Sender.htm & http://www.modellautobahnen.de/newsletter/download/wdp_dc-car_steuerung2.pdf for details) */
	unsigned char command[3];

	dequeueNextGL(busnumber, &gltmp);
	addr = gltmp.id;
	cacheGetGL(busnumber, addr, &glakt);

	if (gltmp.direction == 2) {
		gltmp.speed = 0;
		gltmp.direction = !glakt.direction;
	}
	
	/* send speed message to car */
	if (gltmp.speed != glakt.speed) {
		set_address_bytes(command,gltmp.id);
		
		if (gltmp.speed < 8)
			command[2] = gltmp.speed + 200;
		else if (gltmp.speed < 16)
			command[2] = gltmp.speed + 208;
		else if (gltmp.speed < 24)
			command[2] = gltmp.speed + 216;
		else if (gltmp.speed <= 28)
			command[2] = gltmp.speed + 224;
		else
			command[2] = 200;
		
		write_command(command, sizeof(command), fd, busnumber);
	}

	/* send function message to car */
	if (gltmp.funcs != glakt.funcs) {
		unsigned char byte;

		/* separate functions to different messages with offset for byte 2 0x40 (default), 0x60 (+32) or 0x70 (+64, currently not implemented) */
		if( (gltmp.funcs&126) != (glakt.funcs&126) ) {
			set_address_bytes(command,gltmp.id);
			byte = 128;

			if(gltmp.funcs & 2)
				byte = byte + 1;
			if(gltmp.funcs & 4)
				byte = byte + 2;
			if(gltmp.funcs & 8)
				byte = byte + 4;
			if(gltmp.funcs & 16)
				byte = byte + 8;
			if(gltmp.funcs & 32)
				byte = byte + 16;
			if(gltmp.funcs & 64)
				byte = byte + 32;
			
			command[2]=byte;
			write_command(command, sizeof(command),fd, busnumber);
		}

		if( ((gltmp.funcs&3969) != (glakt.funcs&3969)) && (__dccar->mode == DCCAR) ) {
			set_address_bytes(command,gltmp.id);
			command[1]=command[1] + 32;
			byte = 128;
				
			if(gltmp.funcs & 1)
				byte = byte + 1;
			if(gltmp.funcs & 128)
				byte = byte + 2;
			if(gltmp.funcs & 256)
				byte = byte + 4;
			if(gltmp.funcs & 512)
				byte = byte + 8;
			if(gltmp.funcs & 1024)
				byte = byte + 16;
			if(gltmp.funcs & 2048)
				byte = byte + 32;
		
			command[2]=byte;
			write_command(command, sizeof(command), fd, busnumber);
		}
	}

	cacheSetGL(busnumber, addr, gltmp);
	buses[busnumber].watchdog++;
}

/*thread cleanup routine for this bus*/
static void end_bus_thread(bus_thread_t * btd)
{	
	int result;

	syslog_bus(btd->bus, DBG_INFO, "DC-Car bus terminated.");
	result = pthread_mutex_destroy(&buses[btd->bus].transmit_mutex);
	if (result != 0) {
		syslog_bus(btd->bus, DBG_WARN,
		  "pthread_mutex_destroy() failed: %s (errno = %d).",
		strerror(result), result);
	}

	result = pthread_cond_destroy(&buses[btd->bus].transmit_cond);
	if (result != 0) {
		syslog_bus(btd->bus, DBG_WARN,
		  "pthread_mutex_init() failed: %s (errno = %d).",
		strerror(result), result);
	}

	free(buses[btd->bus].driverdata);
	free(btd);
}

/*main thread routine for this bus*/
void *thr_sendrec_DCCAR(void *v)
{
	int addr, ctr;
	struct timeval akt_time, cmp_time;
	ga_state_t gatmp;
	int last_cancel_state, last_cancel_type;

	bus_thread_t *btd = (bus_thread_t *) malloc(sizeof(bus_thread_t));
	
	if (btd == NULL)
		pthread_exit((void *) 1);
	btd->bus = (bus_t) v;
	btd->fd = -1;

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last_cancel_state);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &last_cancel_type);

	/*register cleanup routine */
	pthread_cleanup_push((void *) end_bus_thread, (void *) btd);

	syslog_bus(btd->bus, DBG_INFO, "DC-Car bus started (device = %s).",
		buses[btd->bus].device.file.path);

	/*enter endless loop to process work tasks */
	while (true) {
		buses[btd->bus].watchdog = 1;

		/*POWER action arrived */
		if (buses[btd->bus].power_changed == 1)
			handle_power_command(btd->bus);

		/* loop shortcut to prevent processing of GA, GL (and FB)
		 * without power on; arriving commands will flood the command
		 * queue */
		if (buses[btd->bus].power_state == 0) {

			/* wait 1 ms */
			if (usleep(1000) == -1) {
				syslog_bus(btd->bus, DBG_ERROR,
				  "usleep() failed: %s (errno = %d)\n",
				  strerror(errno), errno);
			}
			continue;
		}

		/*GL action arrived */
		if (!queue_GL_isempty(btd->bus))
			handle_gl_command(btd->bus);

		/*FB action arrived */
		/* currently nothing to do here */
		buses[btd->bus].watchdog++;

		/* busy wait and continue loop */
		/* wait 1 ms */
		if (usleep(1000) == -1) {
			syslog_bus(btd->bus, DBG_ERROR,
			  "usleep() failed: %s (errno = %d)\n",
				strerror(errno), errno);
		}
	}

	/*run the cleanup routine */
	pthread_cleanup_pop(1);
	return NULL;
}
