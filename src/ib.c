/***************************************************************************
                          intelliboxd.c  -  description
                             -------------------
    begin                : Don Apr 19 17:35:13 MEST 2001
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>
#include <sys/io.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "config-srcpd.h"
#include "ib.h"
#include "io.h"
#include "srcp-fb-s88.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-time.h"

extern const char *WELCOME_MSG;
extern char *DEV_COMPORT;

extern int	CMDPORT; 	
extern int	FBPORT; 	
extern int	INFOPORT;
extern int	NUMBER_FB;

#ifdef TESTMODE
extern int testmode;
#endif

speed_t baud;				// Baudrate der seriellen Schnittstelle

extern volatile int server_reset;
extern volatile int power_state;
extern          char power_msg[200];
extern volatile int power_changed;
extern volatile int server_shutdown;

extern volatile int commands_gl;
extern volatile int sending_gl;
extern volatile int commands_ga;
extern volatile int sending_ga;

extern volatile struct _GL gl[MAXGLS];
extern volatile struct _GL ngl[50];		
extern volatile struct _GL ogl[50];		
extern volatile struct _GA ga[MAXGAS];
extern volatile struct _GA nga[50]; 		
extern volatile struct _GA oga[50];		
extern volatile struct _GA tga[50];
extern volatile int fb[MAXFBS];
extern int file_descriptor[NUMBER_SERVERS];

void* thr_sendrecintellibox(void *v)
{
  int i, i1;
  int temp;
  int addr;
  int commands_ok;
  int fd;
  unsigned char byte2send;
  unsigned char status;
  unsigned char rr;
  unsigned char xevnt1, xevnt2, xevnt3;
  struct _GL gltmp;
  struct _GA gatmp;
  struct timeval akt_time, cmp_time;
		
  int zaehler1, fb_zaehler1, fb_zaehler2;
	
#ifndef TESTMODE
  syslog(LOG_INFO, "thr_sendrecintellibox gestartet");
#else
  syslog(LOG_INFO, "thr_sendrecintellibox gestartet, testmode=%i",testmode);
#endif

  fd = file_descriptor[SERVER_IB];
  zaehler1 = 0;
  fb_zaehler1 = 0;
  fb_zaehler2 = 1;

  while(1)
  {
	//	syslog(LOG_INFO, "thr_sendrecintellibox Start in Schleife");
		/* Start/Stop */
		//fprintf(stderr, "START/STOP... ");
		if(power_changed)
		{
			byte2send = power_state ? 0xA7 : 0xA6;
  	  writeByte(fd, &byte2send, 250);
    	readByte(fd, &status);
	    if(status == 0x00)									// war alles OK ?
		    power_changed = 0;
		}
		/* Lokdecoder */
		//fprintf(stderr, "LOK's... ");
		/* nur senden, wenn wirklich etwas vorliegt */
		if(commands_gl)
		{
			commands_ok = 1;
			sending_gl = 1;
			for(i=0; i<50; i++)
			{
				if(ngl[i].id)
				{
	    		gltmp = ngl[i];
			    addr  = gltmp.id;
			    // Fahrrichtung, Geschwindigkeit od. eine Funktion geändert ?
			    if((gltmp.direction != gl[addr].direction) ||
	  		  	 (gltmp.speed != gl[addr].speed) ||
	    		   (gltmp.flags != gl[addr].flags))
			    { 	
			    	// Lokkommando soll gesendet werden
						byte2send = 0x80;
						writeByte(fd, &byte2send,0);	
						// lowbyte der Adresse senden
						temp = gltmp.id;
						temp &= 0x00FF;		    	
						byte2send = temp;
						writeByte(fd, &byte2send,0);			
						// highbyte der Adresse senden
						temp = gltmp.id;
						temp >>= 8;
						byte2send = temp;
						writeByte(fd, &byte2send,0); 			
			    	if(gltmp.direction == 2) 			// Nothalt ausgelöst ?
						{	
							byte2send = 1;							// Nothalt setzen
						}
						else
						{
							byte2send = calcspeed(gltmp.speed, gltmp.maxspeed, 126); // Geschwindigkeit senden
						  if(byte2send > 0)
						  {
						    byte2send++;
						  }
						}
						writeByte(fd, &byte2send,0); 			
						// Richtung, Licht und Funktionen setzen
						byte2send = gltmp.flags;
						byte2send |= 0xc0;
						if(gltmp.direction)
						{	
							byte2send |= 0x20;
						}
						writeByte(fd, &byte2send,2);
			      readByte(fd, &status);
			      if((status == 0) || (status == 0x41) || (status == 0x42))
	  		    {
					    ngl[i].id = 0;
			  		  gl[addr] = gltmp;
						} 	
						else
						{
							if((status == 2) || (status == 0x0C))
							{	
							  ngl[i].id = 0;
							}
						}
		    	}
				}
			}
			sending_gl = 0;
			if(commands_ok)
			{
				commands_gl = 0;
			}
		}
		
		gettimeofday(&akt_time, NULL);
		// zuerst eventuell Decoder abschalten
		for(i=0;i<50;i++)
		{
			if(tga[i].id)
			{
				syslog(LOG_INFO, "Zeit %i,%i", (int)akt_time.tv_sec, (int)akt_time.tv_usec);
				cmp_time = tga[i].t;
				if(cmpTime(&cmp_time, &akt_time))			// Ausschaltzeitpunkt erreicht ?
				{
					gatmp = tga[i];
		  	  byte2send = 0x90;
		    	writeByte(fd, &byte2send,0);
			    temp = gatmp.id;
		  	  temp &= 0x00FF;
		    	byte2send = temp;
			    writeByte(fd, &byte2send,0);
		  	  temp = gatmp.id;
		    	temp >>= 8;
			    byte2send = temp;
		  	  if(gatmp.port)
		    	{	
		    		byte2send |= 0x80;
		    	}
			    writeByte(fd, &byte2send, 2);
					readByte(fd, &rr);
					ga[addr]=gatmp;
					tga[i].id=0;
				}
			}			
		}
		
		// Decoder einschalten
		if(commands_ga)
		{
//			commands_ok = 1;
			sending_ga = 1;
			for(i=0;i<50;i++)
			{
	  		if(nga[i].id)
		  	{
					gatmp = nga[i];
					addr = gatmp.id;
		  	  byte2send = 0x90;
		    	writeByte(fd, &byte2send,0);
			    temp = gatmp.id;
		  	  temp &= 0x00FF;
		    	byte2send = temp;
			    writeByte(fd, &byte2send,0);
		  	  temp = gatmp.id;
		    	temp >>= 8;
			    byte2send = temp;
			    if(gatmp.action)
		    	{	
		    		byte2send |= 0x40;
		  	  }
		  	  if(gatmp.port)
		    	{
		    		byte2send |= 0x80;
			    }
			    writeByte(fd, &byte2send, 0);
			    status = 1;
					if(gatmp.action && (gatmp.activetime > 0))
					{
						for(i1=0;i1<50;i1++)
						{
							if(tga[i1].id == 0)
							{
								gatmp.t = akt_time;
								gatmp.t.tv_sec += gatmp.activetime / 1000;
								gatmp.t.tv_usec += (gatmp.activetime % 1000) * 1000;
								if(gatmp.t.tv_usec > 1000000)
								{
									gatmp.t.tv_sec++;
									gatmp.t.tv_usec -= 1000000;
								}
								tga[i] = gatmp;
								status = 0;
								syslog(LOG_INFO, "GA %i für Abschaltung um %i,%i auf %i", tga[i].id, (int)tga[i].t.tv_sec, (int)tga[i].t.tv_usec, i);
								break;
							}
						}
					}
					readByte(fd, &rr);
					if(status)
						ga[addr]=gatmp;
					nga[i].id=0;
				}
			}
			sending_ga = 0;
			commands_ga = 0;
		}
		/* Abfrage auf Statusänderungen :
			 1. Änderungen an S88-Modulen
			 2. manuelle Lokbefehle
			 3. manuelle Weichenbefehle */
		byte2send = 0xC8;
	  writeByte(fd, &byte2send, 2);
  	xevnt2 = 0x00;
	  xevnt3 = 0x00;
		readByte(fd, &xevnt1);
		if(xevnt1 & 0x80)
		{
			readByte(fd, &xevnt2);
			if(xevnt2 & 0x80)
				readByte(fd, &xevnt3);
		}
		
		if(xevnt1 & 0x01)				// mindestens eine Lok wurde von Hand gesteuert
		{
			byte2send = 0xC9;
			writeByte(fd, &byte2send,2);
			readByte(fd, &rr);
			while(rr != 0x80)
			{
				if(rr == 1)
				{
					gltmp.speed = 0;				// Lok befindet sich im Nothalt
					gltmp.direction = 2;
				}
				else
				{
					gltmp.speed = rr;				// Lok fährt mit dieser Geschwindigkeit
					gltmp.direction = 0;
					if(gltmp.speed > 0)
						gltmp.speed--;
				}
				readByte(fd, &rr);
				gltmp.flags = rr & 0xf0;;
				readByte(fd, &rr);
				gltmp.id = rr;
				readByte(fd, &rr);
				if((rr & 0x80) && (gltmp.direction == 0))
					gltmp.direction = 1;		// Richtung ist vorwärts
				if(rr & 0x40)
					gltmp.flags |= 0x010;		// Licht ist an
				rr &= 0x3F;
				gltmp.id |= rr << 8;
				for(i=0;i<50;i++)
					if(ogl[i].id == 0)
					{
						ogl[i] = gltmp;
						break;
					}
				readByte(fd, &rr);
				readByte(fd, &rr);
			}			
		}
	
		if(xevnt1 & 0x04)				// mindestens eine Rückmeldung hat sich geändert
		{
			byte2send = 0xCB;
			writeByte(fd, &byte2send,2);
			readByte(fd, &rr);
			while(rr != 0x00)
			{
				rr--;
				i = rr;
				readByte(fd, &rr);
				temp = rr;
				temp <<= 8;	
				readByte(fd, &rr);
				fb[i] = temp | rr;
				readByte(fd, &rr);
        syslog(LOG_INFO, "Rückmeldung %i mit 0x%02x", i, fb[i]);
			}
		}
	
		if(xevnt1 & 0x20)				// mindestens eine Weiche wurde von Hand geschaltet
		{
			byte2send = 0xCA;
			writeByte(fd, &byte2send,0);
			readByte(fd, &rr);
			temp = rr;
			for(i=0;i<temp;i++)			
			{
				readByte(fd, &rr);
				gatmp.id = rr;
				readByte(fd, &rr);
				gatmp.id |= (rr & 0x07) << 8;
				for(addr=0;addr<50;addr++)
					if(oga[addr].id == 0)
					{
						oga[addr] = gatmp;
						break;
					}
			} 	
		}
