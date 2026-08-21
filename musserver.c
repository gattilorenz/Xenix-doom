/* Emacs style mode select   -*- C++ -*- */
/*-----------------------------------------------------------------------------*/
/**/
/* DESCRIPTION:*/
/*	Doom music player for the Xenix port, output via /dev/sbmidi.*/
/**/
/*	Spawned by I_InitSound() (see i_sound.c) alongside, but as a*/
/*	SEPARATE process from, sndserver -- reads play/stop/quit*/
/*	commands from its stdin, see sb_proto.h for the wire protocol.*/
/**/
/*	Why a separate process rather than folding this into sndserver:*/
/*	/dev/sbmidi's write() is not buffered/DMA-paced the way*/
/*	/dev/sbdsp's is. Reading sb.c directly (midi_write() in the*/
/*	driver) shows the KERNEL itself calls sleep() per MIDI byte,*/
/*	against each byte's scheduled time -- meaning a single write()*/
/*	call blocks the calling process for the real-time duration of*/
/*	however many packets were in that write. Doing that from inside*/
/*	sndserver's process would stall sound-effect mixing for as long*/
/*	as music is playing. The driver explicitly supports /dev/sbdsp*/
/*	and /dev/sbmidi being held open by two different processes at*/
/*	once (see the comment in midi_open() in sb.c), at the cost of*/
/*	possible brief gaps in DMA playback while MIDI bytes are being*/
/*	sent -- an acknowledged hardware limitation, not a bug here.*/
/**/
/*	Doom's music lumps are in the compact MUS format, not Standard*/
/*	MIDI File -- this parses MUS directly and emits the same*/
/*	4-byte (MIDI byte + 24-bit millisecond delay) packet format*/
/*	/dev/sbmidi expects, which the kernel then paces on its own.*/
/*	Output is batched in small chunks (not one huge write for the*/
/*	whole song) specifically so this process can check for a*/
/*	stop/replace command between batches without needing signals --*/
/*	since a single write() blocks for real time, that's the only*/
/*	way to stay responsive without introducing the kind of custom*/
/*	timing code that caused so much trouble in sndserver's own*/
/*	pacing (see that file's history). One known gap: a single*/
/*	packet's own delay can still make one write() block longer than*/
/*	a batch boundary would suggest, if a MUS event has an unusually*/
/*	long rest before it -- game music is not expected to have gaps*/
/*	long enough for this to matter in practice.*/
/**/
/*-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/lock.h>

#include "sb_proto.h"


/**/
/*	Minimal standalone WAD directory reader.*/
/*	Same approach and reasoning as sndserver.c's copy: mirrors the*/
/*	on-disk layout in w_wad.h without linking the whole zone/wad*/
/*	subsystem. Kept as a separate copy rather than a shared object*/
/*	file so sndserver.c and musserver.c stay independently buildable*/
/*	single-file programs, matching how the rest of this port's*/
/*	standalone tools are structured.*/
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
	fprintf(stderr, "musserver: couldn't open %s\n", path);
	exit(1);
    }

    if ( read(wad_fd, &header, sizeof(header)) != sizeof(header)
	 || ( strncmp(header.identification, "IWAD", 4)
	      && strncmp(header.identification, "PWAD", 4) ) )
    {
	fprintf(stderr, "musserver: %s is not a WAD file\n", path);
	exit(1);
    }

    wad_numlumps = header.numlumps;
    wad_dir = (wad_lump_t*) malloc(wad_numlumps * sizeof(wad_lump_t));
    if (!wad_dir)
    {
	fprintf(stderr, "musserver: out of memory (wad directory)\n");
	exit(1);
    }

    lseek(wad_fd, header.infotableofs, 0);
    if ( read(wad_fd, wad_dir, wad_numlumps * sizeof(wad_lump_t))
	 != wad_numlumps * (int)sizeof(wad_lump_t) )
    {
	fprintf(stderr, "musserver: short read on %s directory\n", path);
	exit(1);
    }
}


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
/*	MUS lump loading.*/
/**/

