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
/* file: maerklin.c                                            */
/* job : implements routines to compute data for the           */
/*       various Maerklin protocols and send this data to      */
/*       the serial device.                                    */
/*                                                             */
/* Torsten Vogt, january 1999                                  */
/*                                                             */
/* last changes: june 2000                                     */
/*               january 2001                                  */
/*                                                             */
/*
   thanks to Dieter Schaefer for testing and correcting the
   handling of the solenoids decoders (thr_protocol_maerklin_ms())
*/
/***************************************************************/

/**********************************************************************

 implemented protocols:
 
 M1: maerklin protocol type 1 (old)
 M2: maerklin protocol type 2 (new)
 M3: maerklin protocol type 2 (new) with 28 speed steps (Wikinger decoder)
 M4: maerklin protocol type 2 (new) with 256 addresses (Uhlenbrock)
 M5: maerklin protocol type 2 (new) with 27 speed steps (newer marklin decoders)
 MS: maerklin protocol for solenoids (type 1 and type 2)
 MF: maerklin_protocol for function decoders (old)
 
**********************************************************************/  

#include "ddl.h"
#include "ddl_maerklin.h"
#include "srcp-gl.h"

#define TRUE 1
#define FALSE 0  

int comp_maerklin_1(int busnumber, int address, int direction, int speed, int func) {
  
   char trits[9];
   char packet[18];
   int i,j;
   struct _GLSTATE loco;
   DBG(busnumber, DBG_DEBUG,"command for M�rklin protocol type 1: %d received", address);
 
   /* no special error handling, it's job of the clients */
   if (address<0 || address>80 || func<0 || func>1 || speed<0 || speed>15 ||
       direction<0 || direction>1)
      return 1;
   getGL(busnumber, address, &loco);
   if (direction!=loco.direction) { 
      speed=1;
   }

   /* compute address trits */
   for (i=0; i<4; i++) trits[i]=MotorolaCodes[address].Code[i];

   /* compute func trit     */
   if (func) trits[4]='H'; else trits[4]='L' ;     
   /* compute speed trits   */
   for (i=5; i<9; i++) {
      j=speed%2;
      speed=speed/2;
      switch (j) {
         case 0: trits[i]='L'; break;
         case 1: trits[i]='H'; break;
      }
   } 

   for (i=0;i<9;i++) {
      switch (trits[i]) {
         case 'L': packet[2*i]=LO; packet[2*i+1]=LO; break;
         case 'H': packet[2*i]=HI; packet[2*i+1]=HI; break; 
         case 'O': packet[2*i]=HI; packet[2*i+1]=LO; break;
      }	 					     
   }

   update_MaerklinPacketPool(busnumber, address, packet, packet, packet, packet, packet);
   queue_add(address, packet,QM1LOCOPKT,18);  

   return 0;
}

