/* Emacs style mode select   -*- C++ -*- */
/*-----------------------------------------------------------------------------*/
/**/
/* $Id:$*/
/**/
/* Copyright (C) 1993-1996 by id Software, Inc.*/
/**/
/* This source is available for distribution and/or modification*/
/* only under the terms of the DOOM Source Code License as*/
/* published by id Software. All rights reserved.*/
/**/
/* The source is distributed in the hope that it will be useful,*/
/* but WITHOUT ANY WARRANTY; without even the implied warranty of*/
/* FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License*/
/* for more details.*/
/**/
/* $Log:$*/
/**/
/* DESCRIPTION:*/
/*	DOOM graphics stuff for X11, UNIX.*/
/**/
/*-----------------------------------------------------------------------------*/

static const char
rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include <stdlib.h>
#include <unistd.h>

#ifdef LINUX
int XShmGetEventBase( Display* dpy ); /* problems with g++?*/
#endif

#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>

#include <signal.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"

#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/sysmacros.h>
#include <sys/page.h>
#include <sys/event.h>
#include <mouse.h>
#include <sys/vtkd.h>
#include <sys/errno.h>


short devhandle,savin[20],savary[66];
#define DISABLE_RENDER 1
#define  GMODE  1
#define  CMODE  0
#define  BOTTOM 0
#define  LEFT   0
#define  CENTER 1


/* SCO CGI functions, no header file for them :| */
short v_opnwk(short work_in[],short *dev_handle, short work_out[]);
short vq_error();
short v_pline(short dev_handle,short count, short xy[]);
short vqt_attributes(short dev_handle, short attrib[]);
short vst_alignment(short dev_handle,short hor_in,
					short vert_in,short *hor_out,short *vert_out);
short v_gtext(short dev_handle, short x, short y, char string[]);
short vrq_string(short dev_handle,short max_length,short echo_mode,
				 short echo_xy[],char string[]);
short vq_curaddress(short dev_handle,short *row, short*column);
short vs_curaddress(short dev_handle, short row, short column);
short v_curtext(short dev_handle,char string[]);
short v_enter_cur(short dev_handle);
short v_clswk(short dev_handle);
short vrd_curkeys(short dev_handle,short input_mode,short *direction, char *key);
short vb_pixels(short dev_handle,short origin[],short width,short height,
				short valid_width[],short valid_height[],char pixels[]);
short vs_color(short dev_handle,short ind_in,short rgb_in[],short rgb_out[]);


int event_queueFD;
char *ttyn;





void I_ShutdownGraphics(void)
{
	int consoleFD;
	printf("I_ShutdownGraphics\n");
	/* close event queue if still open */
	if (fcntl(event_queueFD, F_GETFL) != -1 || errno != EBADF) {
		printf("I_ShutdownGraphics - Closing event queue\n");
		ev_close();
	}
	
	/* restore tty mode */
	consoleFD = open(ttyn, O_RDWR | O_NDELAY, 0);
	ioctl(consoleFD, KDSKBMODE, K_XLATE);
	close(consoleFD);          
#ifndef DISABLEGRAPHICS
	v_clswk( devhandle );
#endif
}


void sig_handle(sig) {
	switch (sig) {
		case 1: printf("Caught SIGHUP: hangup\n"); break;
		case 2: printf("Caught SIGINT: interrupt (rubout)\n"); break;
		case 3: printf("Caught SIGQUIT: quit (ASCII FS)\n"); break;
		case 4: printf("Caught SIGILL: illegal instruction\n"); break;
		case 5: printf("Caught SIGTRAP: trace trap\n"); break;
		case 6: printf("Caught SIGIOT (or SIGABRT): IOT instruction\n"); break;
		case 7: printf("Caught SIGEMT: EMT instruction\n"); break;
		case 8: printf("Caught SIGFPE: floating point exception\n"); break;
		case 10: printf("Caught SIGBUS: bus error\n"); break;
		case 11: printf("Caught SIGSEGV: segmentation violation\n"); break;
		case 12: printf("Caught SIGSYS: bad argument to system call\n"); break;
		case 13: printf("Caught SIGPIPE: write on a pipe with no one to read it\n"); break;
		case 14: printf("Caught SIGALRM: alarm clock\n"); break;
		case 15: printf("Caught SIGTERM: software termination signal from kill\n"); break;
		case 16: printf("Caught SIGUSR1: user defined signal 1\n"); break;
		case 17: printf("Caught SIGUSR2: user defined signal 2\n"); break;
		case 18: printf("Caught SIGCLD: death of a child\n"); break;
		case 19: printf("Caught SIGPWR: power-fail restart\n"); break;
		default:
			printf("%d signal caught, application terminated.\n", sig);
	}
	I_ShutdownGraphics();
	exit( sig );
}


/**/
/* I_StartFrame*/
/**/
void I_StartFrame (void)
{

}


