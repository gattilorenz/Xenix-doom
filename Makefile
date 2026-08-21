################################################################
#
# $Id:$
#
# $Log:$
#
CC=  gcc  # gcc or g++

CFLAGS=-g -w -DNORMALUNIX #-DUSECGI #-DDISABLEGRAPHICS
LDFLAGS=-L/usr/lib/386
LIBS=-lccgi -ltermlib -ltcap -lcurses -levent


# subdirectory for objects
O=xenix

# not too sophisticated dependency
#
# i_sound.c is deliberately NOT in this shared list: it needs to be
# compiled twice, once for each of the two binaries below (xnxdoom
# without sound, xnxdoom-snd with) -- see the targets themselves.
OBJS=				\
		$(O)/doomdef.o		\
		$(O)/doomstat.o		\
		$(O)/dstrings.o		\
		$(O)/i_system.o		\
		$(O)/i_video.o		\
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


# sndserver and musserver are separate standalone executables (see
# sndserver.c/musserver.c), each spawned via popen() by I_InitSound() at
# runtime -- neither is linked into xnxdoom itself. sndserver shares
# sounds.o with the main game so both processes agree on sound effect
# numbering; musserver needs no shared objects, since it identifies music
# by name over the wire rather than by an index into a shared table.
SNDSERVER_OBJS=			\
		$(O)/sndserver.o	\
		$(O)/sounds.o

all:	 $(O)/xnxdoom $(O)/xnxdoom-snd $(O)/sndserver $(O)/musserver
		 ln /proj/doom1.wad $(O)/doom1.wad
clean:
	rm -f *.o *~ *.flc
	rm -f xenix/*

# Sound-disabled build (the default). i_sound.c is compiled plain, with
# no defines -- I_InitSound() never spawns sndserver/musserver
# (everything else in i_sound.c already no-ops on its own once they
# stay NULL). This uses the ordinary $(O)/%.o pattern rule below, same
# as every other source file, since no extra flag is needed.
$(O)/xnxdoom:	$(OBJS) $(O)/i_sound.o $(O)/i_main.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(O)/i_sound.o $(O)/i_main.o \
	-o $(O)/xnxdoom $(LIBS)

# Sound-enabled build: same source tree, i_sound.c compiled with
# -DWITHSOUND so I_InitSound() spawns sndserver and musserver and
# talks to them over a pipe -- see sndserver.c, musserver.c and
# sb_proto.h. Deliberately its own object (i_sound_snd.o, not
# i_sound.o) and its own binary, not a flag on the shared xnxdoom
# target -- switching which variant you build should never risk
# silently linking a stale object compiled the other way.
$(O)/xnxdoom-snd:	$(OBJS) $(O)/i_sound_snd.o $(O)/i_main.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(O)/i_sound_snd.o $(O)/i_main.o \
	-o $(O)/xnxdoom-snd $(LIBS)

$(O)/i_sound_snd.o:	i_sound.c
	$(CC) $(CFLAGS) -DWITHSOUND -c i_sound.c -o $(O)/i_sound_snd.o

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