int comp_maerklin_2(int busnumber, int address, int direction, int speed, int func,
                     int f1, int f2, int f3, int f4) {

   char trits[9];
   char packet[18];
   char f_packets[4][18];
   int  fx = 0, fx_changed = 0;
   char *fx_packet;
   
   char mask[5];
   int i,j;
   int adr=0;
   int mspeed;
  
#ifdef _DEBUG_
   printf("\ncommand for M�rklin protocol type 2 (M2) received\n");
#endif
 
   adr=address;  
   if (direction==0) direction=-1; else direction=1;
   speed=direction*speed;
      
   /* no special error handling, it's job of the clients */
   if (address<0 || address>80 || func<0 || func>1 || speed<-15 || speed>15 ||
       f1<0 || f1>1 || f2<0 || f2>1 || f3<0 || f3>1 || f4<0 || f4>1)
      return 1;

   /* compute address trits */
   for (i=0; i<4; i++) trits[i]=MotorolaCodes[address].Code[i];    
   
   /* compute func trit     */
   if (func) trits[4]='H'; else trits[4]='L' ;     

   /* so far the same procedure as by marklin type 1, but now ... */			   
   /* compute speed trits   */
   if (speed<-7)                                strcpy(mask,"HLHL");
   if (speed<=0 && speed>=-7 && direction== -1) strcpy(mask,"HLHH");
   if (speed>=0 && speed<=7  && direction== 1 ) strcpy(mask,"LHLH");
   if (speed>7)                                 strcpy(mask,"LHLL");      
   speed=abs(speed);
   mspeed=speed;

   if (speed==1) speed=0;                 /* speed  1 is obsolete */
   for (i=5; i<9; i++) {
      j=speed%2;
      speed=speed/2;
      switch (j) {
         case 0: trits[i]='L'; break;
         case 1: trits[i]='H'; break;
      }
      if (trits[i]=='H' && mask[i-5]=='L') trits[i]='O';
      if (trits[i]=='L' && mask[i-5]=='H') trits[i]='U'; /* Ooops, whats */
   }                                                     /* this? :-)    */

#ifdef _DEBUG_
   printf("   computed trits: ");
   for (i=0; i<9; i++) printf("%2c    ", trits[i]);    
   printf("\n");
#endif
   
   for (i=0;i<9;i++) {
      switch (trits[i]) {
         case 'L': packet[2*i]=LO; packet[2*i+1]=LO; break;
         case 'H': packet[2*i]=HI; packet[2*i+1]=HI; break; 
         case 'O': packet[2*i]=HI; packet[2*i+1]=LO; break;
         case 'U': packet[2*i]=LO; packet[2*i+1]=HI; break; 
      }	 					     
   }
   for (j=0; j<4; j++) {
      for (i=0;i<18;i++) {
         f_packets[j][i]=packet[i];
      }
   }
   f_packets[0][11]=HI; f_packets[0][13]=HI; f_packets[0][15]=LO;
   if (f1) f_packets[0][17]=HI; else f_packets[0][17]=LO;
   f_packets[1][11]=LO; f_packets[1][13]=LO; f_packets[1][15]=HI;
   if (f2) f_packets[1][17]=HI; else f_packets[1][17]=LO;
   f_packets[2][11]=LO; f_packets[2][13]=HI; f_packets[2][15]=HI;
   if (f3) f_packets[2][17]=HI; else f_packets[2][17]=LO;
   f_packets[3][11]=HI; f_packets[3][13]=HI; f_packets[3][15]=HI;
   if (f4) f_packets[3][17]=HI; else f_packets[3][17]=LO;

   /* thanks to Dieter Schaefer for the following code */
   for(j=0;j<4;j++) {
      if(((j==0)&&(mspeed==3)&&(!f1))
      || ((j==1)&&(mspeed==4)&&(!f2))
      || ((j==2)&&(mspeed==6)&&(!f3))
      || ((j==3)&&(mspeed==7)&&(!f4))) {
          f_packets[j][11]=HI; f_packets[j][13]=LO;
          f_packets[j][15]=HI;}
      if(((j==0)&&(mspeed==11)&&(f1))
      || ((j==1)&&(mspeed==12)&&(f2))
      || ((j==2)&&(mspeed==14)&&(f3))
      || ((j==3)&&(mspeed==15)&&(f4))) {
          f_packets[j][11]=LO; f_packets[j][13]=HI;
          f_packets[j][15]=LO;}
   }

#ifdef _DEBUG_
   printf("   data packages : ");
   for (i=0; i<18; i++) printf("%2d ", packet[i]);
   printf("\n                   ");
   for (i=0; i<18; i++) printf("%2d ", f_packets[0][i]);
   printf("\n                   ");
   for (i=0; i<18; i++) printf("%2d ", f_packets[1][i]);
   printf("\n                   ");
   for (i=0; i<18; i++) printf("%2d ", f_packets[2][i]);
   printf("\n                   ");
   for (i=0; i<18; i++) printf("%2d ", f_packets[3][i]);
   printf("\n");   
#endif

#ifdef _DEBUG_   
   printf("   sending packages ... \n");   
#endif

   /* lets have a look, what has changed ... */
   for (i=0; i<4; i++) {
      fx_packet=get_maerklin_packet(busnumber, adr,i);
      if (fx_packet[17]!=f_packets[i][17]) {
         fx_changed=1;
         fx=i;
         break;
      }
   }
  
   update_MaerklinPacketPool(busnumber, adr, packet, f_packets[0], f_packets[1], 
                                          f_packets[2], f_packets[3]); 
   if (!fx_changed) queue_add(adr,packet,QM2LOCOPKT,18);  
   else queue_add(adr,f_packets[fx],QM2FXPKT,18); 

   return 0;
}