void I_GetEvent(void)
{
	EVENT *evp;
	event_t event;	
	int scancode = 0;
	int is_release = 0;
	int key;
	
	/* read from event stream */
	evp = ev_read();
	if (evp==NULL) 
		return;

#ifdef DISABLEGRAPHICS 	
	printf("I_GetEvent - evp is not null\n");
#endif	
	
	
	if (EV_TAG(*evp) & T_STRING)
		scancode = EV_BUF(*evp)[0];	
	ev_pop();
	
	/*return if we did not read a scancode*/
	if (scancode==0)
		return;
#ifdef DISABLEGRAPHICS 			
	printf("scancode: %x\n",scancode);
#endif	
	switch (scancode) {
		case 0xe0: /* need to read another character to know the key*/
					
				  	evp = ev_read();
			    	if (evp==NULL)
						return;
					scancode=0;
					if (EV_TAG(*evp) & T_STRING)
						scancode = EV_BUF(*evp)[0];	
					ev_pop();
					if (scancode==0)
						return;
#ifdef DISABLEGRAPHICS 							
					printf("2nd scancode: %x\n",scancode);
#endif					
					switch (scancode){
						case 0xc8: is_release=1;
						case 0x48: key = KEY_UPARROW; break;
						case 0xd0: is_release=1;
						case 0x50: key = KEY_DOWNARROW; break;
						case 0xcd: is_release=1;
						case 0x4d: key = KEY_RIGHTARROW; break;
						case 0xcb: is_release=1;
						case 0x4b: key = KEY_LEFTARROW; break;
						case 0xb8: is_release=1;
						case 0x38: key = KEY_RALT; break;
/*#ifdef DISABLEGRAPHICS */
						/* if graphics are disabled, DEL quits the app */
						case 0x53: I_ShutdownGraphics(); 
								   exit(0); 
/*#endif */
					}
					break;
		case 0x9d: is_release=1;
		case 0x1d: key = KEY_RCTRL; break; /* this is really left CTRL */
		/*TODO: my mac doesn't have a right ctrl lol */
		case 0xb9: is_release=1;
		case 0x39: key = ' '; break;
		case 0x9c: is_release=1;
		case 0x1c: key = KEY_ENTER; break;
		case 0x81: is_release=1;
		case 0x01: key = KEY_ESCAPE; break;
		case 0x95: is_release=1;
		case 0x15: key = 'y'; break;
		case 0xB1: is_release=1;
		case 0x31: key = 'n'; break;
	}
	
	if (is_release)
		event.type = ev_keyup;
	else event.type = ev_keydown;
	event.data1 = key;
#ifdef DISABLEGRAPHICS 						
	printf("Posting keycode %d with type %d\n",key,event.type);
#endif		
	D_PostEvent(&event);
	/* notes: 
	   some scancodes start with e0
	   key		press	release
	   UP:		e0 48	e0 c8
	   DOWN:	e0 50	e0 d0
	   RIGHT:	e0 4d	e0 cd
	   LEFT:	e0 4b	e0 cb
	   RALT:	e0 38	e0 b8	   
	   SPACE:	   39	   b9
	   LCTRL:	   1d	   9d
	   RSHIFT:	   2a	   aa
	   LSHIFT:	   36	   
	   1:		    2	   82
	   2:			3	   83
	   ...
	   7:			8	   88
	   0:			b
   	   -:		    c
   	   +:	shit it uses a modifier... see below key for =
   	   =:			d
	   ESC:			1
	   ENTER:	   1c
	   TAB:			f
   	   F1:		   3b
   	   F2:		   3c
   	   ...
   	   F10:		   44
   	   F:		   21
   	   M:		   32
   	   C:		   2e
   	   
   	   to implement the three QWERTY rows:
   	   Q:		   10
   	   A:		   1e
   	   Z:		   2c
	*/
	
}


/**/
/* I_StartTic*/
/**/

void I_StartTic (void)
{
	/*
	I_GetEvent();
	*/
}

/**/
/* I_UpdateNoBlit*/
/**/
void I_UpdateNoBlit (void)
{
	/*
	origin[0] = 0;
	origin[1] = 0;
	width[0] = 0;
	width[1] = 19;
	height[0] = 0;
	height[1] = 19;
	i = 0;
	for (r=0;r<20;r++)
		for (c=0;c<20;c++) {
			i++;
			pixels[i]=rand();
			if (r==c)
				pixels[i] = 0;
		}
	vb_pixels(devhandle,origin,20,20,width,height,pixels);
	*/
	/* STUPID DIRTYBOX is top - right - bottom - left, so (ymax,xmax,ymin,xmin) */
	/*printf("Updated area dirtybox: (height=%d,x=%d,y=%d,width=%d)\n",dirtybox[0],dirtybox[1],dirtybox[2],dirtybox[3]);*/
}