#ifdef TESTMODE
		if(testmode)
		{
			zaehler1++;
			if(zaehler1 > 10000)
			{
				zaehler1 = 0;
				fb[fb_zaehler1] = fb_zaehler2;
				if(fb_zaehler2 == 0)
				{	
					fb_zaehler2 = 0x8000;
					fb_zaehler1++;
					if(fb_zaehler1 > 30)
						fb_zaehler1 = 0;
				}
				else
					fb_zaehler2 >>= 1;
			}
		}
#endif
	}			// Ende WHILE(1)
}

int init_comport(char *name)
{
	int status;
	int fd;
	unsigned char rr;
	struct termios interface;

	printf("try opening serial line %s for %i baud\n", name, (2400 * (1 << (baud-11))));
	fd = open(name,O_RDWR);
	if (fd == -1)
	{
		printf("dammit, couldn't open device.\n");
	}
  else
	{
		tcgetattr(fd, &interface);
		interface.c_oflag = ONOCR;
		interface.c_cflag = CS8 | CRTSCTS | CSTOPB | CLOCAL | CREAD | HUPCL;
		interface.c_iflag = IGNBRK;
		interface.c_lflag = IEXTEN;
		cfsetispeed(&interface, baud);
		cfsetospeed(&interface, baud);
		interface.c_cc[VMIN] = 0;
		interface.c_cc[VTIME] = 1;
		tcsetattr(fd, TCSANOW, &interface);
		status = 0;
		sleep(1);
#ifdef TESTMODE
		if(testmode == 0)
#endif
		while(status != -1)
			status = readByte(fd, &rr);
	}
	return fd;
}

