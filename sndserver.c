/* Emacs style mode select   -*- C++ -*- */
/*-----------------------------------------------------------------------------*/
/**/
/* DESCRIPTION:*/
/*	Standalone sound-effect mixing/output process for the Xenix*/
/*	 port of xnxdoom.*/
/**/
/*	This program owns /dev/sbdsp for the lifetime of the game.*/
/*	It is spawned by I_InitSound() (see i_sound.c) via popen(),*/
/*	and reads play/quit commands from its stdin -- see sb_proto.h*/
/*	for the wire protocol.*/
/**/
/*	It exists as a separate process, rather than code in the main*/
/*	game binary, because /dev/sbdsp has no non-blocking write mode:*/
/*	write() blocks once both of the driver's kernel double-buffers*/
/*	are full. Doing that blocking write from inside the game loop*/
/*	would stall rendering/input on every buffer boundary. This is*/
/*	the same problem (and the same solution) as id's own Linux*/
/*	sndserver against /dev/dsp; see the port plan for details.*/
/**/
/*	This process loads all sound-effect sample data directly from*/
/*	the WAD itself (a self-contained WAD directory reader, not*/
/*	linked against w_wad.c/z_zone.c) so raw PCM never has to be*/
/*	piped through the text protocol. The sound-effect name table*/
/*	(S_sfx[], NUMSFX) is linked in from sounds.o, the same object*/
/*	the main game uses, so the two processes never disagree about*/
/*	sound effect numbering.*/
/**/
/*-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/lock.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/sb.h>

#include "doomtype.h"
#include "sounds.h"
#include "sb_proto.h"


/**/
/*	Mixer constants.*/
/**/

#define NUM_CHANNELS	8

/* Output chunk: mono 8-bit unsigned, so bytes == samples. Chosen to be*/
/*  exactly half of /dev/sbdsp's kernel double-buffer (DSP_BUF_SIZE,*/
/*  16KB by default or 4KB on the reduced-footprint kernel build) either*/
/*  way, so one mix-and-write cycle always overlaps cleanly with the*/
/*  driver keeping one buffer full while the other drains.*/
#define CHUNK_SAMPLES	2048

#define SAMPLERATE	11025	/* Hz. Matches the DMX sfx lumps' native rate.*/


/**/
/*	Minimal standalone WAD directory reader.*/
/*	Mirrors the on-disk layout in w_wad.h, but reimplemented here*/
/*	so this binary does not need to link the whole zone/wad*/
/*	subsystem -- see the port plan for why.*/
/**/

typedef struct
{
    char	identification[4];
    int		numlumps;
    int		infotableofs;
} wad_header_t;

typedef struct
{
    int		filepos;
    int		size;
    char	name[8];
} wad_lump_t;

static int		wad_fd;
static wad_lump_t*	wad_dir;
static int		wad_numlumps;


static void
wad_open
( char*		path )
{
    wad_header_t	header;

    wad_fd = open(path, O_RDONLY);
    if (wad_fd < 0)
    {
	fprintf(stderr, "sndserver: couldn't open %s\n", path);
	exit(1);
    }

    if ( read(wad_fd, &header, sizeof(header)) != sizeof(header)
	 || ( strncmp(header.identification, "IWAD", 4)
	      && strncmp(header.identification, "PWAD", 4) ) )
    {
	fprintf(stderr, "sndserver: %s is not a WAD file\n", path);
	exit(1);
    }

    wad_numlumps = header.numlumps;
    wad_dir = (wad_lump_t*) malloc(wad_numlumps * sizeof(wad_lump_t));
    if (!wad_dir)
    {
	fprintf(stderr, "sndserver: out of memory (wad directory)\n");
	exit(1);
    }

    lseek(wad_fd, header.infotableofs, 0);
    if ( read(wad_fd, wad_dir, wad_numlumps * sizeof(wad_lump_t))
	 != wad_numlumps * (int)sizeof(wad_lump_t) )
    {
	fprintf(stderr, "sndserver: short read on %s directory\n", path);
	exit(1);
    }
}


