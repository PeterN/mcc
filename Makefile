CFLAGS := -Wall -Werror -O3 -g -DINFINITY=HUGE_VAL -D_GNU_SOURCE
LDFLAGS := -lz -lpthread -lsqlite3 -lrt -ldl -lm

PNGCFLAGS := `pkg-config libpng12 --cflags`
PNGLDFLAGS := `pkg-config libpng12 --libs`

LIBSRC := astar.c
LIBSRC += astar_worker.c
LIBSRC += block.c
LIBSRC += client.c
LIBSRC += colour.c
LIBSRC += commands.c
LIBSRC += config.c
LIBSRC += cuboid.c
LIBSRC += faultgen.c
LIBSRC += filter.c
LIBSRC += hash.c
LIBSRC += hook.c
LIBSRC += landscape.c
LIBSRC += land2.c
LIBSRC += level.c
LIBSRC += level_worker.c
LIBSRC += md5.c
LIBSRC += module.c
LIBSRC += network.c
LIBSRC += network_worker.c
LIBSRC += npc.c
LIBSRC += packet.c
LIBSRC += perlin.c
LIBSRC += player.c
LIBSRC += playerdb.c
LIBSRC += queue.c
LIBSRC += socket.c
LIBSRC += timer.c
LIBSRC += undodb.c
LIBSRC += worker.c
LIBOBJ := $(LIBSRC:.c=.o)
LIBO := libmcc.so

MCCSRC := mcc.c
MCCOBJ := $(MCCSRC:.c=.o)
MCCO := mcc

SETRANKSRC := setrank.c
SETRANKOBJ := $(SETRANKSRC:.c=.o)
SETRANKO := setrank

IMAGESRC := image.c render.c
IMAGEOBJ := $(IMAGESRC:.c=.o)
IMAGEO := image.so

BANIPSRC := banip.c
BANIPOBJ := $(BANIPSRC:.c=.o)
BANIPO := banip

MODULESSRC := 8ball.c airlayer.c book.c cannon.c corecmds.c decoration.c doors.c heartbeat.c\
irc.c nohacks.c npctest.c portal.c signs.c spleef.c trap.c tnt.c wireworld.c zombies.c log.c

MODULESOBJ := $(MODULESSRC:.c=.o)
MODULESO := $(MODULESSRC:.c=.so)

all: $(LIBO) $(MCCO) $(SETRANKO) $(BANIPO) $(IMAGEO) $(MODULESO)

clean:
	rm -f *.d $(LIBOBJ) $(LIBO) $(MCCOBJ) $(MCCO) $(SETRANKOBJ) $(SETRANKO) $(BANIPOBJ) $(BANIPO) $(IMAGEOBJ) $(IMAGEO) $(MODULESOBJ) $(MODULESO)

SOURCES = $(LIBSRC) $(MCCSRC) $(SETRANKSRC) $(BANIPSRC) $(IMAGESRC) $(MODULESSRC)

$(LIBO): $(LIBOBJ)
	$(CC) -shared -fPIC -Wl,-soname,libmcc.so -o $(LIBO) $(LIBOBJ)

$(MODULESO): $(MODULESOBJ) $(LIBO)

%.so: %.o
	$(CC) -shared -fPIC -Wl,-soname,$@ $< -o $@

$(IMAGEO): $(IMAGEOBJ) $(LIBO)
	$(CC) $(LDFLAGS) -shared -fPIC -Wl,-soname,$(IMAGEO) $(IMAGEOBJ) $(PNGLDFLAGS) -o $@

$(MCCO): $(MCCOBJ) $(LIBO)
	$(CC) $(LDFLAGS) $(MCCOBJ) -L. -lmcc -o $@

$(SETRANKO): $(SETRANKOBJ) $(LIBO)
	$(CC) $(LDFLAGS) $(SETRANKOBJ) -L. -lmcc -o $@

$(BANIPO): $(BANIPOBJ) $(LIBO)
	$(CC) $(LDFLAGS) $(BANIPOBJ) -L. -lmcc -o $@

%.o: %.c
	$(CC) -c -fPIC $(CFLAGS) $< -o $@

%.d: %.c
	$(CC) -MM $(CFLAGS) $< | sed 's/$*.o/& $@/g' > $@
#	$(SHELL) -ec '$(CC) -M $(CPPFLAGS) $< | sed 's/$*.o/& $@/g' > $@'

-include $(SOURCES:.c=.d)