/**/
/* I_FinishUpdate*/
/**/
void I_FinishUpdate (void)
{
#ifndef DISABLEGRAPHICS
	short origin[2], valid_width[2], valid_height[2];
	int i, outline;
	char pixels[64000];
	
	origin[0] = 0;
	origin[1] = 0;
	valid_width[0] = 0;
	valid_width[1] = 319;
	valid_height[0] = 0;
	valid_height[1] = 199;

	for (i=0; i < 200; i++) {
		outline = 199-i;
		memcpy(pixels+320*i,screens[0]+320*outline,320);
	}

	vb_pixels(devhandle,origin,320,200,valid_width,valid_height,pixels);
#endif
	I_GetEvent();
}


/**/
/* I_ReadScreen*/
/**/
void I_ReadScreen (byte* scr)
{


}



/**/
/* I_SetPalette*/
/**/

void I_SetPalette (byte* palette)
{
#ifndef DISABLEGRAPHICS
	register short c;
	register int i;
	short intin[3],intout[3];
	/*short	colors[256][3];*/
	
    /* set the colormap entries, color by color (sigh) */
	for (i=0 ; i<256 ; i++)	{
	    c=gammatable[usegamma][*palette++];
	    intin[0]=(c>>2)*15;
	    
	    c=gammatable[usegamma][*palette++];
	    intin[1]=(c>>2)*15;

	    c=gammatable[usegamma][*palette++];
	    intin[2]=(c>>2)*15;
	    
	    vs_color(devhandle, i, intin, intout);
    }    
    
    /* store the colors to the current colormap in one go.
       UNUSED: for no apparent reason it BREAKS AFTER 150
       and has bugs earlier before that too */
	/*vsc_table(devhandle,0,127,colors);  */
#endif
}



void I_InitGraphics(void)
{
	int consoleFD;
	dmask_t omask, dmask = D_STRING;
	short error;
	if (getenv("CGIDISP") == NULL) {
    	printf("No CGIDISP env variable found\n");
    	printf("Make sure you correctly set CGIPATH and CGIDISP\n");
    	printf("CGIPATH=/usr/lib/cgi\n");
    	printf("CGIDISP=/usr/lib/vga256\n");
    	printf("export CGIPATH CGIDISP\n");
    	exit(1);
    }
    

	/* set tty to RAW mode to get scancodes */
	ttyn = (char *) ttyname(0);
	consoleFD = open(ttyn, O_RDWR | O_NDELAY, 0);
	ioctl(consoleFD, KDSKBMODE, K_RAW);
	close(consoleFD);    
    
    
    /* Initialize the event queue */
	if (event_queueFD = ev_init() < 0) {
		printf("initialization of event queue failed (result=%d)\n",event_queueFD);
		printf("(did you run mkdev mouse?)\n");
		I_ShutdownGraphics();
		exit(1);
	}
    omask=dmask;
    if ((event_queueFD = ev_open(&dmask)) < 0) {
	    printf("open keyboard failed (result=%d)\n",event_queueFD);                
	    printf("(try rebooting)\n");
        I_ShutdownGraphics();
        exit(1);
    }    
    
#ifndef DISABLEGRAPHICS
	/* initialize i/o for newframe prompt*/
	signal( SIGHUP, sig_handle );
	signal( SIGINT, sig_handle );
	signal( SIGQUIT, sig_handle );
	signal( SIGILL, sig_handle );
	signal( SIGTRAP, sig_handle );
	signal( SIGIOT, sig_handle );
	signal( SIGEMT, sig_handle );
	signal( SIGFPE, sig_handle );
	signal( SIGBUS, sig_handle );
	signal( SIGSEGV, sig_handle );

	/* Open graphics */
	savin[0] = 2;                /* raster mode */
	savin[1] = 1;
	savin[2] = 1;
	savin[3] = 3;
	savin[4] = 1;
	savin[5] = 1;
	savin[6] = 1;
	savin[7] = 0;
	savin[8] = 0;
	savin[9] = 1;
	savin[10] = 1; 
	savin[11] = 'C';             /* OPEN CRT DEVICE */
	savin[12] = 'G';
	savin[13] = 'I';
	savin[14] = 'D';
	savin[15] = 'I';
	savin[16] = 'S';
	savin[17] = 'P';
	savin[18] = ' ';


	/* open the workstation and save output in savary array */
	error = v_opnwk(savin, &devhandle, savary);
	if (error < 0) {
		printf("Cgitest Error %d opening device display\n",vq_error());
		printf("Check the environment variables: CGIPATH and CGIDISP.\n" );
		exit (-1);
	}
	
#endif
    
   
#ifdef DISABLEGRAPHICS 	
	printf("I_InitGraphics finished\n");
#endif
}



void InitExpand (void)
{
}


void InitExpand2 (void)
{
}

