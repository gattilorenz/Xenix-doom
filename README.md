# Xenix-doom
A port of Doom for Xenix 386.

I tested it under VMware with Xenix 2.3.4.

To compile, you need to install the original Development System, gcc2.5.8, and the CGI
graphics libraries.

Make sure to set the CGIDISP and CGIPATH environment variables. No input support yet,
but you'll probably need to initialize the mouse (with mkdev mouse, I chose the
keyboard mouse and activated it on tty1a).

