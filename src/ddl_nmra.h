/* +----------------------------------------------------------------------+ */
/* | DDL - Digital Direct for Linux                                       | */
/* +----------------------------------------------------------------------+ */
/* | Copyright (c) 2002 - 2003 Vogt IT                                    | */
/* +----------------------------------------------------------------------+ */
/* | This source file is subject of the GNU general public license 2,     | */
/* | that is bundled with this package in the file COPYING, and is        | */
/* | available at through the world-wide-web at                           | */
/* | http://www.gnu.org/licenses/gpl.txt                                  | */
/* | If you did not receive a copy of the PHP license and are unable to   | */
/* | obtain it through the world-wide-web, please send a note to          | */
/* | gpl-license@vogt-it.com so we can mail you a copy immediately.       | */
/* +----------------------------------------------------------------------+ */
/* | Authors:   Torsten Vogt vogt@vogt-it.com                             | */
/* |                                                                      | */
/* +----------------------------------------------------------------------+ */

/***************************************************************/
/* erddcd - Electric Railroad Direct Digital Command Daemon    */
/*    generates without any other hardware digital commands    */
/*    to control electric model railroads                      */
/*                                                             */
/* file: maerklin.h                                            */
/* job : exports the functions from nmra.c                     */
/*                                                             */
/* Torsten Vogt, june 1999                                     */
/*                                                             */
/* last changes: Torsten Vogt, march 2000                      */
/*                                                             */
/***************************************************************/

#ifndef __NMRA_H__
#define __NMRA_H__ 

int translateBitstream2Packetstream(int dccversion, char *Bitstream, char *Packetstream,
                                    int force_translation);

/* signal generating functions for nmra dcc */

/* NMRA standard decoder      */
int comp_nmra_baseline(int busnumber, int address, int direction, int speed);

/* 4-func,7-bit-addr,28 sp-st.*/
int comp_nmra_f4b7s28(int busnumber, int address, int direction, int speed, int func,
                      int f1, int f2, int f3, int f4);

/* 4-func,7-bit-addr,128 sp-st*/
int comp_nmra_f4b7s128(int busnumber, int address, int direction, int speed, int func,
                       int f1, int f2, int f3, int f4);

/* 4-func,14-bit-addr,28 sp-st*/
int comp_nmra_f4b14s28(int busnumber, int address, int direction, int speed, int func,
                       int f1, int f2, int f3, int f4);

/* 4-func,14-bit-addr,128 sp-st*/
int comp_nmra_f4b14s128(int busnumber, int address, int direction, int speed, int func,
                       int f1, int f2, int f3, int f4); 

/* NMRA accessory decoder     */
int comp_nmra_accessory(int busnumber, int nr, int output, int activate);

/* service mode functions */
void protocol_nmra_sm_write_cvbyte(int busnumber,int sckt, int cv, int value);
void protocol_nmra_sm_verify_cvbyte(int busnumber,int sckt, int cv, int value);
void protocol_nmra_sm_write_cvbit(int busnumber, int sckt, int cv, int bit, int value);
void protocol_nmra_sm_write_phregister(int busnumber, int sckt, int reg, int value);
void protocol_nmra_sm_verify_phregister(int busnumber, int sckt,int reg,int value);

#endif
