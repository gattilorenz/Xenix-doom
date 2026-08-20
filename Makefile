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
# with sound, doom-mute without) -- see the targets themselves.
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


# sndserver is a separate standalone executable (see sndserver.c), spawned
# via popen() by I_InitSound() at runtime -- it is not linked into xnxdoom
# itself. It shares sounds.o with the main game so both processes agree on
# sound effect numbering.
SNDSERVER_OBJS=			\
		$(O)/sndserver.o	\
		$(O)/sounds.o

all:	 $(O)/xnxdoom $(O)/doom-mute $(O)/sndserver
		 ln /proj/doom1.wad $(O)/doom1.wad
clean:
	rm -f *.o *~ *.flc
	rm -f xenix/*

# Sound-enabled build (the normal one). I_InitSound() spawns
# sndserver and talks to it over a pipe -- see sndserver.c and
# sb_proto.h.
$(O)/xnxdoom:	$(OBJS) $(O)/i_sound.o $(O)/i_main.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(O)/i_sound.o $(O)/i_main.o \
	-o $(O)/xnxdoom $(LIBS)

# Sound-disabled build: same source tree, i_sound.c compiled with
# -DNOSOUND so I_InitSound() never spawns sndserver (everything
# else in i_sound.c already no-ops on its own once sndserver stays
# NULL). Deliberately its own object (i_sound_nosound.o, not
# i_sound.o) and its own binary, not a flag on the shared xnxdoom
# target -- switching which variant you build should never risk
# silently linking a stale object compiled the other way.
$(O)/doom-mute:	$(OBJS) $(O)/i_sound_nosound.o $(O)/i_main.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(O)/i_sound_nosound.o $(O)/i_main.o \
	-o $(O)/doom-mute $(LIBS)

$(O)/i_sound_nosound.o:	i_sound.c
	$(CC) $(CFLAGS) -DNOSOUND -c i_sound.c -o $(O)/i_sound_nosound.o

$(O)/sndserver:	$(SNDSERVER_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SNDSERVER_OBJS) \
	-o $(O)/sndserver -lm

$(O)/%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

#############################################################
#
#############################################################
