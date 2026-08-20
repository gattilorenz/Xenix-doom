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
/*	This is the client side of the sndserver architecture (see*/
/*	sndserver.c and sb_proto.h): all of this file does is spawn*/
/*	the sndserver process and write play/quit commands to its*/
/*	pipe. No sample data or mixing happens here -- see the port*/
/*	plan for why (in short: /dev/sbdsp has no non-blocking write*/
/*	mode, so the game process cannot own it directly without*/
/*	risking a stall on every buffer boundary).*/
/**/
/*-----------------------------------------------------------------------------*/

static const char
rcsid[] = "$Id: i_unix.c,v 1.5 1997/02/03 22:45:10 b1 Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "i_sound.h"
#include "d_main.h"
#include "w_wad.h"
#include "sb_proto.h"

#include "doomdef.h"
#include "doomstat.h"

/* Separate sound server process, talked to over a pipe.*/
FILE*	sndserver = 0;
char*	sndserver_filename = "./sndserver";


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


void
I_InitSound()
{
#ifdef NOSOUND
  /* Sound support left out of this build entirely -- see the*/
  /*  Makefile's xnxdoom-nosound target. sndserver stays NULL, so*/
  /*  I_StartSound()/I_ShutdownSound() above already no-op on their*/
  /*  own "if (sndserver)" checks; nothing else to gate.*/
  fprintf(stderr, "I_InitSound: built without sound support\n");
#else
  char buffer[1024];

  /* wadfiles[0] is the primary IWAD path, set up by IdentifyVersion()*/
  /*  and already opened by W_InitMultipleFiles() by the time I_Init()*/
  /*  (and hence I_InitSound()) runs -- see D_DoomMain(). sndserver*/
  /*  reads sound effect data straight out of that same file.*/
  if (!wadfiles[0])
  {
    fprintf(stderr, "I_InitSound: no WAD file, not starting sndserver\n");
    return;
  }

  sprintf(buffer, "%s %s -quiet", sndserver_filename, wadfiles[0]);

  if ( !access(sndserver_filename, X_OK) )
  {
    sndserver = popen(buffer, "w");
    if (!sndserver)
      fprintf(stderr, "I_InitSound: could not start sound server [%s]\n", buffer);
  }
  else
    fprintf(stderr, "I_InitSound: could not find sound server [%s]\n",
	    sndserver_filename);
#endif
}


/**/
/* MUSIC API.*/
/* Still no music done.*/
/* Remains. Dummies.*/
/**/
void I_InitMusic(void)		{ }
void I_ShutdownMusic(void)	{ }
void I_SetMusicVolume(int volume) { volume = 0; }

static int	looping=0;
static int	musicdies=-1;

void I_PlaySong(int handle, int looping)
{
  /* UNUSED.*/
  handle = looping = 0;
  musicdies = gametic + TICRATE*30;
}

void I_PauseSong (int handle)
{
  /* UNUSED.*/
  handle = 0;
}

void I_ResumeSong (int handle)
{
  /* UNUSED.*/
  handle = 0;
}

void I_StopSong(int handle)
{
  /* UNUSED.*/
  handle = 0;

  looping = 0;
  musicdies = 0;
}

void I_UnRegisterSong(int handle)
{
  /* UNUSED.*/
  handle = 0;
}

int I_RegisterSong(void* data)
{
  /* UNUSED.*/
  data = NULL;

  return 1;
}

/* Is the song playing?*/
int I_QrySongPlaying(int handle)
{
  /* UNUSED.*/
  handle = 0;
  return looping || musicdies > gametic;
}
