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
/*	System interface for sound.*/
/**/
/*	This is the client side of the sndserver/musserver architecture*/
/*	(see sndserver.c, musserver.c and sb_proto.h): all of this file*/
/*	does is spawn those two processes and write commands to their*/
/*	pipes. No sample data, mixing, or MUS parsing happens here --*/
/*	see the port plan for why sound effects and music each need*/
/*	their own separate process rather than living in the game's own*/
/*	process (in short: neither /dev/sbdsp nor /dev/sbmidi has a*/
/*	non-blocking write mode, so the game process cannot own either*/
/*	directly without risking a stall -- and sound effects and music*/
/*	cannot share ONE process either, since a single write() to*/
/*	/dev/sbmidi blocks for the real-time duration of however much*/
/*	music it just wrote, which would starve sound-effect mixing for*/
/*	as long as music is playing).*/
/**/
/*-----------------------------------------------------------------------------*/

static const char
rcsid[] = "$Id: i_unix.c,v 1.5 1997/02/03 22:45:10 b1 Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "i_sound.h"
#include "d_main.h"
#include "w_wad.h"
#include "sb_proto.h"

#include "doomdef.h"
#include "doomstat.h"

/* Separate sound-effects and music server processes, each talked to*/
/*  over its own pipe.*/
FILE*	sndserver = 0;
char*	sndserver_filename = "./sndserver";
FILE*	musserver = 0;
char*	musserver_filename = "./musserver";


/**/
/* SFX API*/
/**/

/* Initialize channels? Nothing to do: the mixing tables live in*/
/*  sndserver, not here.*/
void I_SetChannels()
{
}


/**/
/* Retrieve the raw data lump index for a given SFX name.*/
/* Used only for s_sound.c's own bookkeeping; the game process*/
/*  never touches the actual sample data (sndserver loads that*/
/*  itself, straight from the WAD).*/
/**/
int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    char namebuf[9];
    sprintf(namebuf, "ds%s", sfx->name);
    return W_GetNumForName(namebuf);
}


/**/
/* Starting a sound means sending a 'play' command to sndserver.*/
/* Priority is ignored, as with the original id reference*/
/*  implementation this is adapted from.*/
/**/
int
I_StartSound
( int		id,
  int		vol,
  int		sep,
  int		pitch,
  int		priority )
{
  /* UNUSED.*/
  priority = 0;

  /* s_sound.c keeps volume on DOOM's native 0-15 scale (snd_SfxVolume's*/
  /*  own range, see m_menu.c's slider) -- see also its own commented-*/
  /*  out "*8" next to S_SetSfxVolume(), the original id source's own*/
  /*  reminder that this scaling has to happen somewhere. sndserver's*/
  /*  mixer indexes vol_lookup[vol*256+sample] with vol expected in*/
  /*  0-127 (see sndserver.c); left unscaled, every sound played at*/
  /*  roughly 1/8th of the intended level -- quiet enough on real*/
  /*  hardware to pass as silence. Scale here, at the wire-protocol*/
  /*  boundary, so s_sound.c's own 0-15 comparisons against*/
  /*  snd_SfxVolume elsewhere stay correct.*/
  vol *= 8;
  if (vol > 127)
    vol = 127;

  if (sndserver)
  {
    fprintf(sndserver, SB_PROTO_PLAY_FMT, id, pitch, vol, sep);
    fflush(sndserver);
  }

  /* Not a real handle -- sndserver never acknowledges anything back*/
  /*  over the (one-way) pipe. Matches id's own SNDSERV behaviour.*/
  return id;
}


void I_StopSound (int handle)
{
  /* UNUSED. The wire protocol has no stop command -- see the port*/
  /*  plan for why this is an accepted MVP gap. Sounds either play*/
  /*  to completion or get evicted by sndserver's own oldest-channel*/
  /*  stealing when all 8 of its channels are busy.*/
  handle = 0;
}


int I_SoundIsPlaying(int handle)
{
    /* Ouch. Since sndserver never reports completion back, this is*/
    /*  only ever a rough guess, same as the original reference.*/
    return gametic < handle;
}


void
I_UpdateSoundParams
( int	handle,
  int	vol,
  int	sep,
  int	pitch)
{
  /* UNUSED. No update-in-place command in the wire protocol.*/
  handle = vol = sep = pitch = 0;
}


/* Nothing to update/submit here: sndserver mixes and writes its own*/
/*  buffers on its own schedule, paced by the blocking write() to*/
/*  /dev/sbdsp.*/
void I_UpdateSound( void )
{
}

void I_SubmitSound(void)
{
}


void I_ShutdownSound(void)
{
  if (sndserver)
  {
    fprintf(sndserver, SB_PROTO_QUIT_FMT);
    fflush(sndserver);
    pclose(sndserver);
    sndserver = 0;
  }
}


