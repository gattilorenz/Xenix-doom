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

#define POINTER_WARP_COUNTDOWN	1

#include <stdio.h>
#include <signal.h>

short devhandle,savin[20],savary[66];
#define DISABLE_RENDER 1
#define  GMODE  1
#define  CMODE  0
#define  BOTTOM 0
#define  LEFT   0
#define  CENTER 1

void box(short x, short y, short w, short h);
void report_error(char func[], short mode);
void fatal(short errnum, char func[]);
void waitcr();

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



#ifndef __M_XENIX
/* Blocky mode,*/
/* replace each 320x200 pixel with multiply*multiply pixels.*/
/* According to Dave Taylor, it still is a bonehead thing*/
/* to use ....*/
static int	multiply=1;
#endif




void I_ShutdownGraphics(void)
{
	printf("I_ShutdownGraphics\n");

	v_clswk( devhandle );
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


}


/**/
/* I_StartTic*/
/**/

void SendKeypress(int key) {
	event_t ev;
	ev.type = ev_keydown;
	ev.data1 = key;
	D_PostEvent(&ev);
	ev.type = ev_keyup;
	D_PostEvent(&ev);
}

void I_StartTic (void)
{
	short ktype,direction;
	char key;
	event_t ev;
	
	ktype = vrd_curkeys(devhandle,2,&direction,&key);
	if (ktype>0) {
		if (ktype==1) {
			ev.type = ev_keydown;
			switch (direction){
				case 8:
					ev.data1=KEY_UPARROW;
					break;
				case 2:
					ev.data1=KEY_DOWNARROW;
					break;
				case 4:
					ev.data1=KEY_LEFTARROW;
					break;				
				case 6:
					ev.data1=KEY_RIGHTARROW;			
			}	
			/*D_PostEvent(&ev);*/
			SendKeypress(ev.data1);
		}
	}
	else {
		ev.type=ev_keydown;
		ev.data1=KEY_ENTER;
		SendKeypress(ev.data1);
		
	}
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

}



void I_InitGraphics(void)
{
	
	short error;
	if (getenv("CGIDISP") == NULL) {
    	printf("No CGIDISP env variable found\n");
    	printf("Make sure you correctly set CGIPATH and CGIDISP\n");
    	printf("CGIPATH=/usr/lib/cgi\n");
    	printf("CGIDISP=/usr/lib/vga256\n");
    	printf("export CGIPATH CGIDISP\n");
    	exit(1);
    }
	
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
	if (error < 0)
	{
		printf("Cgitest Error %d opening device display\n",vq_error());
	printf("Check the environment variables: CGIPATH and CGIDISP.\n" );
		exit (-1);
	}
    

}



void InitExpand (void)
{
}


void InitExpand2 (void)
{
}


void report_error(char func[], short mode)
{
	short errnum, err_ptsin[2];
	char   blanks[80], err_cstrng[80];
	short  i;

	/* initialize output strings */
	for (i=0; i<79; i++) blanks[i] = ' ';
	blanks[79] = '\000';
	errnum = -(vq_error());
	sprintf (err_cstrng,
		"Error no. %d in CGI function %s.  Tap `RETURN' to continue...",
		(short)errnum, func);

	switch (mode) {
		case GMODE:
			if (savin[10] && savary[45] == 0) { 

				short attribs[10], horiz, vert;

				/* save current attributes; set alignment to {center,bottom} */
				if (vqt_attributes(devhandle, attribs) < 0 ||
					 vst_alignment(devhandle,CENTER,BOTTOM,&horiz,&vert) < 0)
					fatal(errnum,func);

				/* fill the message area with blanks, then write the prompt */
				if (v_gtext (devhandle, 16384, 31600, blanks) < 0 ||
					 v_gtext (devhandle, 16384, 31600, err_cstrng) < 0)
					fatal(errnum,func);

				/* wait for string input */
				err_ptsin[0] = 0;
				err_ptsin[1] = 0;
				if (vrq_string(devhandle, 80, 0, err_ptsin, err_cstrng) < 0)
					fatal(errnum,func);

				/* reinstate alignment */
				if (vst_alignment(devhandle,attribs[3],attribs[4],&horiz,&vert) < 0)
					fatal(errnum,func);
			}
		break;
		case CMODE: {
			short row, column;

			/* save current cursor position */
			if (vq_curaddress(devhandle, &row, &column) < 0) 
				fatal(errnum,func);

			/* move cursor to position (2,0) */
			if (vs_curaddress(devhandle, 2, 0) < 0)
				fatal(errnum,func);

			/* fill message area with blanks & overwrite error message */
			if (v_curtext(devhandle, blanks) < 0 ||
				 vs_curaddress(devhandle, 2, 0) < 0 ||
				 v_curtext(devhandle, err_cstrng) < 0)
				fatal(errnum,func);

			/* wait for string input */
			err_ptsin[0] = 0;
			err_ptsin[1] = 0;
			if (vrq_string(devhandle, 80, 0, err_ptsin, err_cstrng) < 0)
				fatal(errnum,func);

			/* restore cursor position */
			if (vs_curaddress(devhandle, row, column) < 0)
				fatal(errnum,func);
		}
	}
}



void fatal(short errnum, char func[])
{
	/* close the workstation */
	v_clswk(devhandle);           /* no recourse on error */

	/* now reopen and reclose it to leave it in default state */
	v_opnwk(savin, &devhandle, savary);
	v_enter_cur(devhandle);       /* (make sure we exit in cursor mode) */
	v_clswk(devhandle);

	/* write fatal message (using stdio) and bug out */
	printf ("Fatal error no. %d in CGI function %s.\n", (int)errnum, func);
	exit (-2);
}

