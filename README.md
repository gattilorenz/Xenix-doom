# Xenix-doom
A port of Doom for Xenix 386.

I tested it under VMware with Xenix 2.3.4.

To compile, you need to install the original Development System, gcc2.5.8, and the CGI
graphics libraries.

Make sure to set the CGIDISP and CGIPATH environment variables, and to initialize the 
mouse (with mkdev mouse, I chose the keyboard mouse and activated it on tty1a).

TODO: 
- The status bar/HUD is partially black (?)
- Implement I_UpdateNoBlit for faster refresh
- Use a loop in I_GetEvent to consume all events in the queue?
