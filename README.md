## Xenix-doom
A port of Doom for Xenix 386 (now with sound support!).

![A short video of Doom running on Xenix](Doom_on_Xenix.gif)

I tested it under Xenix 2.3.4; on my Pentium 166Mhz it runs at full speed.

To compile it, you need to install the original Development System, gcc 2.5.8 (and the CGI graphics library if you want to build the cgi/cgisound variants, read below) and gmake.

Make sure to initialize the mouse (with mkdev mouse, I chose the "keyboard mouse" and activated it on tty1a). Mouse support is not actually implemented, but mkdev mouse is required to use SCO's event manager for reading keyboard events (see chapter 10 of http://www.bitsavers.org/pdf/sco/system_V_2.x/Development_System/XG-10-10-88-5.0_2.3_XENIX_System_V_C_Language_Guide_Oct88.pdf).

### Build
Run `gmake` for the list of targets: `vga` and `cgi` pick the graphics backend, `vgasound`/`cgisound` add sound. All four produce `xenix/xnxdoom`, so build only one variant at a time.

VGA is the default and fastest choice. CGI is needed for EGA cards but is **s l o w** -- on a real machine you probably need a 1Ghz+ CPU, since Doom's I_SetPalette call is a particular bottleneck due to a bug in CGI's vsc_table function. It theoretically runs on EGA cards as well, using a customized palette taken from https://www.doomworld.com/idgames/graphics/ega_pal. Set the CGIDISP (vga256 or ega) and CGIPATH (/usr/lib/cgi) environment variables at runtime for the cgi/cgisound builds.

The sound variants need the [SoundBlaster driver for Xenix](https://github.com/gattilorenz/xenix-sb-driver) and also build the sndserver/musserver companion processes. Music is sent via MIDI to an external synthesizer for timing/CPU/simplicity reasons -- if you're on 86Box, I recommend FluidSynth with a Roland SC-55 soundfont (e.g. [this one](https://github.com/nitro-shoe/sc-55-soundfont/releases/)).

Note that the driver and sound code are mostly written by Claude.

### TODO: 
- fix the CGI implementation; add cgifctns.h, see if slowness can be addressed
- ~~add sound~~
- ~~The status bar/HUD background is black until automap is entered the first time (?)~~
- ~~Use a loop in I_GetEvent to consume all events in the queue?~~
- ~~Use direct VGA access, expecially to change the palette in one go (see https://www.tuhs.org/Usenet/comp.unix.xenix.sco/1991-February/000624.html, http://uw714doc.sco.com/en/man/html.7/display.7.html / http://www.polarhome.com/service/man/?qf=screen&tf=2&of=Xenix&sf=HW, http://web.mit.edu/ghostscript/src/ghostscript-8.14/src/gdevsco.c)~~
