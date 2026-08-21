/* Emacs style mode select   -*- C++ -*- */
/*-----------------------------------------------------------------------------*/
/**/
/* DESCRIPTION:*/
/*	Wire protocol between the Doom game process (client, i_sound.c)*/
/*	and the two audio server processes it spawns: sndserver.c (sound*/
/*	effects, over /dev/sbdsp) and musserver.c (music, over*/
/*	/dev/sbmidi). They are separate processes -- see musserver.c's*/
/*	own notes for why -- but share this one protocol header and the*/
/*	same shape of pipe.*/
/**/
/*	Each pipe is one-way (game -> server, over the server's stdin,*/
/*	as set up by popen() in I_InitSound()) and carries ASCII text,*/
/*	one newline-terminated command per line. There is no ack/reply*/
/*	channel. sndserver only ever sees the SB_PROTO_PLAY/QUIT commands*/
/*	below; musserver only ever sees the SB_PROTO_MUS* ones.*/
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

/* q\n -- shut down the server (sndserver or musserver).*/
#define SB_PROTO_QUIT		'q'
#define SB_PROTO_QUIT_FMT	"q\n"

/* m<0|1><name>\n -- musserver: register and play music lump D_<NAME>*/
/*  (name matches musicinfo_t.name in sounds.c, e.g. "e1m1"). The*/
/*  digit is the looping flag: 1 loops the song until stopped or*/
/*  replaced (normal level music), 0 plays it once then goes idle*/
/*  (title screen, intermission, cast/cutscene stingers).*/
#define SB_PROTO_MUSPLAY	'm'
#define SB_PROTO_MUSPLAY_FMT	"m%d%s\n"

/* x\n -- musserver: stop whatever is playing and go idle.*/
#define SB_PROTO_MUSSTOP	'x'
#define SB_PROTO_MUSSTOP_FMT	"x\n"

/* Longest line either side ever writes/reads, plus slack. Longest*/
/*  music name in sounds.c is well under 16 chars.*/
#define SB_PROTO_MAXLINE	32

#endif
/*-----------------------------------------------------------------------------*/
