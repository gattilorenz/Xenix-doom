################################################################
#
# $Id:$
#
# $Log:$
#
CC=  gcc  # gcc or g++

CFLAGS=-g -w -DNORMALUNIX
LDFLAGS=-L/usr/lib/386
LIBS=-lccgi -ltermlib -ltcap -lcurses -levent


# subdirectory for objects
O=xenix

# not too sophisticated dependency
#
# i_sound.c and i_video.c are compiled separately per target, below.
OBJS=				\
		$(O)/doomdef.o		\
		$(O)/doomstat.o		\
		$(O)/dstrings.o		\
		$(O)/i_system.o		\
		$(O)/i_net.o			\
		$(O)/tables.o			\
		$(O)/f_finale.o		\
		$(O)/f_wipe.o 		\
		$(O)/d_main.o			\
		$(O)/d_net.o			\
		$(O)/d_items.o		\
		$(O)/g_game.o			\
		$(O)/m_menu.o			\
		$(O)/m_misc.o			\
		$(O)/m_argv.o  		\
		$(O)/m_bbox.o			\
		$(O)/m_fixed.o		\
		$(O)/m_swap.o			\
		$(O)/m_cheat.o		\
		$(O)/m_random.o		\
		$(O)/am_map.o			\
		$(O)/p_ceilng.o		\
		$(O)/p_doors.o		\
		$(O)/p_enemy.o		\
		$(O)/p_floor.o		\
		$(O)/p_inter.o		\
		$(O)/p_lights.o		\
		$(O)/p_map.o			\
		$(O)/p_maputl.o		\
		$(O)/p_plats.o		\
		$(O)/p_pspr.o			\
		$(O)/p_setup.o		\
		$(O)/p_sight.o		\
		$(O)/p_spec.o			\
		$(O)/p_switch.o		\
		$(O)/p_mobj.o			\
		$(O)/p_telept.o		\
		$(O)/p_tick.o			\
		$(O)/p_saveg.o		\
		$(O)/p_user.o			\
		$(O)/r_bsp.o			\
		$(O)/r_data.o			\
		$(O)/r_draw.o			\
		$(O)/r_main.o			\
		$(O)/r_plane.o		\
		$(O)/r_segs.o			\
		$(O)/r_sky.o			\
		$(O)/r_things.o		\
		$(O)/w_wad.o			\
		$(O)/wi_stuff.o		\
		$(O)/v_video.o		\
		$(O)/st_lib.o			\
		$(O)/st_stuff.o		\
		$(O)/hu_stuff.o		\
		$(O)/hu_lib.o			\
		$(O)/s_sound.o		\
		$(O)/z_zone.o			\
		$(O)/info.o				\
		$(O)/strings.o				\
		$(O)/sounds.o


# sndserver/musserver are standalone executables spawned by
# I_InitSound() at runtime, not linked into xnxdoom.
SNDSERVER_OBJS=			\
		$(O)/sndserver.o	\
		$(O)/sounds.o

.PHONY:	help vga cgi vgasound cgisound clean

help:
	@echo "Usage: gmake <target>"
	@echo ""
	@echo "  vga       -- direct VGA graphics, no sound"
	@echo "  cgi       -- CGI graphics library, no sound"
	@echo "  vgasound  -- direct VGA graphics, with sound"
	@echo "  cgisound  -- CGI graphics library, with sound"
	@echo "  clean     -- remove all built objects and binaries"
	@echo ""
	@echo "Every target produces xenix/xnxdoom -- pick one variant at a"
	@echo "time, since each overwrites the same binary name. vgasound"
	@echo "and cgisound also build xenix/sndserver and xenix/musserver."
	@echo "See the README for what each variant needs at compile/run time."

# .PHONY so switching targets always relinks xnxdoom, since they all
# share that one output name.

vga:	$(OBJS) $(O)/i_video.o $(O)/i_sound.o $(O)/i_main.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(O)/i_video.o $(O)/i_sound.o $(O)/i_main.o \
	-o $(O)/xnxdoom $(LIBS)
	-ln /proj/doom1.wad $(O)/doom1.wad

cgi:	$(OBJS) $(O)/i_video_cgi.o $(O)/i_sound.o $(O)/i_main.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(O)/i_video_cgi.o $(O)/i_sound.o $(O)/i_main.o \
	-o $(O)/xnxdoom $(LIBS)
	-ln /proj/doom1.wad $(O)/doom1.wad

vgasound:	$(OBJS) $(O)/i_video.o $(O)/i_sound_snd.o $(O)/i_main.o $(O)/sndserver $(O)/musserver
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(O)/i_video.o $(O)/i_sound_snd.o $(O)/i_main.o \
	-o $(O)/xnxdoom $(LIBS)
	-ln /proj/doom1.wad $(O)/doom1.wad

cgisound:	$(OBJS) $(O)/i_video_cgi.o $(O)/i_sound_snd.o $(O)/i_main.o $(O)/sndserver $(O)/musserver
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(O)/i_video_cgi.o $(O)/i_sound_snd.o $(O)/i_main.o \
	-o $(O)/xnxdoom $(LIBS)
	-ln /proj/doom1.wad $(O)/doom1.wad

clean:
	rm -f *.o *~ *.flc
	rm -f xenix/*

$(O)/i_sound_snd.o:	i_sound.c
	$(CC) $(CFLAGS) -DWITHSOUND -c i_sound.c -o $(O)/i_sound_snd.o

$(O)/i_video_cgi.o:	i_video.c
	$(CC) $(CFLAGS) -DUSECGI -c i_video.c -o $(O)/i_video_cgi.o

$(O)/sndserver:	$(SNDSERVER_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SNDSERVER_OBJS) \
	-o $(O)/sndserver -lm

$(O)/musserver:	$(O)/musserver.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(O)/musserver.o \
	-o $(O)/musserver

$(O)/%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

#############################################################
#
#############################################################
