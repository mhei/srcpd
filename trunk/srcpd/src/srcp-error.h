/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */

#ifndef _SRCP_ERROR_H
#define _SRCP_ERROR_H

/* Handshake */
#define SRCP_OK_GO 200
#define SRCP_OK_PROTOCOL 201
#define SRCP_OK_CONNMODE 202

#define SRCP_HS_UNKNOWN 401

/* COMMAND MODE */
#define SRCP_INFO 100
#define SRCP_OK 200

#define SRCP_WRONGVALUE 412
#define SRCP_NODATA 416

#define SRCP_UNSUPPORTEDDEVICEGROUP 422
#define SRCP_UNSUPPORTED 423

int srcp_fmt_msg(int errno, char *msg);

#endif
