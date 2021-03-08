# Xenix-doom
A port of Doom for Xenix 386.

![A short video of Doom running on Xenix](Doom_on_Xenix.gif)

I tested it under VMware with Xenix 2.3.4.

To compile, you need to install the original Development System, gcc2.5.8, and the CGI
graphics libraries.

Make sure to set the CGIDISP and CGIPATH environment variables, and to initialize the 
mouse (with mkdev mouse, I chose the keyboard mouse and activated it on tty1a).

If installed on a real machine, you probably need a 1Ghz+ CPU to have a playable game.
The CGI library is pretty slow and half-broken; the I_SetPalette call in particular 
is *very* slow because each color is set individually, due to a bug in the vsc_table
function.

# TODO: 
- The status bar/HUD is partially black (?)
- Implement I_UpdateNoBlit for faster refresh (but DOOM linux code always redraws whole scene...)
- Use a loop in I_GetEvent to consume all events in the queue?
- Use direct VGA access, expecially to change the palette in one go (see https://www.tuhs.org/Usenet/comp.unix.xenix.sco/1991-February/000624.html )