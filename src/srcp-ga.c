/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * This software is published under the restrictions of the 
 * GNU License Version2
 *
 * Ostern 2002, Matthias Trute
 *            - echte Kommandoqueue anstelle der Vormerkungen.
 *
 * 04.07.2001 Frank Schmischke
 *            - Feld für Vormerkungen wurde auf 50 reduziert
 *            - Position im Feld für Vormerkung ist jetzt unabhängig von der
 *              Decoderadresse
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/time.h>
#include <pthread.h>

#include "config-srcpd.h"
#include "srcp-ga.h"

#define QUEUELEN 500

volatile struct _GA ga[MAXGAS];	// soviele Generic Accessoires gibts
static struct _GA queue[QUEUELEN];	// Kommandoqueue.
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;	/* mutex to synchronize queue inserts */
static int out = 0, in = 0;

extern int NUMBER_GA;

static int queue_len();
static int queue_isfull();

int queueGA(char *prot, int addr, int port, int aktion, long activetime)
{
    struct timeval akt_time;
    
    if ((addr > 0) && (addr <= NUMBER_GA)) {
	while (queue_isfull()) {
	    usleep(1000);
	}

	pthread_mutex_lock(&queue_mutex);
	strcpy((void *) queue[in].prot, prot);
	queue[in].action = aktion;
	queue[in].port = port;
	queue[in].activetime = activetime;
	gettimeofday(&akt_time, NULL);
	queue[in].tv[port] = akt_time;
	queue[in].id = addr;
	in++;
	if (in == QUEUELEN)
	    in = 0;
	pthread_mutex_unlock(&queue_mutex);
    } else {
	return -1;
    }
    return 0;
}

int queue_ga_empty()
{
    return (in == out);
}

static int queue_len()
{
    if (in >= out)
	return in - out;
    else
	return QUEUELEN + in - out;
}

/* maybe, 1 element in the queue cannot be used.. */
static int queue_isfull()
{
    return queue_len() >= QUEUELEN - 1;
}

/** liefert nächsten Eintrag und >=0, oder -1 */
int getNextGA(struct _GA *ga)
{
    if (in == out)
	return -1;
    *ga = queue[out];
    return out;
}

/** liefert nächsten Eintrag oder -1, setzt fifo pointer neu! */
int unqueueNextGA(struct _GA *ga)
{
    if (in == out)
	return -1;

    *ga = queue[out];
    out++;
    if (out == QUEUELEN)
	out = 0;
    return out;
}

int getGA(char *prot, int addr, struct _GA *a)
{
    if ((addr > 0) && (addr <= NUMBER_GA)) {
	*a = ga[addr];
	return 0;
    } else {
	return 1;
    }
}

int infoGA(struct _GA a, char *msg)
{
    sprintf(msg, "INFO GA %s %d %d %d %ld\n", a.prot, a.id, a.port,
	    a.action, a.activetime);
    return 0;
}

int cmpGA(struct _GA a, struct _GA b)
{
    return ((a.action == b.action) && (a.port == b.port));
}

void initGA()
{
    int i, error;
    for (i = 0; i < MAXGAS; i++) {
	strcpy((void *) ga[i].prot, "PS");
	ga[i].id = i;
    }
    in = 0;
    out = 0;
}
