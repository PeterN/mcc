CFLAGS := -Wall -O3 -g -DINFINITY=HUGE_VAL -D_GNU_SOURCE
LDFLAGS := -lz -lpthread -lsqlite3 -lrt -ldl -lm

PNGCFLAGS := `pkg-config libpng12 --cflags`
PNGLDFLAGS := `pkg-config libpng12 --libs`

LIBSRC := astar.c
LIBSRC += astar_worker.c
LIBSRC += block.c
LIBSRC += chunk.c
LIBSRC += chunked_level.c
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
LIBSRC += module.c
LIBSRC += network.c
LIBSRC += npc.c
LIBSRC += packet.c
LIBSRC += perlin.c
LIBSRC += player.c
LIBSRC += playerdb.c
LIBSRC += queue.c
LIBSRC += timer.c
LIBSRC += undodb.c
LIBSRC += worker.c
LIBOBJ := $(LIBSRC:.c=.o)
LIBO := libmcc.so

HBSRC := heartbeat.c
HBOBJ := $(HBSRC:.c=.o)
HBO := heartbeat.so

IPCSRC := ipc.c
IPCOBJ := $(IPCSRC:.c=.o)
IPCO := ipc.so

IRCSRC := irc.c
IRCOBJ := $(IRCSRC:.c=.o)
IRCO := irc.so

MCCSRC := mcc.c
MCCOBJ := $(MCCSRC:.c=.o)
MCCO := mcc

SETRANKSRC := setrank.c
SETRANKOBJ := $(SETRANKSRC:.c=.o)
SETRANKO := setrank

WWSRC := wireworld.c
WWOBJ := $(WWSRC:.c=.o)
WWO := wireworld.so

SPLEEFSRC := spleef.c
SPLEEFOBJ := $(SPLEEFSRC:.c=.o)
SPLEEFO := spleef.so

TNTSRC := tnt.c
TNTOBJ := $(TNTSRC:.c=.o)
TNTO := tnt.so

ALSRC := airlayer.c
ALOBJ := $(ALSRC:.c=.o)
ALO := airlayer.so

PORTALSRC := portal.c
PORTALOBJ := $(PORTALSRC:.c=.o)
PORTALO := portal.so

SIGNSSRC := signs.c
SIGNSOBJ := $(SIGNSSRC:.c=.o)
SIGNSO := signs.so

ZOMBIESRC := zombies.c
ZOMBIEOBJ := $(ZOMBIESRC:.c=.o)
ZOMBIEO := zombies.so

NOHACKSSRC := nohacks.c
NOHACKSOBJ := $(NOHACKSSRC:.c=.o)
NOHACKSO := nohacks.so

NPCTESTSRC := npctest.c
NPCTESTOBJ := $(NPCTESTSRC:.c=.o)
NPCTESTO := npctest.so

DOORSSRC := doors.c
DOORSOBJ := $(DOORSSRC:.c=.o)
DOORSO := doors.so

BANIPSRC := banip.c
BANIPOBJ := $(BANIPSRC:.c=.o)
BANIPO := banip

RENDERSRC := render.c
RENDEROBJ := $(RENDERSRC:.c=.o)
RENDERO := render

RENDER2SRC := render_chunked.c
RENDER2OBJ := $(RENDER2SRC:.c=.o)
RENDER2O := render2

all: $(MCCO) $(WWO) $(SPLEEFO) $(TNTO) $(HBO) $(IRCO) $(ALO) $(IPCO) $(SETRANKO) $(BANIPO) $(PORTALO) $(DOORSO) $(RENDERO) $(RENDER2O) $(ZOMBIEO) $(SIGNSO) $(NOHACKSO) $(NPCTESTO)

clean:
	rm $(LIBOBJ) $(MCCOBJ) $(LIBO) $(MCCO) $(SPLEEFOBJ) $(SPLEEFO) $(WWOBJ) $(WWO) $(TNTOBJ) $(TNTO) $(HBOBJ) $(HBO) $(IRCOBJ) $(IRCO) $(ALOBJ) $(ALO) $(IPCOBJ) $(IPCO) $(SETRANKOBJ) $(SETRANKO) $(BANIPOBJ) $(BANIPO) $(PORTALOBJ) $(PORTALO) $(DOORSOBJ) $(DOORSO) $(RENDEROBJ) $(RENDERO) $(RENDER2OBJ) $(RENDER2O) $(ZOMBIEOBJ) $(ZOMBIEO) $(SIGNSO) $(SIGNSOBJ) $(NOHACKSO) $(NOHACKSOBJ) $(NPCTESTO) $(NPCTESTOBJ)