static unsigned char*	mus_data = 0;
static int		mus_len = 0;
static unsigned char*	mus_score = 0;	/* start of the event stream*/
static int		mus_scorelen = 0;

static int
be16
( unsigned char*	p )
{
    /* MUS headers are little-endian.*/
    return (p[0] | (p[1] << 8));
}


static int
mus_load
( char*		name )
{
    char	lumpname[16];
    int		lumpidx;
    int		size;
    unsigned char* data;

    sprintf(lumpname, "D_%s", name);
    lumpidx = wad_find(lumpname);
    if (lumpidx < 0)
    {
	fprintf(stderr, "musserver: %s not found\n", lumpname);
	return 0;
    }

    size = wad_dir[lumpidx].size;
    if (size < 16)
    {
	fprintf(stderr, "musserver: %s too short to be MUS\n", lumpname);
	return 0;
    }

    data = (unsigned char*) malloc(size);
    if (!data)
    {
	fprintf(stderr, "musserver: out of memory loading %s\n", lumpname);
	return 0;
    }

    lseek(wad_fd, wad_dir[lumpidx].filepos, 0);
    if ( read(wad_fd, data, size) != size )
    {
	fprintf(stderr, "musserver: short read on lump %s\n", lumpname);
	free(data);
	return 0;
    }

    if ( strncmp((char*) data, "MUS\032", 4) != 0 )
    {
	fprintf(stderr, "musserver: %s is not a MUS lump\n", lumpname);
	free(data);
	return 0;
    }

    if (mus_data)
	free(mus_data);

    mus_data = data;
    mus_len = size;
    mus_scorelen = be16(data+4);
    mus_score = data + be16(data+6);

    if ( mus_score + mus_scorelen > mus_data + mus_len )
    {
	/* Trust the WAD lump's own size over a corrupt/truncated*/
	/*  internal length field.*/
	mus_scorelen = (mus_data + mus_len) - mus_score;
    }

    if ( mus_scorelen <= 0 || mus_score < mus_data
	 || mus_score >= mus_data + mus_len )
    {
	/* A genuinely empty/malformed score would otherwise make*/
	/*  play_song() spin forever hitting mus_done on every pass*/
	/*  of a looping song, with nothing in that path to block on.*/
	fprintf(stderr, "musserver: %s has an empty score\n", lumpname);
	free(data);
	mus_data = 0;
	return 0;
    }

    return 1;
}


/**/
/*	MIDI packet output.*/
/*	Same 4-byte packet format (MIDI byte + 24-bit LE millisecond*/
/*	delay) the driver's /dev/sbmidi expects and paces itself,*/
/*	against lbolt, entirely in the kernel -- see the file header.*/
/**/

#define OUTPKTS		32	/* small on purpose -- see file header*/
#define MAXDELTA	0xffffffL

static unsigned char	outbuf[OUTPKTS * 4];
static int		outused = 0;
static long		last_ms = 0L;
static int		midi_fd = -1;


static int
flushout( void )
{
    int		n;

    if (outused == 0)
	return 1;

    n = outused * 4;
    if ( write(midi_fd, (char*) outbuf, n) != n )
    {
	fprintf(stderr, "musserver: write /dev/sbmidi failed\n");
	outused = 0;
	return 0;
    }
    outused = 0;
    return 1;
}


static int
emit
( int		mbyte,
  long		at_ms )
{
    long	delta;

    delta = at_ms - last_ms;
    if (delta < 0L)
	delta = 0L;
    if (delta > MAXDELTA)
	delta = MAXDELTA;
    last_ms += delta;

    outbuf[outused*4+0] = mbyte & 0xff;
    outbuf[outused*4+1] = delta & 0xff;
    outbuf[outused*4+2] = (delta >> 8) & 0xff;
    outbuf[outused*4+3] = (delta >> 16) & 0xff;
    outused++;

    if (outused >= OUTPKTS)
	return flushout();
    return 1;
}


