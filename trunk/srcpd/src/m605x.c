/* cvs: $Id$             */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


/* Die Konfiguration des seriellen Ports von M6050emu (D. Schaefer)   */
/* wenngleich etwas verändert, mea culpa..                            */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>

#include "config.h"
#include "m605x.h"
#include "srcp-fb-m6051.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-srv.h"

/* Folgende Werte lassen sich z.T. verändern, siehe Kommandozeilen/Configdatei */
/* unveränderliche sind mit const markiert                                     */

/* das Programm arbeitet mit vielen Threads. und in der C-Fibel steht was
 * von volatile, schadet vermutlich nicht.
 */
volatile int io_thread_running = 0; /* eine Art Wachhund, siehe main()     */
volatile int server_shutdown_state   = 0; /* wird gesetzt und beendet den Server */
volatile int server_reset_state      = 0; /* Reset des Servers, unimplemented    */

/* Datenaustausch, zugeschnitten auf den 6051 von Märklin (tm?) */
volatile int cmd32_pending     = 0; /* Als nächstes muß ein 32 abgesetzt werden */

/* einige Zeitkonstanten, alles Millisekunden */
int ga_min_active_time = 75;
int pause_between_cmd  = 200;
int pause_between_bytes  = 2;

/********************************************************
 * von dem Thread gibt es genau einen pro Interface
 * also wohl nur einen überhaupt..
 * Datenaustausch über einige globale
 * Speicherbereiche..
 ********************************************************
 */
/*******************************************************
 *     SERIELLE SCHNITTSTELLE KONFIGURIEREN           
 *******************************************************
 */
int init_line6051(char *name) {
    int FD;
    struct termios interface;
    if(debuglevel)
	syslog(LOG_INFO, "Opening 605x: %s", name);
    if ((FD=open(name,O_RDWR)) == -1) {
	syslog(LOG_INFO,"couldn't open device.");
	return(-1);
    }
    tcgetattr(FD, &interface);
    interface.c_oflag = ONOCR | ONLRET;
    interface.c_oflag &= ~(OLCUC| ONLCR | OCRNL);
    interface.c_cflag = CS8 | CRTSCTS ;
    interface.c_cflag &= ~(CSTOPB| PARENB);
    interface.c_iflag = IGNBRK | IGNPAR;
    interface.c_iflag &= ~(ISTRIP | IXON | IXOFF | IXANY);
    interface.c_lflag = NOFLSH | IEXTEN;
    interface.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | TOSTOP | PENDIN);
    cfsetospeed(&interface, B2400);
    cfsetispeed(&interface, B2400);
    tcsetattr(FD, TCSAFLUSH, &interface);

    return(FD);
}

unsigned char readbyte6051(int FD) {
    int i;
    unsigned char the_byte;
    i = read(FD, &the_byte, 1);
    return (the_byte);
}

void writebyte6051(int FD, unsigned char b, unsigned long msecs) {
    write(FD, &b, 1);
    tcflush(FD, TCOFLUSH);
    usleep(msecs*1000);
}