int comp_maerklin_3(int busnumber, int address, int direction, int speed, int func,
                     int f1, int f2, int f3, int f4) {

   char trits[9];
   char packet[18];
   char f_packets[4][18];
   int  fx = 0, fx_changed = 0;
   char *fx_packet;
   
   char mask[5];
   int i,j;
   int adr=0;
   int speed_halfstep =0;
   int mspeed;
  
#ifdef _DEBUG_
   printf("\ncommand for M�rklin protocol type 3 (28 speed steps) (M3) received\n");
#endif
   
   adr=address;
   if (direction==0) direction=-1; else direction=1;
   speed=direction*speed;   
      
   /* no special error handling, it's job of the clients */
   if (address<0 || address>255 || func<0 || func>1 || speed<-28 || speed>28 ||
       f1<0 || f1>1 || f2<0 || f2>1 || f3<0 || f3>1 || f4<0 || f4>1)
      return 1;

   /* compute address trits */
   for (i=0; i<4; i++) trits[i]=MotorolaCodes[address].Code[i];    

   /* so far the same procedure as by marklin type 1, but now ... */ 

   speed_halfstep = !(abs(speed) % 2);
   if (speed>0)
      speed = ((speed+1)/2)+1;
   else
      speed = ((speed-1)/2)-1;      

   /* compute func trit and speed halfstep */
   if (!speed_halfstep)
      if (func) trits[4]='H'; else trits[4]='L' ;  
   else
      if (func) trits[4]='O'; else trits[4]='U' ;   

   /* compute speed trits   */
   if (speed<-7)                                strcpy(mask,"HLHL");
   if (speed<=0 && speed>=-7 && direction== -1) strcpy(mask,"HLHH");
   if (speed>=0 && speed<=7  && direction== 1 ) strcpy(mask,"LHLH");
   if (speed>7)                                 strcpy(mask,"LHLL");      
   speed=abs(speed);
   mspeed=speed;

   if (speed==1) speed=0;                 /* speed  1 is obsolete */
   for (i=5; i<9; i++) {
      j=speed%2;
      speed=speed/2;
      switch (j) {
         case 0: trits[i]='L'; break;
         case 1: trits[i]='H'; break;
      }
      if (trits[i]=='H' && mask[i-5]=='L') trits[i]='O';
      if (trits[i]=='L' && mask[i-5]=='H') trits[i]='U'; /* Ooops, whats */
   }                                                     /* this? :-)    */

#ifdef _DEBUG_
   printf("   computed trits: ");
   for (i=0; i<9; i++) printf("%2c    ", trits[i]);    
   printf("\n");
#endif
   
   for (i=0;i<9;i++) {
      switch (trits[i]) {
         case 'L': packet[2*i]=LO; packet[2*i+1]=LO; break;
         case 'H': packet[2*i]=HI; packet[2*i+1]=HI; break; 
         case 'O': packet[2*i]=HI; packet[2*i+1]=LO; break;
         case 'U': packet[2*i]=LO; packet[2*i+1]=HI; break; 
      }                                              
   }
   for (j=0; j<4; j++) {
      for (i=0;i<18;i++) {
         f_packets[j][i]=packet[i];
      }
   }
   f_packets[0][11]=HI; f_packets[0][13]=HI; f_packets[0][15]=LO;
   if (f1) f_packets[0][17]=HI; else f_packets[0][17]=LO;
   f_packets[1][11]=LO; f_packets[1][13]=LO; f_packets[1][15]=HI;
   if (f2) f_packets[1][17]=HI; else f_packets[1][17]=LO;
   f_packets[2][11]=LO; f_packets[2][13]=HI; f_packets[2][15]=HI;
   if (f3) f_packets[2][17]=HI; else f_packets[2][17]=LO;
   f_packets[3][11]=HI; f_packets[3][13]=HI; f_packets[3][15]=HI;
   if (f4) f_packets[3][17]=HI; else f_packets[3][17]=LO;

   /* thanks to Dieter Schaefer for the following code */
   for(j=0;j<4;j++) {
      if(((j==0)&&(mspeed==3)&&(!f1))
      || ((j==1)&&(mspeed==4)&&(!f2))
      || ((j==2)&&(mspeed==6)&&(!f3))
      || ((j==3)&&(mspeed==7)&&(!f4))) {
          f_packets[j][11]=HI; f_packets[j][13]=LO;
          f_packets[j][15]=HI;}
      if(((j==0)&&(mspeed==11)&&(f1))
      || ((j==1)&&(mspeed==12)&&(f2))
      || ((j==2)&&(mspeed==14)&&(f3))
      || ((j==3)&&(mspeed==15)&&(f4))) {
          f_packets[j][11]=LO; f_packets[j][13]=HI;
          f_packets[j][15]=LO;}
   }

#ifdef _DEBUG_
   printf("   data packages : ");
   for (i=0; i<18; i++) printf("%2d ", packet[i]);
   printf("\n                   ");
   for (i=0; i<18; i++) printf("%2d ", f_packets[0][i]);
   printf("\n                   ");
   for (i=0; i<18; i++) printf("%2d ", f_packets[1][i]);
   printf("\n                   ");
   for (i=0; i<18; i++) printf("%2d ", f_packets[2][i]);
   printf("\n                   ");
   for (i=0; i<18; i++) printf("%2d ", f_packets[3][i]);
   printf("\n");   
#endif

#ifdef _DEBUG_   
   printf("   sending packages ... \n");   
#endif

   /* lets have a look, what has changed ... */
   for (i=0; i<4; i++) {
      fx_packet=get_maerklin_packet(busnumber, adr,i);
      if (fx_packet[17]!=f_packets[i][17]) {
         fx_changed=1;
         fx=i;
         break;
      }
   }
  
   update_MaerklinPacketPool(busnumber, adr, packet, f_packets[0], f_packets[1], 
                                          f_packets[2], f_packets[3]); 
   if (!fx_changed) queue_add(adr, packet,QM2LOCOPKT,18);  
   else queue_add(adr, f_packets[fx],QM2FXPKT,18); 

   return 0;
}

