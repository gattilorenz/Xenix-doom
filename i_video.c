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


void initkeyhandler();
int read_scancode();
char oldkeystate[128];



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
		case SIGHUP: printf("Caught SIGHUP: hangup\n"); break;
		case SIGINT: printf("Caught SIGINT: interrupt (rubout)\n"); break;
		case SIGQUIT: printf("Caught SIGQUIT: quit (ASCII FS)\n"); break;
		case SIGILL: printf("Caught SIGILL: illegal instruction\n"); break;
		case SIGTRAP: printf("Caught SIGTRAP: trace trap\n"); break;
		case SIGIOT: printf("Caught SIGIOT (or SIGABRT): IOT instruction\n"); break;
		case SIGEMT: printf("Caught SIGEMT: EMT instruction\n"); break;
		case SIGFPE: printf("Caught SIGFPE: floating point exception\n"); break;
		case SIGBUS: printf("Caught SIGBUS: bus error\n"); break;
		case SIGSEGV: printf("Caught SIGSEGV: segmentation violation\n"); break;
		case SIGSYS: printf("Caught SIGSYS: bad argument to system call\n"); break;
		case SIGPIPE: printf("Caught SIGPIPE: write on a pipe with no one to read it\n"); break;
		case SIGALRM: printf("Caught SIGALRM: alarm clock\n"); break;
		case SIGTERM: printf("Caught SIGTERM: software termination signal from kill\n"); break;
		case SIGUSR1: printf("Caught SIGUSR1: user defined signal 1\n"); break;
		case SIGUSR2: printf("Caught SIGUSR2: user defined signal 2\n"); break;
		case SIGCLD: printf("Caught SIGCLD: death of a child\n"); break;
		case SIGPWR: printf("Caught SIGPWR: power-fail restart\n"); break;
		default:
			printf("signal %d caught, application terminated.\n", sig);
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



void initkeyhandler()
{
	int i;
	for (i=0;i<128;i++) 
		oldkeystate[i]=0;
}

int read_scancode() {
	int scancode = 0;
	EVENT *evp;	
	/* read scancode from event queue */
  	evp = ev_read();
   	if (evp==NULL)
		return 0;
	/* check that it's a Keyboard event */
	if (EV_TAG(*evp) & T_STRING)
		scancode = EV_BUF(*evp)[0];	
	/* remove it from queue */
	ev_pop();	
	return scancode;
}


byte ASCIINames[128] =	
					{
/*	 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
 	 0  ,27 ,'1','2','3','4','5','6','7','8','9','0','-','=',8  ,9  ,		 /* 0 */
	'q','w','e','r','t','y','u','i','o','p','[',']',13 ,(0x80+0x1d),'a','s',	 /* 1 */
	'd','f','g','h','j','k','l',';',39 ,'`',(0x80+0x36),92 ,'z','x','c','v', /* 2 */
	'b','n','m',',','.','/',0  ,'*',(0x80+0x38),' ',0  ,(0x80+0x3b),(0x80+0x3c),(0x80+0x3d),(0x80+0x3e),(0x80+0x3f),	 /* 3 */
	(0x80+0x40),(0x80+0x41),(0x80+0x42),(0x80+0x43),(0x80+0x44),(0x80+0x57),(0x80+0x58),'7',0xad,'9',0x2d,0xac,'5',0xae,'+','1',		 /* 4 */
	0xaf,'3','0',127,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,		 /* 5 */
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,		 /* 6 */
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0			 /* 7 */
					};

void I_GetEvent(void)
{
	event_t event;	
	int scancode = 0;
	
	scancode = read_scancode();

	if (scancode == 0xe0 || scancode == 0xe1) 	/* need to read another character to know the key*/
		scancode = read_scancode();

	if (scancode==0)
		return;	
		
	if (scancode == 0x53) /* quit with DEL */
		raise(SIGINT);
	
	if (scancode > 0x7f) {
		event.type = ev_keyup;
		scancode -= 0x80;
		oldkeystate[scancode] = 0;
	}
	else {
		event.type = ev_keydown;
		/* stop key repeat */
		if (oldkeystate[scancode] == 1)
			return;
		oldkeystate[scancode] = 1;
	}
		
	if (ASCIINames[scancode]!=0)
		 event.data1 = ASCIINames[scancode];
	else event.data1 = scancode;
	
	
#ifdef DISABLEGRAPHICS 						
	printf("Posting keycode %d with type %d\n",event.data1,event.type);
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
	I_GetEvent();
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
    
    initkeyhandler();
   
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

