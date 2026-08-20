/* Emacs style mode select   -*- C++ -*- */
/*-----------------------------------------------------------------------------*/
/**/
/* DESCRIPTION:*/
/*	Wire protocol between the Doom game process (client, i_sound.c)*/
/*	and the sndserver process (server, sndserver.c).*/
/**/
/*	The pipe is one-way (game -> server, over the server's stdin,*/
/*	as set up by popen() in I_InitSound()) and carries ASCII text,*/
/*	one newline-terminated command per line. There is no ack/reply*/
/*	channel.*/
/**/
/*	This header is the single source of truth for the command*/
/*	characters and field widths, shared by both sides so they*/
/*	cannot drift out of sync.*/
/**/
/*-----------------------------------------------------------------------------*/

#ifndef __SB_PROTO__
#define __SB_PROTO__

/* p<id><pitch><vol><sep>\n -- play sound effect S_sfx[id].*/
/* Each field is exactly 2 hex digits (00-ff).*/
#define SB_PROTO_PLAY		'p'
#define SB_PROTO_PLAY_FMT	"p%2.2x%2.2x%2.2x%2.2x\n"

/* q\n -- shut down the server.*/
#define SB_PROTO_QUIT		'q'
#define SB_PROTO_QUIT_FMT	"q\n"

/* Longest line either side ever writes/reads, plus slack.*/
#define SB_PROTO_MAXLINE	32

#endif
/*-----------------------------------------------------------------------------*/