/* es gibt für einige Dekodder 27 FS, für andere 14! */
int calcspeed(int vs, int vmax, int n_fs) {
    int rs;
    if(0==vmax) 
	return vs;
    if(vs<0)
	vs = 0;
    if(vs>vmax)
	vs = vmax;
    rs = (vs * n_fs) / vmax;
    if((rs==0) && (vs!=0))
	rs = 1;
    return rs;
}
void* thr_sendrec6051(void *v) {
    unsigned char SendByte;
    int i, fd;
    char *dev = (char*) v;
    struct timezone dummytz;

    fd = init_line6051(dev);
    if(fd<0) {
	syslog(LOG_INFO, "Interface 6051 %s nicht vorhanden?!", dev);
	return (void*) fd;
    }
    /* erst mal alle Schaltaktionen canceln?, lieber nicht pauschal.. */
    io_thread_running = 1;
    if(cmd32_pending) {
	writebyte6051(fd, 32, pause_between_cmd);
	cmd32_pending = 0;
    }
    
    while(1) {
	io_thread_running = 2;
	/* Start/Stop */
	//fprintf(stderr, "START/STOP... ");
	if(power_changed) {
	    writebyte6051(fd, power_state?96:97, pause_between_cmd);
	    writebyte6051(fd, power_state?96:97, pause_between_cmd); /* zweimal, wir sind paranoid */
	    power_changed = 0;
	}
	io_thread_running = 3;
	/* Lokdecoder */
	//fprintf(stderr, "LOK's... ");
	/* primitiv alle Lokadressen durchhecheln */
	if(!cmd32_pending) {
	    for(i=1; i<MAXGLS; i++) {
		struct _GL gltmp;
		if(ngl_mm[i].id) {
		    char c;
		    gltmp = ngl_mm[i];
		    ngl_mm[i].id = 0;
		    if(gltmp.direction == 2) {
			gltmp.speed = 0;
			gltmp.direction = !gl_mm[i].direction;
		    }
		    // Vorwärts/Rückwärts
		    if(gltmp.direction != gl_mm[i].direction) {
			c = 15 + 16*( (gltmp.flags & 0x10)? 1 : 0);
			writebyte6051(fd, c, pause_between_bytes);
			writebyte6051(fd, gltmp.id, pause_between_cmd);
		    }
		    // Geschwindigkeit und Licht setzen, erst recht nach Richtungswechsel
		    if((gltmp.speed != gl_mm[i].speed) || 
		       ((gltmp.flags & 0x10) != (gl_mm[i].flags & 0x10)) ||
		       (gltmp.direction != gl_mm[i].direction) ){
			c = calcspeed(gltmp.speed, gltmp.maxspeed, gltmp.n_fs) + 16*( (gltmp.flags & 0x10)? 1 : 0);
			/* jetzt aufpassen: n_fs erzwingt ggf. mehrfache Ansteuerungen des Dekoders */
			/* das Protokoll ist da wirklich eigenwillig, vorerst ignoriert!            */
			writebyte6051(fd, c, pause_between_bytes); 
			writebyte6051(fd, gltmp.id, pause_between_cmd); 
		    }
		    // Erweiterte Funktionen des 6021 senden, manchmal
		    if(M6020MODE==0 && gltmp.flags != gl_mm[i].flags) {
			c = (gltmp.flags & 0x0f) + 64;
			writebyte6051(fd, c, pause_between_bytes);
			writebyte6051(fd, gltmp.id, pause_between_cmd);
		    }
		    gettimeofday(&gltmp.tv, &dummytz);
		    gl_mm[i] = gltmp;
		}
		io_thread_running = 4;
	    }
	}
	io_thread_running = 5;
	/* Magnetantriebe, die muessen irgendwann sehr bald abgeschaltet werden */
	//fprintf(stderr, "und die Antriebe");
	for(i=1;i<MAXGAS;i++) {
	    struct _GA gatmp;
	    char c;
	    if(nga_mm[i].id) {
		gatmp = nga_mm[i];
		if(gatmp.action==1) {
		    gettimeofday(&gatmp.tv[gatmp.port], &dummytz);
		    ga_mm[i]=gatmp;
		    if(gatmp.activetime >= 0) {
			gatmp.activetime += (gatmp.activetime>ga_min_active_time)?0:ga_min_active_time; // mind. 75ms
			gatmp.action = 0; // nächste Aktion ist automatisches Aus
		    } else {
			gatmp.activetime = ga_min_active_time; // egal wieviel, mind. 75m ein
		    }
		    c = 33+(gatmp.port?0:1);
		    writebyte6051(fd, c, pause_between_bytes);
		    writebyte6051(fd, gatmp.id, (gatmp.activetime));
		    cmd32_pending = 1;
		}
		if((gatmp.action == 0) && cmd32_pending) {
		    // fprintf(stderr, "32 abzusetzen\n");
		    writebyte6051(fd, 32, pause_between_cmd);
		    cmd32_pending = 0;
		}
		gettimeofday(&gatmp.tv[gatmp.port], &dummytz);
		ga_mm[i]=gatmp;
		nga_mm[i].id=0;
		io_thread_running = 5;
	    }
	}
	io_thread_running = 6;
	//fprintf(stderr, "Feedback ...");
	/* S88 Status einlesen, einen nach dem anderen */
	if(!cmd32_pending && NUMBER_FB) {
	    SendByte = 128+NUMBER_FB;
	    writebyte6051(fd, SendByte, pause_between_bytes);
	    io_thread_running = 7;
	    for(i=1; i<=NUMBER_FB; i++) {
		fb[i].l = readbyte6051(fd);
		io_thread_running = 8;
		fb[i].h = readbyte6051(fd);
	    }
	    usleep(pause_between_cmd*1000);
	}
	io_thread_running = 9;
	// fprintf(stderr, " ende\n");
    }
    close(fd);
}

