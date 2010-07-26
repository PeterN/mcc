CFLAGS := -Wall -g -DINFINITY=HUGE_VAL
LDFLAGS := -lz -lpthread -lsqlite3 -lrt -ldl -lm

LIBSRC := block.c
LIBSRC += client.c
LIBSRC += colour.c
LIBSRC += commands.c
LIBSRC += config.c
LIBSRC += cuboid.c
LIBSRC += faultgen.c
LIBSRC += filter.c
LIBSRC += hashtable.c
LIBSRC += hashtable_itr.c
LIBSRC += hashtable_utility.c
LIBSRC += level.c
LIBSRC += module.c
LIBSRC += network.c
LIBSRC += packet.c
LIBSRC += perlin.c
LIBSRC += player.c
LIBSRC += playerdb.c
LIBSRC += timer.c
LIBSRC += undodb.c
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


all: $(MCCO) $(WWO) $(SPLEEFO) $(TNTO) $(HBO) $(IRCO) $(ALO) $(IPCO)

clean:
	rm $(LIBOBJ) $(MCCOBJ) $(LIBO) $(MCCO) $(SPLEEFOBJ) $(SPLEEFO) $(WWOBJ) $(WWO) $(TNTOBJ) $(TNTO) $(HBOBJ) $(HBO) $(IRCOBJ) $(IRCO) $(ALOBJ) $(ALO) $(IPCOBJ) $(IPCO)

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

$(MCCO): $(MCCOBJ) $(LIBO)
	$(CC) $(LDFLAGS) $(MCCOBJ) -L. -lmcc -o $@

.c.o:
	$(CC) -c -fPIC $(CFLAGS) $< -o $@

# DO NOT DELETE