static int
emitmsg
( unsigned char*	buf,
  int			n,
  long			at_ms )
{
    int		i;

    for (i=0 ; i<n ; i++)
	if ( !emit(buf[i], at_ms) )
	    return 0;
    return 1;
}


/**/
/*	MUS -> MIDI event translation.*/
/*	MUS format: a header (already parsed by mus_load()) followed by*/
/*	a single interleaved event stream (unlike SMF, there is only*/
/*	one stream to walk, no per-track merge needed). Ticks are a*/
/*	fixed 140 per second -- unlike SMF there is no tempo map, so*/
/*	this is a single constant conversion, not a running tempo*/
/*	state.*/
/**/
/*	Event byte: bit 7 = last event in this time-slice (a delta-time*/
/*	VLQ follows after this event's own data bytes); bits 6-4 = event*/
/*	type; bits 3-0 = channel (15 is always percussion, mapped to*/
/*	MIDI channel 9 -- the GM convention MUS's own channel 15*/
/*	predates but lines up with).*/
/**/

#define MUS_RELEASE	0
#define MUS_PLAY	1
#define MUS_PITCHBEND	2
#define MUS_SYSEVENT	3
#define MUS_CTRLCHANGE	4
#define MUS_SCOREEND	6

static unsigned char*	mus_p;
static unsigned char*	mus_end;
static int		mus_done;

static int	chan_vol[16];		/* last-known per-channel volume*/

/* MUS controller number (as sent in a type-4 event) -> MIDI CC*/
/*  number. Index 0 (program/instrument change) is handled*/
/*  separately below since it is a Program Change, not a CC.*/
static int	mus_ctrl_to_midi[10] =
	{ -1, 0, 1, 7, 10, 11, 91, 93, 64, 67 };


static int
mus_midichan
( int		ch )
{
    return (ch == 15) ? 9 : ch;
}


static long
mus_readvar( void )
{
    long	v = 0L;
    int		c;

    do
    {
	if (mus_p >= mus_end)
	{
	    mus_done = 1;
	    return 0L;
	}
	c = *mus_p++;
	v = (v << 7) | (c & 0x7f);
    } while (c & 0x80);

    return v;
}