/* Case-insensitive, up-to-8-char compare of a lump name against a*/
/*  (possibly shorter, NUL-terminated) name.*/
static int
wad_namematch
( char*		lumpname,
  char*		name )
{
    int		i;
    char	c1;
    char	c2;

    for (i=0 ; i<8 ; i++)
    {
	c1 = lumpname[i];
	c2 = name[i];

	if (c1 >= 'a' && c1 <= 'z')
	    c1 -= 'a' - 'A';
	if (c2 >= 'a' && c2 <= 'z')
	    c2 -= 'a' - 'A';

	if (c1 != c2)
	    return 0;

	if (c2 == 0)
	    return 1;
    }

    return 1;
}


/* Returns -1 if not found. Scans backwards, as w_wad.c's*/
/*  W_CheckNumForName does, so a later lump of the same name wins.*/
static int
wad_find
( char*		name )
{
    int		i;

    for (i=wad_numlumps-1 ; i>=0 ; i--)
    {
	if ( wad_namematch(wad_dir[i].name, name) )
	    return i;
    }

    return -1;
}


/**/
/*	Sound-effect sample cache.*/
/*	Loaded once at startup, kept for the life of the process.*/
/**/

static unsigned char*	sfx_data[NUMSFX];
static int		sfx_length[NUMSFX];


static void
load_one_sfx
( int		sfxid )
{
    char		lumpname[16];
    int			lumpidx;
    int			size;
    int			paddedsize;
    unsigned char*	raw;
    unsigned char*	padded;
    int			i;

    sprintf(lumpname, "DS%s", S_sfx[sfxid].name);
    lumpidx = wad_find(lumpname);

    if (lumpidx < 0)
    {
	/* Same fallback id's own getsfx() used: DOOM II sounds requested*/
	/*  against a shareware WAD, or any other missing lump.*/
	fprintf(stderr, "sndserver: %s not found, using DSPISTOL\n", lumpname);
	lumpidx = wad_find("DSPISTOL");
    }

    if (lumpidx < 0)
    {
	fprintf(stderr, "sndserver: DSPISTOL fallback missing too, giving up\n");
	exit(1);
    }

    size = wad_dir[lumpidx].size;
    raw = (unsigned char*) malloc(size);
    if (!raw)
    {
	fprintf(stderr, "sndserver: out of memory loading %s\n", lumpname);
	exit(1);
    }

    lseek(wad_fd, wad_dir[lumpidx].filepos, 0);
    if ( read(wad_fd, raw, size) != size )
    {
	fprintf(stderr, "sndserver: short read on lump %s\n", lumpname);
	exit(1);
    }

    /* Sfx lumps start with an 8-byte DMX header (format id, sample*/
    /*  rate, sample count) that isn't sample data; skip it. Pad the*/
    /*  tail out to a chunk boundary with silence (128, the unsigned*/
    /*  8-bit zero level) so the mixer never walks past the end of*/
    /*  the buffer mid-chunk.*/
    paddedsize = ((size-8 + (CHUNK_SAMPLES-1)) / CHUNK_SAMPLES) * CHUNK_SAMPLES;
    padded = (unsigned char*) malloc(paddedsize);
    if (!padded)
    {
	fprintf(stderr, "sndserver: out of memory padding %s\n", lumpname);
	exit(1);
    }

    memcpy(padded, raw+8, size-8);
    for (i=size-8 ; i<paddedsize ; i++)
	padded[i] = 128;

    free(raw);

    sfx_data[sfxid] = padded;
    sfx_length[sfxid] = paddedsize;
}