int comp_maerklin_4(int busnumber, int address, int direction, int speed, int func,
                         int f1, int f2, int f3, int f4) {

   char trits[9];
   char packet[18];
   char f_packets[4][18];
   int  fx = 0, fx_changed = 0;
   char *fx_packet;

   char mask[5];
   int i,j;
   int adr=0;
   int mspeed;

#ifdef _DEBUG_
   printf("\ncommand for M�rklin protocol type 4 (M4, Uhlenbrock Intellibox) received\n");
#endif

   adr=address;
   if (direction==0) direction=-1; else direction=1;
   speed=direction*speed;     

   /* no special error handling, it's job of the clients */
   if (address<0 || address>255 || func<0 || func>1 || speed<-15 || speed>15 ||
       f1<0 || f1>1 || f2<0 || f2>1 || f3<0 || f3>1 || f4<0 || f4>1)
      return 1;

   /* compute address trits */
   for (i=0; i<4; i++) trits[i]=MotorolaCodes[address].Code[i];

   /* compute func trit     */
   if (func) trits[4]='H'; else trits[4]='L' ;

   /* so far the same procedure as by marklin type 1, but now ... */

   /* compute speed trits   */
   if (speed<-7)                                strcpy(mask,"HLHL");
   if (speed<=0 && speed>=-7 && direction== -1) strcpy(mask,"HLHH");
   if (speed>=0 && speed<=7  && direction== 1 ) strcpy(mask,"LHLH");
   if (speed>7)                                 strcpy(mask,"LHLL"); 
   speed=abs(speed);
   mspeed=speed;

   //if (speed==1) speed=0;                 /* speed  1 is obsolete */
   for (i=5; i<9; i++) {
      j=speed%2;
      speed=speed/2;
      switch (j) {
         case 0: trits[i]='L'; break;
         case 1: trits[i]='H'; break;
      }
      if (trits[i]=='H' && mask[i-5]=='L') trits[i]='O';
      if (trits[i]=='L' && mask[i-5]=='H') trits[i]='U'; /* Ooops, whats */
   }                                                     /* this? :-)    */

#ifdef _DEBUG_
   printf("   computed trits: ");
   for (i=0; i<9; i++) printf("%2c    ", trits[i]);
   printf("\n");
#endif

   for (i=0;i<9;i++) {
      switch (trits[i]) {   
         case 'L': packet[2*i]=LO; packet[2*i+1]=LO; break;
         case 'H': packet[2*i]=HI; packet[2*i+1]=HI; break;
         case 'O': packet[2*i]=HI; packet[2*i+1]=LO; break;
         case 'U': packet[2*i]=LO; packet[2*i+1]=HI; break;
      }
   }
   for (j=0; j<4; j++) {
      for (i=0;i<18;i++) {
         f_packets[j][i]=packet[i];
      }
   }
   f_packets[0][11]=HI; f_packets[0][13]=HI; f_packets[0][15]=LO;
   if (f1) f_packets[0][17]=HI; else f_packets[0][17]=LO;
   f_packets[1][11]=LO; f_packets[1][13]=LO; f_packets[1][15]=HI;
   if (f2) f_packets[1][17]=HI; else f_packets[1][17]=LO;
   f_packets[2][11]=LO; f_packets[2][13]=HI; f_packets[2][15]=HI;
   if (f3) f_packets[2][17]=HI; else f_packets[2][17]=LO;
   f_packets[3][11]=HI; f_packets[3][13]=HI; f_packets[3][15]=HI;
   if (f4) f_packets[3][17]=HI; else f_packets[3][17]=LO;

   /* thanks to Dieter Schaefer for the following code */
   for(j=0;j<4;j++) {
      if(((j==0)&&(mspeed==3)&&(!f1))
      || ((j==1)&&(mspeed==4)&&(!f2))
      || ((j==2)&&(mspeed==6)&&(!f3))
      || ((j==3)&&(mspeed==7)&&(!f4))) {
          f_packets[j][11]=HI; f_packets[j][13]=LO;
          f_packets[j][15]=HI;}
      if(((j==0)&&(mspeed==11)&&(f1))
      || ((j==1)&&(mspeed==12)&&(f2))
      || ((j==2)&&(mspeed==14)&&(f3))
      || ((j==3)&&(mspeed==15)&&(f4))) {
          f_packets[j][11]=LO; f_packets[j][13]=HI;
          f_packets[j][15]=LO;}
   }

#ifdef _DEBUG_
   printf("   data packages : ");
   for (i=0; i<18; i++) printf("%2d ", packet[i]);
   printf("\n                   ");
   for (i=0; i<18; i++) printf("%2d ", f_packets[0][i]);
   printf("\n                   ");
   for (i=0; i<18; i++) printf("%2d ", f_packets[1][i]);
   printf("\n                   ");
   for (i=0; i<18; i++) printf("%2d ", f_packets[2][i]);
   printf("\n                   ");    
   for (i=0; i<18; i++) printf("%2d ", f_packets[3][i]);
   printf("\n");
#endif

#ifdef _DEBUG_
   printf("   sending packages ... \n");
#endif

   /* lets have a look, what has changed ... */
   for (i=0; i<4; i++) {
      fx_packet=get_maerklin_packet(busnumber, adr,i);
      if (fx_packet[17]!=f_packets[i][17]) {
         fx_changed=1;
         fx=i;
         break;
      }
   }

   update_MaerklinPacketPool(busnumber, adr, packet, f_packets[0], f_packets[1],
                                          f_packets[2], f_packets[3]);
   if (!fx_changed) queue_add(adr, packet,QM2LOCOPKT,18);
   else queue_add(adr, f_packets[fx],QM2FXPKT,18);

   return 0;
}