#ifdef WITHSOUND
/* Shared by I_InitSound() below to spawn either server process the*/
/*  same way: "<filename> <wadpath> -quiet" over a popen() pipe.*/
/**/
/* logname's stdout/stderr are redirected to a log file rather than*/
/*  left inherited from this process. I_InitGraphics() (see*/
/*  i_video.c) later switches the console into raw VGA mode 13h via*/
/*  ioctl(0, SW_VGA13, 0) -- any stray text either server writes to*/
/*  that console afterwards (e.g. an mus_load() error mid-game) would*/
/*  land on a linear-framebuffer-mapped device expecting pixel data,*/
/*  not characters, which is a known way to wedge real/emulated PC*/
/*  VGA hardware. Truncated (">"), not appended, so each run's log*/
/*  reflects only that run.*/
static FILE*
spawn_server
( char*		what,
  char*		filename,
  char*		wadpath,
  char*		logname )
{
  char	buffer[1024];
  FILE*	f;

  sprintf(buffer, "%s %s -quiet >%s 2>&1", filename, wadpath, logname);

  if ( access(filename, X_OK) )
  {
    fprintf(stderr, "I_InitSound: could not find %s [%s]\n", what, filename);
    return 0;
  }

  f = popen(buffer, "w");
  if (!f)
    fprintf(stderr, "I_InitSound: could not start %s [%s]\n", what, buffer);
  return f;
}
#endif


void
I_InitSound()
{
#ifndef WITHSOUND
  /* Sound support left out of this build by default -- see the*/
  /*  Makefile's xnxdoom target (xnxdoom-snd is the sound-enabled*/
  /*  one). sndserver/musserver stay NULL, so everything above*/
  /*  already no-ops on its own "if (sndserver)"/"if (musserver)"*/
  /*  checks; nothing else to gate.*/
  fprintf(stderr, "I_InitSound: built without sound support\n");
#else
  /* wadfiles[0] is the primary IWAD path, set up by IdentifyVersion()*/
  /*  and already opened by W_InitMultipleFiles() by the time I_Init()*/
  /*  (and hence I_InitSound()) runs -- see D_DoomMain(). Both servers*/
  /*  read their data straight out of that same file.*/
  if (!wadfiles[0])
  {
    fprintf(stderr, "I_InitSound: no WAD file, not starting sound servers\n");
    return;
  }

  /* If either server's popen()'d shell can't actually start it (bad*/
  /*  path in the saved config, missing binary, etc.), the pipe ends*/
  /*  up with no reader. The very next write to it would otherwise*/
  /*  raise SIGPIPE, whose default disposition kills this whole*/
  /*  process outright -- mid-VGA-mode, with the keyboard driver in*/
  /*  raw scancode mode, which is as ugly a crash as it sounds. Losing*/
  /*  sound entirely is fine; losing the whole game over it is not.*/
  signal(SIGPIPE, SIG_IGN);

  sndserver = spawn_server("sound server", sndserver_filename, wadfiles[0], "sndserver.log");
  musserver = spawn_server("music server", musserver_filename, wadfiles[0], "musserver.log");
#endif
}


/**/
/* MUSIC API.*/
/* Starting a song means sending a 'play' command to musserver, same*/
/*  shape as I_StartSound() above. musserver loops the song on its*/
/*  own (per the looping flag) until told to stop or play something*/
/*  else -- see musserver.c.*/
/**/
void I_InitMusic(void)		{ }

void I_ShutdownMusic(void)
{
  if (musserver)
  {
    fprintf(musserver, SB_PROTO_QUIT_FMT);
    fflush(musserver);
    pclose(musserver);
    musserver = 0;
  }
}

void I_SetMusicVolume(int volume) { volume = 0; }

/* UNUSED. musserver has no pause/resume in its wire protocol -- true*/
/*  mid-song pause was never implemented even in id's own reference*/
/*  (I_PauseSong/I_ResumeSong were dummies there too), so this is a*/
/*  pre-existing gap, not a new one.*/
void I_PauseSong (int handle)
{
  handle = 0;
}

void I_ResumeSong (int handle)
{
  handle = 0;
}

/* Remembers the song name between I_RegisterSong() and I_PlaySong(),*/
/*  the same way I_StartSound() needs no state of its own between*/
/*  calls -- these two are always called back-to-back from*/
/*  S_ChangeMusic(), so one slot is enough.*/
static char	registered_name[32] = "";

int I_RegisterSong(char* name)
{
  strncpy(registered_name, name, sizeof(registered_name)-1);
  registered_name[sizeof(registered_name)-1] = 0;

  /* Not a real handle -- matches I_StartSound()'s own convention.*/
  return 1;
}

void
I_PlaySong
( int		handle,
  int		looping )
{
  handle = 0;

  if (musserver)
  {
    fprintf(musserver, SB_PROTO_MUSPLAY_FMT, looping ? 1 : 0, registered_name);
    fflush(musserver);
  }
}

void I_StopSong(int handle)
{
  handle = 0;

  if (musserver)
  {
    fprintf(musserver, SB_PROTO_MUSSTOP_FMT);
    fflush(musserver);
  }
}

void I_UnRegisterSong(int handle)
{
  handle = 0;
  registered_name[0] = 0;
}