static void
load_all_sfx( void )
{
    int		i;
    int		linkidx;

    for (i=1 ; i<NUMSFX ; i++)
    {
	if (!S_sfx[i].link)
	{
	    load_one_sfx(i);
	}
	else
	{
	    /* Alias, e.g. the chaingun sound linked to the pistol's.*/
	    linkidx = S_sfx[i].link - S_sfx;
	    sfx_data[i] = sfx_data[linkidx];
	    sfx_length[i] = sfx_length[linkidx];
	}
    }
}


/**/
/*	Mixer.*/
/*	Adapted from the dead in-process mixer that used to live in*/
/*	i_sound.c (addsfx()/I_UpdateSound()), with two changes:*/
/*	mono output instead of stereo, and separation degrading to*/
/*	a volume attenuation instead of true panning (this hardware*/
/*	has no stereo output at all).*/
/**/

static unsigned char*	channels[NUM_CHANNELS];
static unsigned char*	channelsend[NUM_CHANNELS];
static unsigned int	channelstep[NUM_CHANNELS];
static unsigned int	channelstepremainder[NUM_CHANNELS];
static long		channelstart[NUM_CHANNELS];
static int		channelids[NUM_CHANNELS];
static int*		channelvol_lookup[NUM_CHANNELS];

/* Pitch-to-step lookup, indexed directly by the raw 0-255 pitch byte*/
/*  sent over the wire (steptablemid is just a convenience alias so*/
/*  the pow() table is written out symmetrically around the middle).*/
static int		steptable[256];

/* Volume lookup: vol_lookup[vol*256+sample] gives the signed*/
/*  contribution of one 8-bit unsigned sample at that channel volume.*/
/*  At full volume (vol==127) this is (sample-128), i.e. an exact*/
/*  pass-through, so a single full-volume channel reproduces its*/
/*  source sample exactly once re-biased back to unsigned below.*/
static int		vol_lookup[128*256];

static long		addsfx_clock = 0;

static unsigned char	mixbuffer[CHUNK_SAMPLES];


static void
init_tables( void )
{
    int		i;
    int		j;
    int*	steptablemid = steptable + 128;

    for (i=-128 ; i<128 ; i++)
	steptablemid[i] = (int)(pow(2.0, (i/64.0)) * 65536.0);

    for (i=0 ; i<128 ; i++)
	for (j=0 ; j<256 ; j++)
	    vol_lookup[i*256+j] = ((j-128) * i) / 127;
}


static void
mixer_addsfx
( int		sfxid,
  int		volume,
  int		pitch,
  int		seperation )
{
    int		i;
    int		slot;
    int		oldestnum;
    long	oldest;
    int		s;
    int		vol;

    if (sfxid < 1 || sfxid >= NUMSFX)
	return;

    /* Chainsaw troubles: only one of these at a time.*/
    if ( sfxid == sfx_sawup
	 || sfxid == sfx_sawidl
	 || sfxid == sfx_sawful
	 || sfxid == sfx_sawhit
	 || sfxid == sfx_stnmov
	 || sfxid == sfx_pistol )
    {
	for (i=0 ; i<NUM_CHANNELS ; i++)
	{
	    if ( channels[i] && channelids[i] == sfxid )
	    {
		channels[i] = 0;
		break;
	    }
	}
    }

    /* Find the oldest channel, in case all are in use.*/
    oldest = addsfx_clock;
    oldestnum = 0;
    for (i=0; (i<NUM_CHANNELS) && (channels[i]); i++)
    {
	if (channelstart[i] < oldest)
	{
	    oldestnum = i;
	    oldest = channelstart[i];
	}
    }
    slot = (i == NUM_CHANNELS) ? oldestnum : i;

    channels[slot] = sfx_data[sfxid];
    channelsend[slot] = channels[slot] + sfx_length[sfxid];
    channelstep[slot] = steptable[pitch & 0xff];
    channelstepremainder[slot] = 0;
    channelstart[slot] = addsfx_clock++;
    channelids[slot] = sfxid;

    /* No hardware stereo: separation degrades to attenuating*/
    /*  off-center sources a little, rather than panning them.*/
    s = (seperation & 0xff) - 128;
    vol = volume - ((volume * s * s) >> 16);
    if (vol < 0) vol = 0;
    if (vol > 127) vol = 127;

    channelvol_lookup[slot] = &vol_lookup[vol*256];
}