int comp_maerklin_5(int busnumber, int address, int direction, int speed, int func, 
                    int f1, int f2, int f3, int f4) {

   int sFS1, sFS2;
   int       rtc;
   int       speed_old;
   int       two_commands = FALSE;
   int       acceleration = FALSE;
   struct _GLSTATE loco;
#ifdef _DEBUG_
   printf("\ncommand for M�rklin protocol type 5 (M5) received\n");
   printf("ATTENTION: This means, that one or two M2 commands will follow.\n");
#endif
 
   /* no special error handling, it's job of the clients */
   if (speed<0 || speed>28) return 1;

   /* xFS    rFS   sFS1 -> (500ms) -> sFS2

        0      0      0                  %
              
        2      2      0                  2
        3    2.5      3                  2
        4      3      2                  3
        5    3.5      4                  3
        6      4      3                  4
        7    4.5      5                  4
        8      5      4                  5
        9    5.5      6                  5
       10      6      5                  6
       11    6.5      7                  6
       12      7      6                  7
       13    7.5      8                  7
       14      8      7                  8
       15    8.5      9                  8
       16      9      8                  9
       17    9.5     10                  9 
       18     10      9                 10
       19   10.5     11                 10
       20     11     10                 11
       21   11.5     12                 11
       22     12     11                 12
       23   12.5     13                 12
       24     13     12                 13
       25   13.5     14                 13
       26     14     13                 14
       27   14.5     15                 14
       28     15     14                 15
    */

    getGL(busnumber, address, &loco);
    speed_old=loco.speed;

    acceleration=(speed_old<speed);

    if ( (acceleration  && (speed%2)) ||
         (!acceleration && !(speed%2)) ) two_commands=TRUE;

    if (speed>0) {
       if (two_commands) {         // two commands necessary
          if (speed%2) {
             sFS2=speed/2+1;
             sFS1=sFS2+1;
          }
          else {
             sFS2=speed/2+1;
             sFS1=sFS2-1;
             if (sFS1==1) sFS1=0;
          }
       }
       else {                      // one command necessary 
          sFS1=speed/2+1;
          sFS2=0;
       }
    }
    else {                         // one command necessary
       sFS1=0;
       sFS2=0;
    }

    rtc=comp_maerklin_2(busnumber, address, direction, sFS1, func, f1, f2, f3, f4);
    if ((sFS2>0) && (rtc==0)) {
       usleep(50000);
       rtc=comp_maerklin_2(busnumber, address, direction, sFS2, func, f1, f2, f3, f4);
    }
    return rtc;
}  