int open_comport(int *file, char *name)
{
  int fd;
	int status;
	unsigned short int LSR;
	unsigned char rr;
	unsigned char byte2send;
	struct termios interface;
	
	printf("Begin detecting IB on serial line: %s\n", name);

#ifdef TESTMODE
	if(testmode)	
		return 0;				// testmode, also is a virtual IB always availible
#endif

	printf("Opening serial line %s for 2400 baud\n", name);
	fd=open(name,O_RDWR);
	if (fd == -1)
	{
		printf("dammit, couldn't open device.\n");
		return 1;
	}
	tcgetattr(fd, &interface);
	interface.c_oflag = ONOCR;
	interface.c_cflag = CS8 | CRTSCTS | CSTOPB | CLOCAL | CREAD | HUPCL;
	interface.c_iflag = IGNBRK;
	interface.c_lflag = IEXTEN;
	cfsetispeed(&interface, B2400);
	cfsetospeed(&interface, B2400);
	interface.c_cc[VMIN] = 0;
	interface.c_cc[VTIME] = 1;
	tcsetattr(fd, TCSANOW, &interface);
	
	status = 0;
	sleep(1);
	printf("clearing input-buffer\n");
	while(status != -1)
		status = readByte(fd, &rr);

	close(fd);	
	
	sleep(1);
	printf("sending BREAK\n");
	LSR = 0x3F8 + 3;
	status = ioperm(LSR, 1, 1);
	usleep(200000);
	outb((inb(LSR) | 0x040), LSR);
	usleep(1000000);
	outb((inb(LSR) & 0x0BF), LSR);
	usleep(600000);
	ioperm(LSR, 1, 0);
	sleep(1);
	printf("BREAK sucessfully send\n");
	
		
	baud = B2400;
	fd = init_comport(name);
	if(fd < 0)
	{
		printf("init comport fehlgeschlagen\n");
		return(-1);
	}
	byte2send = 0xC4;
	writeByte(fd, &byte2send,2);
	status = readByte(fd, &rr);
	if(status == -1)
		return(1);
  if(rr=='D')
	{
		printf("Intellibox in download mode.\nDO NOT PROCEED !!!\n");
		return(2);
	}
	printf("switch of P50-commands\n");
	byte2send = 'x';
	writeByte(fd, &byte2send, 0);
	byte2send = 'Z';
	writeByte(fd, &byte2send, 0);
	byte2send = 'z';
	writeByte(fd, &byte2send, 0);
	byte2send = 'A';
	writeByte(fd, &byte2send, 0);
	byte2send = '1';
	writeByte(fd, &byte2send, 0);
	byte2send = 0x0d;
	writeByte(fd, &byte2send, 2);
  status = readByte(fd, &rr);
  if(status != 0)
  	return 1;
  printf("change baudrate to 38400 bps\n");
  byte2send = 'B';
  writeByte(fd, &byte2send, 0);	
  byte2send = '3';
  writeByte(fd, &byte2send, 0);	
  byte2send = '8';
  writeByte(fd, &byte2send, 0);	
  byte2send = '4';
  writeByte(fd, &byte2send, 0);	
  byte2send = '0';
  writeByte(fd, &byte2send, 0);	
	writeByte(fd, &byte2send, 0);
	byte2send = 0x0d;
  writeByte(fd, &byte2send, 0);	
	
  sleep(1);
	close_comport(fd);
		
	baud = B38400;
	fd = init_comport(name);
	if(fd < 0)
	{
		printf("init comport fehlgeschlagen\n");
		return(-1);
	}
	byte2send = 0xC4;
	writeByte(fd, &byte2send,2);
	status = readByte(fd, &rr);
	if(status == -1)
		return(1);
  if(rr=='D')
	{
		printf("Intellibox in download mode.\nDO NOT PROCEED !!!\n");
		return(2);
	}
	
	*file = fd;	
	return 0;
}

void close_comport(int fd)
{
	struct termios interface;
	syslog(LOG_INFO,"Closing serial line");

	tcgetattr(fd, &interface);
	cfsetispeed(&interface, B0);
	cfsetospeed(&interface, B0);
	tcsetattr(fd, TCSANOW, &interface);
	close(fd);
}
