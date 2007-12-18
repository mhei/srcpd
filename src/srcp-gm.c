/* $Id$ */

/*
 *
 */
#include "stdincludes.h"

#include "config-srcpd.h"
#include "srcp-error.h"
#include "srcp-info.h"
#include "srcp-gm.h"
#include "syslogmessage.h"


int setGM(sessionid_t sid, sessionid_t rid, char *msg)
{
    struct timeval akt_time;
    char *msgtmp;
    syslog_bus(0, DBG_DEBUG, "GM message: %s", msg);

    if (sid != 0 && is_valid_info_session(sid) == 0)
        return SRCP_WRONGVALUE;

    if (rid != 0 && is_valid_info_session(rid) == 0)
        return SRCP_WRONGVALUE;

    /*
    <time> 100 INFO 0 GM <send_to_id> <reply_to_id> <message_type> <message>
    */
    gettimeofday(&akt_time, NULL);
    msgtmp = malloc(strlen(msg) + 30);
    sprintf(msgtmp, "%lu.%.3lu 100 INFO 0 GM %lu %lu %s\n",
            akt_time.tv_sec, akt_time.tv_usec / 1000, sid, rid, msg);

    /*spool message for all info sessions*/
    if (sid == 0)
        queueInfoMessage(msgtmp);

    /*TODO: spool message for a single info session*/
    else
        queueInfoMessage(msgtmp);

    free(msgtmp);
    return SRCP_OK;
}
