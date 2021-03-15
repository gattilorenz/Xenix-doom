## Xenix-doom
A port of Doom for Xenix 386.

![A short video of Doom running on Xenix](Doom_on_Xenix.gif)

I tested it under VMware with Xenix 2.3.4.

To compile, you need to install the original Development System and gcc2.5.8.

Make sure to initialize the mouse (with mkdev mouse, I chose the "keyboard mouse" and activated it on tty1a). The mouse is not actually used, but is mkdev mouse is required to 
use SCO's event manager (see chapter 10 of http://www.bitsavers.org/pdf/sco/system_V_2.x/Development_System/XG-10-10-88-5.0_2.3_XENIX_System_V_C_Language_Guide_Oct88.pdf)


##### Update
I rewrote the functions in i_video.c to use direct VGA access. The old CGI-based implementation is still available by defining USECGI before compilation. 

If using the CGI graphics libraries, make sure to set the CGIDISP (vga256) and CGIPATH (/usr/lib/cgi) environment variables. On a real machine, you probably need a 1Ghz+ CPU to have a playable game. The CGI library is pretty slow and half-broken; the I_SetPalette call in particular is *very* slow because each color is set individually, due to a bug in the vsc_table
function.


### TODO: 
- The status bar/HUD is partially black until automap is entered the 1st time (?)
- ~~Use a loop in I_GetEvent to consume all events in the queue?~~
- ~~Use direct VGA access, expecially to change the palette in one go (see https://www.tuhs.org/Usenet/comp.unix.xenix.sco/1991-February/000624.html, http://uw714doc.sco.com/en/man/html.7/display.7.html / http://www.polarhome.com/service/man/?qf=screen&tf=2&of=Xenix&sf=HW, http://web.mit.edu/ghostscript/src/ghostscript-8.14/src/gdevsco.c)~~