SOURCES = $(LIBSRC) $(MCCSRC) $(SPLEEFSRC) $(WWSRC) $(TNTSRC) $(HBSRC) $(IRCSRC) $(ALSRC) $(IPCSRC) $(SETRANKSRC) $(BANIPSRC) $(PORTALSRC) $(DOORSSRC) $(RENDERSRC) $(RENDER2SRC) $(ZOMBIESRC) $(SIGNSSRC) $(NOHACKSRC) $(NPCTESTSRC)

$(LIBO): $(LIBOBJ)
	$(CC) -shared -fPIC -Wl,-soname,libmcc.so -o $(LIBO) $(LIBOBJ)

$(HBO): $(HBOBJ)
	$(CC) -shared -fPIC -Wl,-soname,$(HBO) -o $(HBO) $(HBOBJ)

$(IPCO): $(IPCOBJ)
	$(CC) -shared -fPIC -Wl,-soname,$(IPCO) -o $(IPCO) $(IPCOBJ)

$(IRCO): $(IRCOBJ)
	$(CC) -shared -fPIC -Wl,-soname,$(IRCO) -o $(IRCO) $(IRCOBJ)

$(WWO): $(WWOBJ) $(LIBO)
	$(CC) -shared -fPIC -Wl,-soname,$(WWO) $(WWOBJ) -o $@

$(SPLEEFO): $(SPLEEFOBJ) $(LIBO)
	$(CC) -shared -fPIC -Wl,-soname,$(SPLEEFO) $(SPLEEFOBJ) -o $@

$(TNTO): $(TNTOBJ) $(LIBO)
	$(CC) -shared -fPIC -Wl,-soname,$(TNTO) $(TNTOBJ) -o $@

$(ALO): $(ALOBJ) $(LIBO)
	$(CC) -shared -fPIC -Wl,-soname,$(ALO) $(ALOBJ) -o $@

$(PORTALO): $(PORTALOBJ) $(LIBO)
	$(CC) -shared -fPIC -Wl,-soname,$(PORTALO) $(PORTALOBJ) -o $@

$(SIGNSO): $(SIGNSOBJ) $(LIBO)
	$(CC) -shared -fPIC -Wl,-soname,$(SIGNSO) $(SIGNSOBJ) -o $@

$(ZOMBIEO): $(ZOMBIEOBJ) $(LIBO)
	$(CC) -shared -fPIC -Wl,-soname,$(ZOMBIEO) $(ZOMBIEOBJ) -o $@

$(NOHACKSO): $(NOHACKSOBJ) $(LIBO)
	$(CC) -shared -fPIC -Wl,-soname,$(NOHACKSO) $(NOHACKSOBJ) -o $@

$(NPCTESTO): $(NPCTESTOBJ) $(LIBO)
	$(CC) -shared -fPIC -Wl,-soname,$(NPCTESTO) $(NPCTESTOBJ) -o $@

$(DOORSO): $(DOORSOBJ) $(LIBO)
	$(CC) -shared -fPIC -Wl,-soname,$(DOORSO) $(DOORSOBJ) -o $@

$(MCCO): $(MCCOBJ) $(LIBO)
	$(CC) $(LDFLAGS) $(MCCOBJ) -L. -lmcc -o $@

$(SETRANKO): $(SETRANKOBJ) $(LIBO)
	$(CC) $(LDFLAGS) $(SETRANKOBJ) -L. -lmcc -o $@

$(BANIPO): $(BANIPOBJ) $(LIBO)
	$(CC) $(LDFLAGS) $(BANIPOBJ) -L. -lmcc -o $@

$(RENDERO): $(RENDEROBJ) $(LIBO)
	$(CC) $(LDFLAGS) $(RENDEROBJ) -L. -lmcc $(PNGLDFLAGS) -o $@

$(RENDER2O): $(RENDER2OBJ) $(LIBO)
	$(CC) $(LDFLAGS) $(RENDER2OBJ) -L. -lmcc $(PNGLDFLAGS) -o $@

.c.o:
	$(CC) -c -fPIC $(CFLAGS) $< -o $@

%.d: %.c
	$(CC) -MM $(CFLAGS) $< | sed 's/$*.o/& $@/g' > $@
#	$(SHELL) -ec '$(CC) -M $(CPPFLAGS) $< | sed 's/$*.o/& $@/g' > $@'

include $(SOURCES:.c=.d)