static void
mix_chunk( void )
{
    int		i;
    int		chan;
    int		sample;
    int		acc;

    for (i=0 ; i<CHUNK_SAMPLES ; i++)
    {
	acc = 0;

	for (chan=0 ; chan<NUM_CHANNELS ; chan++)
	{
	    if (channels[chan])
	    {
		sample = *channels[chan];
		acc += channelvol_lookup[chan][sample];

		channelstepremainder[chan] += channelstep[chan];
		channels[chan] += channelstepremainder[chan] >> 16;
		channelstepremainder[chan] &= 65536-1;

		if (channels[chan] >= channelsend[chan])
		    channels[chan] = 0;
	    }
	}

	if (acc > 127)
	    acc = 127;
	else if (acc < -128)
	    acc = -128;

	mixbuffer[i] = (unsigned char)(acc + 128);
    }
}


/**/
/*	/dev/sbdsp handling.*/
/**/

static int	dsp_fd;


static void
dsp_open( void )
{
    dsp_fd = open("/dev/sbdsp", O_WRONLY);
    if (dsp_fd < 0)
    {
	fprintf(stderr, "sndserver: couldn't open /dev/sbdsp\n");
	exit(1);
    }

    if (ioctl(dsp_fd, DSP_IOCTL_RESET, 0) == -1)
	fprintf(stderr, "sndserver: ioctl DSP_IOCTL_RESET failed\n");

    if (ioctl(dsp_fd, DSP_IOCTL_SPEED, SAMPLERATE) == -1)
	fprintf(stderr, "sndserver: ioctl DSP_IOCTL_SPEED failed\n");

    if (ioctl(dsp_fd, DSP_IOCTL_VOICE, 1) == -1)
	fprintf(stderr, "sndserver: ioctl DSP_IOCTL_VOICE failed\n");
}


static void
dsp_close( void )
{
    /* close() does not auto-flush on this driver, by design (so a*/
    /*  killed process can't hang in close()) -- flush explicitly or*/
    /*  the last partial chunk is silently dropped.*/
    if (ioctl(dsp_fd, DSP_IOCTL_FLUSH, 0) == -1)
	fprintf(stderr, "sndserver: ioctl DSP_IOCTL_FLUSH failed\n");

    close(dsp_fd);
}


/**/
/*	Real-time pacing.*/
/*	dsp_write() in the driver only calls sb_start_dma() once a*/
/*	16KB kernel buffer becomes COMPLETELY full -- a chunk written*/
/*	into a still-filling buffer is invisible to the hardware,*/
/*	not merely delayed, until enough further writes complete*/
/*	that buffer. Since each chunk here is 2048 bytes against a*/
/*	16384-byte buffer, a newly mixed sound could otherwise sit*/
/*	behind up to 7 chunks of already-written silence before the*/
/*	driver ever starts playing it. Flushing after every write*/
/*	(see the main loop below) forces immediate hardware dispatch*/
/*	of each chunk instead, and its blocking-until-drained*/
/*	behavior doubles as this loop's pacing -- no separate sleep*/
/*	call is needed.*/
/**/


/**/
/*	Command parsing.*/
/*	Non-blocking poll of stdin, draining and applying any pending*/
/*	commands before each chunk is mixed. See sb_proto.h.*/
/**/
/*	Uses select() rather than poll(): poll() with a zero timeout*/
/*	was found to block indefinitely on this Xenix instead of*/
/*	returning immediately when no data is pending (tested against*/
/*	both a named pipe and, worse, hung the same way in early*/
/*	testing), whereas select() with a zeroed timeval behaves*/
/*	correctly on a real pipe (verified against the actual*/
/*	popen()-created pipe kind, not just a named pipe). This*/
/*	matches id's own Linux sndserver, which used select() for the*/
/*	same purpose against /dev/dsp.*/
/**/