/* Processes one event at time at_ms. Returns 1 if this was the last*/
/*  event of its time-slice (caller should read a delta next), else 0.*/
static int
mus_doevent
( long		at_ms )
{
    int		evbyte;
    int		type;
    int		ch;
    int		mch;
    int		last;
    int		d0;
    int		d1;
    unsigned char msg[3];

    if (mus_p >= mus_end)
    {
	mus_done = 1;
	return 1;
    }

    evbyte = *mus_p++;
    last = evbyte & 0x80;
    type = (evbyte >> 4) & 0x07;
    ch = evbyte & 0x0f;
    mch = mus_midichan(ch);

    switch (type)
    {
      case MUS_RELEASE:
	if (mus_p >= mus_end) { mus_done = 1; break; }
	d0 = *mus_p++ & 0x7f;
	msg[0] = 0x80 | mch;
	msg[1] = d0;
	msg[2] = 0;
	emitmsg(msg, 3, at_ms);
	break;

      case MUS_PLAY:
	if (mus_p >= mus_end) { mus_done = 1; break; }
	d0 = *mus_p++;
	if (d0 & 0x80)
	{
	    if (mus_p >= mus_end) { mus_done = 1; break; }
	    chan_vol[ch] = *mus_p++ & 0x7f;
	}
	msg[0] = 0x90 | mch;
	msg[1] = d0 & 0x7f;
	msg[2] = chan_vol[ch];
	emitmsg(msg, 3, at_ms);
	break;

      case MUS_PITCHBEND:
	if (mus_p >= mus_end) { mus_done = 1; break; }
	d0 = *mus_p++ & 0xff;
	{
	    int bend = d0 << 6;
	    if (bend > 0x3fff) bend = 0x3fff;
	    msg[0] = 0xe0 | mch;
	    msg[1] = bend & 0x7f;
	    msg[2] = (bend >> 7) & 0x7f;
	    emitmsg(msg, 3, at_ms);
	}
	break;

      case MUS_SYSEVENT:
	if (mus_p >= mus_end) { mus_done = 1; break; }
	d0 = *mus_p++ & 0x7f;
	msg[0] = 0xb0 | mch;
	switch (d0)
	{
	  case 10: msg[1] = 120; msg[2] = 0; emitmsg(msg,3,at_ms); break;
	  case 11: msg[1] = 123; msg[2] = 0; emitmsg(msg,3,at_ms); break;
	  case 12: msg[1] = 126; msg[2] = 0; emitmsg(msg,3,at_ms); break;
	  case 13: msg[1] = 127; msg[2] = 0; emitmsg(msg,3,at_ms); break;
	  case 14: msg[1] = 121; msg[2] = 0; emitmsg(msg,3,at_ms); break;
	  /* anything else: no MIDI equivalent, ignore*/
	}
	break;

      case MUS_CTRLCHANGE:
	if (mus_p + 2 > mus_end) { mus_done = 1; break; }
	d0 = *mus_p++ & 0x7f;
	d1 = *mus_p++ & 0x7f;
	if (d0 == 0)
	{
	    msg[0] = 0xc0 | mch;
	    msg[1] = d1;
	    emitmsg(msg, 2, at_ms);
	}
	else if (d0 >= 1 && d0 <= 9 && mus_ctrl_to_midi[d0] >= 0)
	{
	    msg[0] = 0xb0 | mch;
	    msg[1] = mus_ctrl_to_midi[d0];
	    msg[2] = d1;
	    emitmsg(msg, 3, at_ms);
	}
	break;

      case MUS_SCOREEND:
	mus_done = 1;
	break;

      default:
	/* Event types not used by any known MUS file (measure-end,*/
	/*  unused): no data bytes defined, nothing to do.*/
	break;
    }

    return last;
}


/* Sweeps every channel silent. Used on stop, on song change, and*/
/*  before exiting.*/
static void
mus_allnotesoff( void )
{
    unsigned char	m[3];
    int			ch;

    for (ch=0 ; ch<16 ; ch++)
    {
	m[0] = 0xb0 | ch;
	m[1] = 123; m[2] = 0; emitmsg(m, 3, last_ms);
	m[1] = 120; m[2] = 0; emitmsg(m, 3, last_ms);
    }
    flushout();
}


/**/
/*	Command parsing.*/
/*	Same select()-with-a-zero-timeout pattern already proven*/
/*	reliable in sndserver.c's drain_commands() -- see that file's*/
/*	notes on why select()'s TIMEOUT value is not to be trusted on*/
/*	this Xenix, but a zero-timeout readiness check is fine.*/
/**/

static char	cmdbuf[SB_PROTO_MAXLINE];
static int	cmdlen = 0;
static int	quitflag = 0;
static int	stopflag = 0;
static int	haveflag = 0;
static int	pendingloop = 0;
static char	pendingname[SB_PROTO_MAXLINE];


static void
process_line
( char*		line )
{
    if (line[0] == SB_PROTO_QUIT)
    {
	quitflag = 1;
	return;
    }

    if (line[0] == SB_PROTO_MUSSTOP)
    {
	stopflag = 1;
	return;
    }

    if (line[0] == SB_PROTO_MUSPLAY)
    {
	/* m<0|1><name>\n*/
	if (line[1] == '0' || line[1] == '1')
	{
	    pendingloop = (line[1] == '1');
	    strncpy(pendingname, line+2, sizeof(pendingname)-1);
	    pendingname[sizeof(pendingname)-1] = 0;
	    haveflag = 1;
	}
	return;
    }

    /* Unknown command: ignore.*/
}