int comp_maerklin_ms(int busnumber, int address, int port, int action) {
  
   char trits[9];
   char packet[9];
   char *p_packet;
   int i,j;
   int id, subid;

#ifdef _DEBUG_
   printf("\ncommand for solenoid (Maerklin) (MS) received\n");
#endif
 
   /* no special error handling, it's job of the clients */
   if (address<1 || address>324 || action<0 || action>1 || 
       port<0 ||  port>1) 
      return 1;
#if 0 // GA Packet Cache
   getMaerklinGaPacket(address,port,action,&p_packet);
   if (p_packet==NULL) {
#endif
      p_packet=packet;
      id=((address-1) >> 2); 
      subid=(((address-1) & 3) << 1)+port;
      if (action) trits[8] = 'H'; else trits[8] = 'L';

#ifdef _DEBUG_
   printf("   addr:port     : %d:%d\n", id,subid);
#endif   
   
      /* compute address trits */
      for (i=0; i<4; i++) {
         j=id%3;
         id=id/3;
         switch (j) {
            case 0: trits[i]='L'; break;
            case 1: trits[i]='H'; break;
            case 2: trits[i]='O'; break;
         }
      }   
      /* compute func trit (dummy) */
      trits[4]='L' ;     
      /* compute port trits   */			   
      for (i=5; i<8; i++) {
         j=subid%2;
         subid=subid/2;
         switch (j) {
            case 0: trits[i]='L'; break;
            case 1: trits[i]='H'; break;
         }
      }

#ifdef _DEBUG_
   printf("   computed trits: ");
   for (i=0; i<9; i++) printf("%2c    ", trits[i]);    
   printf("\n");
#endif
   
      for (i=0;i<9;i++) {
         switch (trits[i]) {
            case 'L': packet[i]=LO_38K; break;
            case 'H': packet[i]=HI_38K; break; 
            case 'O': packet[i]=OP_38K; break;
         }	 					     
      }
#if 0 // GA Packet Cache
   }
#endif
#ifdef _DEBUG_
   printf("   data package  : ");
   for (i=0; i<9; i++) printf("%2d    ", *(p_packet+i));
   printf("\n");
#endif

   queue_add(address, p_packet,QM1SOLEPKT,9);
#if 0 // GA Packet Cache
   updateMaerklinGaPacketPool(address,port,action,p_packet);
#endif
   return 0;
}

int comp_maerklin_mf(int busnumber, int address, int f1, int f2, int f3, int f4) {
  
   char trits[9];
   char packet[9];
   int i;
  
#ifdef _DEBUG_
   printf("\ncommand for func decoder (M�rklin) (MF) received\n");
#endif
 
   /* no special error handling, it's job of the clients */
   if (address<0 || address>80 || f1<0 || f1>1 ||
       f2<0 || f2>1 || f3<0 || f3>1 || f4<0 || f4>1)
      return 1;

   /* compute address trits */
   for (i=0; i<4; i++) trits[i]=MotorolaCodes[address].Code[i];    

   /* compute func trit (dummy) */
   trits[4]='H';   
   /* compute function trits   */
   if (f1) trits[5]='H'; else trits[5]='L';
   if (f2) trits[6]='H'; else trits[6]='L';
   if (f3) trits[7]='H'; else trits[7]='L';
   if (f4) trits[8]='H'; else trits[8]='L';
   
#ifdef _DEBUG_
   printf("   computed trits: ");
   for (i=0; i<9; i++) printf("%2c    ", trits[i]);    
   printf("\n");
#endif
   
   for (i=0;i<9;i++) {
      switch (trits[i]) {
         case 'L': packet[i]=LO_38K; break;
         case 'H': packet[i]=HI_38K; break; 
         case 'O': packet[i]=OP_38K; break;
      }	 					     
   }

#ifdef _DEBUG_
   printf("   data package  : ");
   for (i=0; i<9; i++) printf("%2d    ", packet[i]);
   printf("\n");
#endif

   queue_add(address, packet,QM1FUNCPKT,9); 

   return 0;
}