static char	cmdbuf[SB_PROTO_MAXLINE];
static int	cmdlen = 0;
static int	quitflag = 0;


static void
process_line
( char*		line )
{
    int		id;
    int		pitch;
    int		vol;
    int		sep;

    if (line[0] == SB_PROTO_QUIT)
    {
	quitflag = 1;
	return;
    }

    if (line[0] == SB_PROTO_PLAY)
    {
	if ( sscanf(line+1, "%2x%2x%2x%2x", &id, &pitch, &vol, &sep) == 4 )
	    mixer_addsfx(id, vol, pitch, sep);
	return;
    }

    /* Unknown command: ignore.*/
}


static void
drain_commands( void )
{
    fd_set		fdset;
    struct timeval	zerowait;
    char		chunk[256];
    int			n;
    int			i;

    for ( ; ; )
    {
	FD_ZERO(&fdset);
	FD_SET(0, &fdset);
	zerowait.tv_sec = 0;
	zerowait.tv_usec = 0;

	if (select(1, &fdset, 0, 0, &zerowait) <= 0)
	    break;
	if ( !FD_ISSET(0, &fdset) )
	    break;

	n = read(0, chunk, sizeof(chunk));
	if (n <= 0)
	{
	    /* Pipe closed: the game process is gone.*/
	    quitflag = 1;
	    return;
	}

	for (i=0 ; i<n ; i++)
	{
	    if (chunk[i] == '\n')
	    {
		cmdbuf[cmdlen] = 0;
		process_line(cmdbuf);
		cmdlen = 0;
	    }
	    else if (cmdlen < SB_PROTO_MAXLINE-1)
	    {
		cmdbuf[cmdlen++] = chunk[i];
	    }
	}
    }
}


int
main
( int		argc,
  char**	argv )
{
    int		i;
    int		quiet = 0;

    if (argc < 2)
    {
	fprintf(stderr, "usage: sndserver <wadpath> [-quiet]\n");
	exit(1);
    }

    /* On this single-CPU emulated box, Doom's own CPU-bound software*/
    /*  renderer can starve this process of scheduling time for up*/
    /*  to a second or more, which is what was actually causing the*/
    /*  perceived audio latency -- pacing alone can't fix a process*/
    /*  that isn't getting run. Boost priority and lock in memory to*/
    /*  avoid that, same trick the original driver's own play_snd.c*/
    /*  uses for the identical reason.*/
    nice(-15);
    plock(PROCLOCK);

    for (i=2 ; i<argc ; i++)
	if (!strcmp(argv[i], "-quiet"))
	    quiet = 1;

    if (!quiet)
	fprintf(stderr, "sndserver: loading sound effects from %s\n", argv[1]);

    wad_open(argv[1]);
    init_tables();
    load_all_sfx();
    dsp_open();

    if (!quiet)
	fprintf(stderr, "sndserver: ready\n");

    while (!quitflag)
    {
	drain_commands();

	if (quitflag)
	    break;

	mix_chunk();

	if ( write(dsp_fd, mixbuffer, CHUNK_SAMPLES) != CHUNK_SAMPLES )
	{
	    fprintf(stderr, "sndserver: write /dev/sbdsp failed\n");
	    break;
	}

	/* Force this chunk to hardware right now instead of leaving it*/
	/*  in a still-filling 16KB buffer -- see the comment above.*/
	/*  This also blocks until it's actually drained, which is what*/
	/*  paces this loop; no separate pacing call is needed.*/
	if ( ioctl(dsp_fd, DSP_IOCTL_FLUSH, 0) == -1 )
	    fprintf(stderr, "sndserver: ioctl DSP_IOCTL_FLUSH failed\n");
    }

    dsp_close();

    return 0;
}
/*-----------------------------------------------------------------------------*/
