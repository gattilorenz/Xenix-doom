## Xenix-doom
A port of Doom for Xenix 386 (no sound or network support).

![A short video of Doom running on Xenix](Doom_on_Xenix.gif)

I tested it under Xenix 2.3.4; on my Pentium 166Mhz it runs at full speed.

To compile it, you need to install the original Development System and gcc 2.5.8 (and the CGI graphics library if you want to compile with USECGI, read below).

Make sure to initialize the mouse (with mkdev mouse, I chose the "keyboard mouse" and activated it on tty1a). Mouse support is not actually implemented, but mkdev mouse is required to use SCO's event manager for reading keyboard events (see chapter 10 of http://www.bitsavers.org/pdf/sco/system_V_2.x/Development_System/XG-10-10-88-5.0_2.3_XENIX_System_V_C_Language_Guide_Oct88.pdf).

### Video libraries
i_video.c uses direct VGA access by default. 
The CGI graphics library can still be used by compiling with -DUSECGI, but it is **s l o w**. On a real machine, you probably need a 1Ghz+ CPU to have a playable game. Doom's I_SetPalette call in particular is a bottleneck because each color is set individually, due to a bug in CGI's vsc_table function. On the other hand, the CGI version theoretically runs on EGA cards as well, using a customized palette taken from https://www.doomworld.com/idgames/graphics/ega_pal.

To run the CGI version, make sure to set the CGIDISP (vga256 or ega) and CGIPATH (/usr/lib/cgi) environment variables.


### TODO: 
- The status bar/HUD background is black until automap is entered the first time (?)
- ~~Use a loop in I_GetEvent to consume all events in the queue?~~
- ~~Use direct VGA access, expecially to change the palette in one go (see https://www.tuhs.org/Usenet/comp.unix.xenix.sco/1991-February/000624.html, http://uw714doc.sco.com/en/man/html.7/display.7.html / http://www.polarhome.com/service/man/?qf=screen&tf=2&of=Xenix&sf=HW, http://web.mit.edu/ghostscript/src/ghostscript-8.14/src/gdevsco.c)~~