static void
read_line_blocking( void )
{
    char	c;
    int		n;

    for ( ; ; )
    {
	n = read(0, &c, 1);
	if (n <= 0)
	{
	    quitflag = 1;
	    return;
	}
	if (c == '\n')
	{
	    cmdbuf[cmdlen] = 0;
	    process_line(cmdbuf);
	    cmdlen = 0;
	    return;
	}
	if (cmdlen < SB_PROTO_MAXLINE-1)
	    cmdbuf[cmdlen++] = c;
    }
}


/* Non-blocking: drains whatever is available right now without*/
/*  waiting. Used between output batches while a song is playing, so*/
/*  a stop/replace command is noticed within one batch instead of*/
/*  only once the whole song has played out.*/
static void
drain_commands_nonblock( void )
{
    fd_set		fdset;
    struct timeval	zerowait;
    char		chunk[64];
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

	n = read(0, chunk, sizeof(chunk));
	if (n <= 0)
	{
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


/**/
/*	Playing one song, with periodic checks for a new command.*/
/*	Returns when the song ends (and looping is off), or when*/
/*	stopped/replaced/quit from the command stream.*/
/**/

static void
play_song
( int		looping )
{
    long	abs_ms;
    long	ms_acc;
    long	delta;
    long	total;
    int		last;
    int		since_check;

    mus_p = mus_score;
    mus_end = mus_score + mus_scorelen;
    mus_done = 0;
    abs_ms = 0L;
    ms_acc = 0L;
    since_check = 0;

    while (!quitflag && !stopflag && !haveflag)
    {
	last = mus_doevent(abs_ms);

	if (last && !mus_done)
	{
	    delta = mus_readvar();
	    total = delta * 1000L + ms_acc;
	    abs_ms += total / 140L;
	    ms_acc = total % 140L;
	}

	if (mus_done)
	{
	    if (!looping)
		break;
	    /* Loop: rewind and keep the clock running so the next*/
	    /*  pass's deltas continue to schedule forward in real*/
	    /*  time rather than resetting to "now".*/
	    mus_p = mus_score;
	    mus_end = mus_score + mus_scorelen;
	    mus_done = 0;
	}

	if (++since_check >= OUTPKTS)
	{
	    since_check = 0;
	    drain_commands_nonblock();
	}
    }

    if (stopflag || haveflag)
    {
	/* Being stopped or replaced: anything still sitting in the*/
	/*  output batch is about to be stale (the notes it was about*/
	/*  to play no longer apply), so drop it rather than flush it*/
	/*  -- flushing would make the driver briefly keep playing the*/
	/*  old song's tail before this function's own all-notes-off*/
	/*  below has any effect.*/
	outused = 0;
    }
    else
    {
	/* Natural end of song (or quitflag): nothing stale about*/
	/*  what's queued, flush it normally.*/
	flushout();
    }

    mus_allnotesoff();
}


/**/

static void
usage( void )
{
    fprintf(stderr, "usage: musserver <wadpath> [-quiet]\n");
    exit(1);
}


int
main
( int		argc,
  char**	argv )
{
    int		i;
    int		quiet = 0;

    if (argc < 2)
	usage();

    for (i=2 ; i<argc ; i++)
	if (!strcmp(argv[i], "-quiet"))
	    quiet = 1;

    /* Same rationale as sndserver.c: keep this responsive even under*/
    /*  CPU contention from Doom's own renderer.*/
    nice(-15);
    plock(PROCLOCK);

    wad_open(argv[1]);

    midi_fd = open("/dev/sbmidi", O_WRONLY);
    if (midi_fd < 0)
    {
	fprintf(stderr, "musserver: couldn't open /dev/sbmidi\n");
	exit(1);
    }

    if (!quiet)
	fprintf(stderr, "musserver: ready\n");

    for (i=0 ; i<16 ; i++)
	chan_vol[i] = 127;

    while (!quitflag)
    {
	stopflag = 0;

	if (!haveflag)
	{
	    read_line_blocking();
	    continue;
	}

	haveflag = 0;
	if ( mus_load(pendingname) )
	    play_song(pendingloop);
    }

    mus_allnotesoff();
    close(midi_fd);

    return 0;
}
/*-----------------------------------------------------------------------------*/
