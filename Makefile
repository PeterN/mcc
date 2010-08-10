CFLAGS := -Wall -O3 -g -DINFINITY=HUGE_VAL -D_GNU_SOURCE
LDFLAGS := -lz -lpthread -lsqlite3 -lrt -ldl -lm

PNGCFLAGS := `pkg-config libpng12 --cflags`
PNGLDFLAGS := `pkg-config libpng12 --libs`

LIBSRC := block.c
LIBSRC += chunk.c
LIBSRC += chunked_level.c
LIBSRC += client.c
LIBSRC += colour.c
LIBSRC += commands.c
LIBSRC += config.c
LIBSRC += cuboid.c
LIBSRC += faultgen.c
LIBSRC += filter.c
LIBSRC += hook.c
LIBSRC += landscape.c
LIBSRC += level.c
LIBSRC += module.c
LIBSRC += network.c
LIBSRC += packet.c
LIBSRC += perlin.c
LIBSRC += player.c
LIBSRC += playerdb.c
LIBSRC += queue.c
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

all: $(MCCO) $(WWO) $(SPLEEFO) $(TNTO) $(HBO) $(IRCO) $(ALO) $(IPCO) $(SETRANKO) $(BANIPO) $(PORTALO) $(DOORSO) $(RENDERO) $(RENDER2O)

clean:
	rm $(LIBOBJ) $(MCCOBJ) $(LIBO) $(MCCO) $(SPLEEFOBJ) $(SPLEEFO) $(WWOBJ) $(WWO) $(TNTOBJ) $(TNTO) $(HBOBJ) $(HBO) $(IRCOBJ) $(IRCO) $(ALOBJ) $(ALO) $(IPCOBJ) $(IPCO) $(SETRANKOBJ) $(SETRANKO) $(BANIPOBJ) $(BANIPO) $(PORTALOBJ) $(PORTALO) $(DOORSOBJ) $(DOORSO) $(RENDEROBJ) $(RENDERO) $(RENDER2OBJ) $(RENDER2O)

depend:
	makedepend $(LIBSRC) $(MCCSRC) $(SPLEEFSRC) $(WWSRC) $(TNTSRC) $(HBSRC) $(IRCSRC) $(ALSRC) $(IPCSRC) $(SETRANKSRC) $(BANIPSRC) $(PORTALSRC) $(DOORSSRC) $(RENDERSRC) $(RENDER2SRC)

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

# DO NOT DELETE

block.o: /usr/include/stdio.h /usr/include/features.h
block.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
block.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
block.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
block.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
block.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
block.o: /usr/include/string.h /usr/include/strings.h mcc.h
block.o: /usr/include/time.h /usr/include/bits/time.h block.h
block.o: /usr/include/stdint.h /usr/include/bits/wchar.h
block.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
block.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
block.o: /usr/include/bits/posix2_lim.h bitstuff.h list.h
block.o: /usr/include/stdlib.h /usr/include/sys/types.h /usr/include/endian.h
block.o: /usr/include/bits/endian.h /usr/include/sys/select.h
block.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
block.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
block.o: /usr/include/alloca.h rank.h level.h /usr/include/pthread.h
block.o: /usr/include/sched.h /usr/include/bits/sched.h /usr/include/signal.h
block.o: /usr/include/bits/setjmp.h physics.h position.h client.h
block.o: /usr/include/netinet/in.h /usr/include/sys/socket.h
block.o: /usr/include/sys/uio.h /usr/include/bits/uio.h
block.o: /usr/include/bits/socket.h /usr/include/bits/sockaddr.h
block.o: /usr/include/asm/socket.h /usr/include/asm/sockios.h
block.o: /usr/include/bits/in.h /usr/include/bits/byteswap.h hook.h packet.h
block.o: colour.h player.h
chunk.o: /usr/include/zlib.h /usr/include/zconf.h /usr/include/zlibdefs.h
chunk.o: /usr/include/sys/types.h /usr/include/features.h
chunk.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
chunk.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
chunk.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
chunk.o: /usr/include/time.h /usr/include/bits/time.h /usr/include/endian.h
chunk.o: /usr/include/bits/endian.h /usr/include/sys/select.h
chunk.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
chunk.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
chunk.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
chunk.o: /usr/include/bits/confname.h /usr/include/getopt.h chunk.h block.h
chunk.o: /usr/include/stdint.h /usr/include/bits/wchar.h
chunk.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
chunk.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
chunk.o: /usr/include/bits/posix2_lim.h bitstuff.h list.h
chunk.o: /usr/include/stdlib.h /usr/include/alloca.h mcc.h
chunk.o: /usr/include/stdio.h /usr/include/libio.h /usr/include/_G_config.h
chunk.o: /usr/include/wchar.h /usr/include/bits/stdio_lim.h
chunk.o: /usr/include/bits/sys_errlist.h rank.h landscape.h
chunked_level.o: /usr/include/stdio.h /usr/include/features.h
chunked_level.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
chunked_level.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
chunked_level.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
chunked_level.o: /usr/include/libio.h /usr/include/_G_config.h
chunked_level.o: /usr/include/wchar.h /usr/include/bits/stdio_lim.h
chunked_level.o: /usr/include/bits/sys_errlist.h /usr/include/stdint.h
chunked_level.o: /usr/include/bits/wchar.h /usr/include/stdlib.h
chunked_level.o: /usr/include/sys/types.h /usr/include/time.h
chunked_level.o: /usr/include/bits/time.h /usr/include/endian.h
chunked_level.o: /usr/include/bits/endian.h /usr/include/sys/select.h
chunked_level.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
chunked_level.o: /usr/include/sys/sysmacros.h
chunked_level.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
chunked_level.o: /usr/include/string.h chunk.h block.h /usr/include/limits.h
chunked_level.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
chunked_level.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
chunked_level.o: bitstuff.h list.h mcc.h rank.h chunked_level.h
chunked_level.o: /usr/include/pthread.h /usr/include/sched.h
chunked_level.o: /usr/include/bits/sched.h /usr/include/signal.h
chunked_level.o: /usr/include/bits/setjmp.h landscape.h queue.h
client.o: /usr/include/stdio.h /usr/include/features.h
client.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
client.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
client.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
client.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
client.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
client.o: /usr/include/string.h /usr/include/dirent.h
client.o: /usr/include/bits/dirent.h /usr/include/bits/posix1_lim.h
client.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h client.h
client.o: /usr/include/netinet/in.h /usr/include/stdint.h
client.o: /usr/include/bits/wchar.h /usr/include/sys/socket.h
client.o: /usr/include/sys/uio.h /usr/include/sys/types.h /usr/include/time.h
client.o: /usr/include/bits/time.h /usr/include/endian.h
client.o: /usr/include/bits/endian.h /usr/include/sys/select.h
client.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
client.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
client.o: /usr/include/bits/uio.h /usr/include/bits/socket.h
client.o: /usr/include/limits.h /usr/include/bits/posix2_lim.h
client.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
client.o: /usr/include/asm/sockios.h /usr/include/bits/in.h
client.o: /usr/include/bits/byteswap.h hook.h list.h /usr/include/stdlib.h
client.o: /usr/include/alloca.h mcc.h packet.h position.h commands.h player.h
client.o: rank.h block.h bitstuff.h colour.h playerdb.h level.h
client.o: /usr/include/pthread.h /usr/include/sched.h
client.o: /usr/include/bits/sched.h /usr/include/signal.h
client.o: /usr/include/bits/setjmp.h physics.h network.h
colour.o: /usr/include/strings.h colour.h
commands.o: /usr/include/stdio.h /usr/include/features.h
commands.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
commands.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
commands.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
commands.o: /usr/include/libio.h /usr/include/_G_config.h
commands.o: /usr/include/wchar.h /usr/include/bits/stdio_lim.h
commands.o: /usr/include/bits/sys_errlist.h /usr/include/string.h
commands.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
commands.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
commands.o: /usr/include/bits/posix2_lim.h /usr/include/dirent.h
commands.o: /usr/include/bits/dirent.h /usr/include/sys/types.h
commands.o: /usr/include/time.h /usr/include/bits/time.h
commands.o: /usr/include/endian.h /usr/include/bits/endian.h
commands.o: /usr/include/sys/select.h /usr/include/bits/select.h
commands.o: /usr/include/bits/sigset.h /usr/include/sys/sysmacros.h
commands.o: /usr/include/bits/pthreadtypes.h /usr/include/sys/stat.h
commands.o: /usr/include/bits/stat.h /usr/include/unistd.h
commands.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
commands.o: /usr/include/getopt.h /usr/include/sys/time.h client.h
commands.o: /usr/include/netinet/in.h /usr/include/stdint.h
commands.o: /usr/include/bits/wchar.h /usr/include/sys/socket.h
commands.o: /usr/include/sys/uio.h /usr/include/bits/uio.h
commands.o: /usr/include/bits/socket.h /usr/include/bits/sockaddr.h
commands.o: /usr/include/asm/socket.h /usr/include/asm/sockios.h
commands.o: /usr/include/bits/in.h /usr/include/bits/byteswap.h hook.h list.h
commands.o: /usr/include/stdlib.h /usr/include/alloca.h mcc.h packet.h
commands.o: position.h level.h /usr/include/pthread.h /usr/include/sched.h
commands.o: /usr/include/bits/sched.h /usr/include/signal.h
commands.o: /usr/include/bits/setjmp.h block.h bitstuff.h rank.h physics.h
commands.o: player.h colour.h playerdb.h network.h module.h undodb.h
config.o: /usr/include/string.h /usr/include/features.h
config.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
config.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h mcc.h
config.o: /usr/include/stdio.h /usr/include/bits/types.h
config.o: /usr/include/bits/typesizes.h /usr/include/libio.h
config.o: /usr/include/_G_config.h /usr/include/wchar.h
config.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
config.o: /usr/include/time.h /usr/include/bits/time.h
cuboid.o: block.h /usr/include/stdint.h /usr/include/features.h
cuboid.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
cuboid.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
cuboid.o: /usr/include/bits/wchar.h /usr/include/limits.h
cuboid.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
cuboid.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
cuboid.o: bitstuff.h list.h /usr/include/stdlib.h /usr/include/sys/types.h
cuboid.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
cuboid.o: /usr/include/time.h /usr/include/bits/time.h /usr/include/endian.h
cuboid.o: /usr/include/bits/endian.h /usr/include/sys/select.h
cuboid.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
cuboid.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
cuboid.o: /usr/include/alloca.h mcc.h /usr/include/stdio.h
cuboid.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
cuboid.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
cuboid.o: rank.h cuboid.h /usr/include/string.h client.h
cuboid.o: /usr/include/netinet/in.h /usr/include/sys/socket.h
cuboid.o: /usr/include/sys/uio.h /usr/include/bits/uio.h
cuboid.o: /usr/include/bits/socket.h /usr/include/bits/sockaddr.h
cuboid.o: /usr/include/asm/socket.h /usr/include/asm/sockios.h
cuboid.o: /usr/include/bits/in.h /usr/include/bits/byteswap.h hook.h packet.h
cuboid.o: position.h level.h /usr/include/pthread.h /usr/include/sched.h
cuboid.o: /usr/include/bits/sched.h /usr/include/signal.h
cuboid.o: /usr/include/bits/setjmp.h physics.h player.h colour.h
faultgen.o: /usr/include/stdlib.h /usr/include/features.h
faultgen.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
faultgen.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
faultgen.o: /usr/include/sys/types.h /usr/include/bits/types.h
faultgen.o: /usr/include/bits/typesizes.h /usr/include/time.h
faultgen.o: /usr/include/bits/time.h /usr/include/endian.h
faultgen.o: /usr/include/bits/endian.h /usr/include/sys/select.h
faultgen.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
faultgen.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
faultgen.o: /usr/include/alloca.h /usr/include/math.h
faultgen.o: /usr/include/bits/huge_val.h /usr/include/bits/mathdef.h
faultgen.o: /usr/include/bits/mathcalls.h faultgen.h mcc.h
faultgen.o: /usr/include/stdio.h /usr/include/libio.h
faultgen.o: /usr/include/_G_config.h /usr/include/wchar.h
faultgen.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
filter.o: /usr/include/stdlib.h /usr/include/features.h
filter.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
filter.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
filter.o: /usr/include/sys/types.h /usr/include/bits/types.h
filter.o: /usr/include/bits/typesizes.h /usr/include/time.h
filter.o: /usr/include/bits/time.h /usr/include/endian.h
filter.o: /usr/include/bits/endian.h /usr/include/sys/select.h
filter.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
filter.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
filter.o: /usr/include/alloca.h /usr/include/math.h
filter.o: /usr/include/bits/huge_val.h /usr/include/bits/mathdef.h
filter.o: /usr/include/bits/mathcalls.h filter.h mcc.h /usr/include/stdio.h
filter.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
filter.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
hook.o: /usr/include/stdio.h /usr/include/features.h /usr/include/sys/cdefs.h
hook.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
hook.o: /usr/include/gnu/stubs-32.h /usr/include/bits/types.h
hook.o: /usr/include/bits/typesizes.h /usr/include/libio.h
hook.o: /usr/include/_G_config.h /usr/include/wchar.h
hook.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h hook.h
hook.o: list.h /usr/include/stdlib.h /usr/include/sys/types.h
hook.o: /usr/include/time.h /usr/include/bits/time.h /usr/include/endian.h
hook.o: /usr/include/bits/endian.h /usr/include/sys/select.h
hook.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
hook.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
hook.o: /usr/include/alloca.h mcc.h
landscape.o: /usr/include/stdint.h /usr/include/features.h
landscape.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
landscape.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
landscape.o: /usr/include/bits/wchar.h block.h /usr/include/limits.h
landscape.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
landscape.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
landscape.o: bitstuff.h list.h /usr/include/stdlib.h /usr/include/sys/types.h
landscape.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
landscape.o: /usr/include/time.h /usr/include/bits/time.h
landscape.o: /usr/include/endian.h /usr/include/bits/endian.h
landscape.o: /usr/include/sys/select.h /usr/include/bits/select.h
landscape.o: /usr/include/bits/sigset.h /usr/include/sys/sysmacros.h
landscape.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h mcc.h
landscape.o: /usr/include/stdio.h /usr/include/libio.h
landscape.o: /usr/include/_G_config.h /usr/include/wchar.h
landscape.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
landscape.o: rank.h landscape.h perlin.h
level.o: /usr/include/stdio.h /usr/include/features.h
level.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
level.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
level.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
level.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
level.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
level.o: /usr/include/stdlib.h /usr/include/sys/types.h /usr/include/time.h
level.o: /usr/include/bits/time.h /usr/include/endian.h
level.o: /usr/include/bits/endian.h /usr/include/sys/select.h
level.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
level.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
level.o: /usr/include/alloca.h /usr/include/string.h /usr/include/sys/stat.h
level.o: /usr/include/bits/stat.h /usr/include/fcntl.h
level.o: /usr/include/bits/fcntl.h /usr/include/limits.h
level.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
level.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
level.o: /usr/include/zlib.h /usr/include/zconf.h /usr/include/zlibdefs.h
level.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
level.o: /usr/include/bits/confname.h /usr/include/getopt.h
level.o: /usr/include/pthread.h /usr/include/sched.h
level.o: /usr/include/bits/sched.h /usr/include/signal.h
level.o: /usr/include/bits/setjmp.h /usr/include/math.h
level.o: /usr/include/bits/huge_val.h /usr/include/bits/mathdef.h
level.o: /usr/include/bits/mathcalls.h filter.h level.h block.h
level.o: /usr/include/stdint.h /usr/include/bits/wchar.h bitstuff.h list.h
level.o: mcc.h rank.h physics.h position.h client.h /usr/include/netinet/in.h
level.o: /usr/include/sys/socket.h /usr/include/sys/uio.h
level.o: /usr/include/bits/uio.h /usr/include/bits/socket.h
level.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
level.o: /usr/include/asm/sockios.h /usr/include/bits/in.h
level.o: /usr/include/bits/byteswap.h hook.h packet.h cuboid.h faultgen.h
level.o: perlin.h player.h colour.h playerdb.h network.h undodb.h util.h
module.o: /usr/include/dlfcn.h /usr/include/features.h
module.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
module.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
module.o: /usr/include/bits/dlfcn.h /usr/include/stdio.h
module.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
module.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
module.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
module.o: /usr/include/string.h mcc.h /usr/include/time.h
module.o: /usr/include/bits/time.h module.h list.h /usr/include/stdlib.h
module.o: /usr/include/sys/types.h /usr/include/endian.h
module.o: /usr/include/bits/endian.h /usr/include/sys/select.h
module.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
module.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
module.o: /usr/include/alloca.h
network.o: /usr/include/stdio.h /usr/include/features.h
network.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
network.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
network.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
network.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
network.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
network.o: /usr/include/string.h /usr/include/sys/types.h /usr/include/time.h
network.o: /usr/include/bits/time.h /usr/include/endian.h
network.o: /usr/include/bits/endian.h /usr/include/sys/select.h
network.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
network.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
network.o: /usr/include/sys/socket.h /usr/include/sys/uio.h
network.o: /usr/include/bits/uio.h /usr/include/bits/socket.h
network.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
network.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
network.o: /usr/include/bits/posix2_lim.h /usr/include/bits/sockaddr.h
network.o: /usr/include/asm/socket.h /usr/include/asm/sockios.h
network.o: /usr/include/netdb.h /usr/include/netinet/in.h
network.o: /usr/include/stdint.h /usr/include/bits/wchar.h
network.o: /usr/include/bits/in.h /usr/include/bits/byteswap.h
network.o: /usr/include/rpc/netdb.h /usr/include/bits/netdb.h
network.o: /usr/include/netinet/tcp.h /usr/include/unistd.h
network.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
network.o: /usr/include/getopt.h /usr/include/fcntl.h
network.o: /usr/include/bits/fcntl.h /usr/include/errno.h
network.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
network.o: /usr/include/asm/errno.h /usr/include/asm-generic/errno.h
network.o: /usr/include/asm-generic/errno-base.h client.h hook.h list.h
network.o: /usr/include/stdlib.h /usr/include/alloca.h mcc.h packet.h
network.o: position.h level.h /usr/include/pthread.h /usr/include/sched.h
network.o: /usr/include/bits/sched.h /usr/include/signal.h
network.o: /usr/include/bits/setjmp.h block.h bitstuff.h rank.h physics.h
network.o: player.h colour.h network.h playerdb.h
packet.o: /usr/include/stdint.h /usr/include/features.h
packet.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
packet.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
packet.o: /usr/include/bits/wchar.h /usr/include/stdio.h
packet.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
packet.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
packet.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
packet.o: /usr/include/stdlib.h /usr/include/sys/types.h /usr/include/time.h
packet.o: /usr/include/bits/time.h /usr/include/endian.h
packet.o: /usr/include/bits/endian.h /usr/include/sys/select.h
packet.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
packet.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
packet.o: /usr/include/alloca.h /usr/include/string.h packet.h position.h
packet.o: network.h client.h /usr/include/netinet/in.h
packet.o: /usr/include/sys/socket.h /usr/include/sys/uio.h
packet.o: /usr/include/bits/uio.h /usr/include/bits/socket.h
packet.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
packet.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
packet.o: /usr/include/bits/posix2_lim.h /usr/include/bits/sockaddr.h
packet.o: /usr/include/asm/socket.h /usr/include/asm/sockios.h
packet.o: /usr/include/bits/in.h /usr/include/bits/byteswap.h hook.h list.h
packet.o: mcc.h commands.h player.h rank.h block.h bitstuff.h colour.h
packet.o: level.h /usr/include/pthread.h /usr/include/sched.h
packet.o: /usr/include/bits/sched.h /usr/include/signal.h
packet.o: /usr/include/bits/setjmp.h physics.h
perlin.o: /usr/include/stdlib.h /usr/include/features.h
perlin.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
perlin.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
perlin.o: /usr/include/sys/types.h /usr/include/bits/types.h
perlin.o: /usr/include/bits/typesizes.h /usr/include/time.h
perlin.o: /usr/include/bits/time.h /usr/include/endian.h
perlin.o: /usr/include/bits/endian.h /usr/include/sys/select.h
perlin.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
perlin.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
perlin.o: /usr/include/alloca.h /usr/include/math.h
perlin.o: /usr/include/bits/huge_val.h /usr/include/bits/mathdef.h
perlin.o: /usr/include/bits/mathcalls.h perlin.h mcc.h /usr/include/stdio.h
perlin.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
perlin.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
player.o: /usr/include/stdio.h /usr/include/features.h
player.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
player.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
player.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
player.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
player.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
player.o: /usr/include/string.h /usr/include/time.h /usr/include/bits/time.h
player.o: bitstuff.h /usr/include/stdint.h /usr/include/bits/wchar.h client.h
player.o: /usr/include/netinet/in.h /usr/include/sys/socket.h
player.o: /usr/include/sys/uio.h /usr/include/sys/types.h
player.o: /usr/include/endian.h /usr/include/bits/endian.h
player.o: /usr/include/sys/select.h /usr/include/bits/select.h
player.o: /usr/include/bits/sigset.h /usr/include/sys/sysmacros.h
player.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/uio.h
player.o: /usr/include/bits/socket.h /usr/include/limits.h
player.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
player.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
player.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
player.o: /usr/include/asm/sockios.h /usr/include/bits/in.h
player.o: /usr/include/bits/byteswap.h hook.h list.h /usr/include/stdlib.h
player.o: /usr/include/alloca.h mcc.h packet.h position.h level.h
player.o: /usr/include/pthread.h /usr/include/sched.h
player.o: /usr/include/bits/sched.h /usr/include/signal.h
player.o: /usr/include/bits/setjmp.h block.h rank.h physics.h player.h
player.o: colour.h playerdb.h network.h util.h
playerdb.o: /usr/include/stdio.h /usr/include/features.h
playerdb.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
playerdb.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
playerdb.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
playerdb.o: /usr/include/libio.h /usr/include/_G_config.h
playerdb.o: /usr/include/wchar.h /usr/include/bits/stdio_lim.h
playerdb.o: /usr/include/bits/sys_errlist.h /usr/include/time.h
playerdb.o: /usr/include/bits/time.h /usr/include/sqlite3.h mcc.h player.h
playerdb.o: /usr/include/stdint.h /usr/include/bits/wchar.h rank.h block.h
playerdb.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
playerdb.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
playerdb.o: /usr/include/bits/posix2_lim.h bitstuff.h list.h
playerdb.o: /usr/include/stdlib.h /usr/include/sys/types.h
playerdb.o: /usr/include/endian.h /usr/include/bits/endian.h
playerdb.o: /usr/include/sys/select.h /usr/include/bits/select.h
playerdb.o: /usr/include/bits/sigset.h /usr/include/sys/sysmacros.h
playerdb.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h colour.h
playerdb.o: position.h
queue.o: /usr/include/stdlib.h /usr/include/features.h
queue.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
queue.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
queue.o: /usr/include/sys/types.h /usr/include/bits/types.h
queue.o: /usr/include/bits/typesizes.h /usr/include/time.h
queue.o: /usr/include/bits/time.h /usr/include/endian.h
queue.o: /usr/include/bits/endian.h /usr/include/sys/select.h
queue.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
queue.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
queue.o: /usr/include/alloca.h /usr/include/pthread.h /usr/include/sched.h
queue.o: /usr/include/bits/sched.h /usr/include/signal.h
queue.o: /usr/include/bits/setjmp.h queue.h mcc.h /usr/include/stdio.h
queue.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
queue.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
timer.o: /usr/include/string.h /usr/include/features.h
timer.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
timer.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
timer.o: /usr/include/time.h /usr/include/bits/time.h
timer.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h list.h
timer.o: /usr/include/stdlib.h /usr/include/sys/types.h /usr/include/endian.h
timer.o: /usr/include/bits/endian.h /usr/include/sys/select.h
timer.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
timer.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
timer.o: /usr/include/alloca.h mcc.h /usr/include/stdio.h
timer.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
timer.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
timer.o: timer.h
undodb.o: /usr/include/stdio.h /usr/include/features.h
undodb.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
undodb.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
undodb.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
undodb.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
undodb.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
undodb.o: /usr/include/stdlib.h /usr/include/sys/types.h /usr/include/time.h
undodb.o: /usr/include/bits/time.h /usr/include/endian.h
undodb.o: /usr/include/bits/endian.h /usr/include/sys/select.h
undodb.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
undodb.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
undodb.o: /usr/include/alloca.h /usr/include/sqlite3.h mcc.h undodb.h
undodb.o: /usr/include/stdint.h /usr/include/bits/wchar.h playerdb.h util.h
mcc.o: /usr/include/stdio.h /usr/include/features.h /usr/include/sys/cdefs.h
mcc.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
mcc.o: /usr/include/gnu/stubs-32.h /usr/include/bits/types.h
mcc.o: /usr/include/bits/typesizes.h /usr/include/libio.h
mcc.o: /usr/include/_G_config.h /usr/include/wchar.h
mcc.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
mcc.o: /usr/include/time.h /usr/include/bits/time.h /usr/include/unistd.h
mcc.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
mcc.o: /usr/include/getopt.h /usr/include/signal.h /usr/include/bits/sigset.h
mcc.o: block.h /usr/include/stdint.h /usr/include/bits/wchar.h
mcc.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
mcc.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
mcc.o: /usr/include/bits/posix2_lim.h bitstuff.h list.h /usr/include/stdlib.h
mcc.o: /usr/include/sys/types.h /usr/include/endian.h
mcc.o: /usr/include/bits/endian.h /usr/include/sys/select.h
mcc.o: /usr/include/bits/select.h /usr/include/sys/sysmacros.h
mcc.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h mcc.h rank.h
mcc.o: level.h /usr/include/pthread.h /usr/include/sched.h
mcc.o: /usr/include/bits/sched.h /usr/include/bits/setjmp.h
mcc.o: /usr/include/string.h physics.h position.h network.h player.h colour.h
mcc.o: playerdb.h client.h /usr/include/netinet/in.h
mcc.o: /usr/include/sys/socket.h /usr/include/sys/uio.h
mcc.o: /usr/include/bits/uio.h /usr/include/bits/socket.h
mcc.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
mcc.o: /usr/include/asm/sockios.h /usr/include/bits/in.h
mcc.o: /usr/include/bits/byteswap.h hook.h packet.h timer.h
spleef.o: block.h /usr/include/stdint.h /usr/include/features.h
spleef.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
spleef.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
spleef.o: /usr/include/bits/wchar.h /usr/include/limits.h
spleef.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
spleef.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
spleef.o: bitstuff.h list.h /usr/include/stdlib.h /usr/include/sys/types.h
spleef.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
spleef.o: /usr/include/time.h /usr/include/bits/time.h /usr/include/endian.h
spleef.o: /usr/include/bits/endian.h /usr/include/sys/select.h
spleef.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
spleef.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
spleef.o: /usr/include/alloca.h mcc.h /usr/include/stdio.h
spleef.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
spleef.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
spleef.o: rank.h level.h /usr/include/pthread.h /usr/include/sched.h
spleef.o: /usr/include/bits/sched.h /usr/include/signal.h
spleef.o: /usr/include/bits/setjmp.h /usr/include/string.h physics.h
spleef.o: position.h
wireworld.o: level.h /usr/include/pthread.h /usr/include/features.h
wireworld.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
wireworld.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
wireworld.o: /usr/include/endian.h /usr/include/bits/endian.h
wireworld.o: /usr/include/sched.h /usr/include/bits/types.h
wireworld.o: /usr/include/bits/typesizes.h /usr/include/time.h
wireworld.o: /usr/include/bits/time.h /usr/include/bits/sched.h
wireworld.o: /usr/include/signal.h /usr/include/bits/sigset.h
wireworld.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/setjmp.h
wireworld.o: /usr/include/string.h block.h /usr/include/stdint.h
wireworld.o: /usr/include/bits/wchar.h /usr/include/limits.h
wireworld.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
wireworld.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
wireworld.o: bitstuff.h list.h /usr/include/stdlib.h /usr/include/sys/types.h
wireworld.o: /usr/include/sys/select.h /usr/include/bits/select.h
wireworld.o: /usr/include/sys/sysmacros.h /usr/include/alloca.h mcc.h
wireworld.o: /usr/include/stdio.h /usr/include/libio.h
wireworld.o: /usr/include/_G_config.h /usr/include/wchar.h
wireworld.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
wireworld.o: rank.h physics.h position.h
tnt.o: level.h /usr/include/pthread.h /usr/include/features.h
tnt.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
tnt.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
tnt.o: /usr/include/endian.h /usr/include/bits/endian.h /usr/include/sched.h
tnt.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
tnt.o: /usr/include/time.h /usr/include/bits/time.h /usr/include/bits/sched.h
tnt.o: /usr/include/signal.h /usr/include/bits/sigset.h
tnt.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/setjmp.h
tnt.o: /usr/include/string.h block.h /usr/include/stdint.h
tnt.o: /usr/include/bits/wchar.h /usr/include/limits.h
tnt.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
tnt.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h bitstuff.h
tnt.o: list.h /usr/include/stdlib.h /usr/include/sys/types.h
tnt.o: /usr/include/sys/select.h /usr/include/bits/select.h
tnt.o: /usr/include/sys/sysmacros.h /usr/include/alloca.h mcc.h
tnt.o: /usr/include/stdio.h /usr/include/libio.h /usr/include/_G_config.h
tnt.o: /usr/include/wchar.h /usr/include/bits/stdio_lim.h
tnt.o: /usr/include/bits/sys_errlist.h rank.h physics.h position.h
heartbeat.o: /usr/include/stdio.h /usr/include/features.h
heartbeat.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
heartbeat.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
heartbeat.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
heartbeat.o: /usr/include/libio.h /usr/include/_G_config.h
heartbeat.o: /usr/include/wchar.h /usr/include/bits/stdio_lim.h
heartbeat.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
heartbeat.o: /usr/include/sys/types.h /usr/include/time.h
heartbeat.o: /usr/include/bits/time.h /usr/include/endian.h
heartbeat.o: /usr/include/bits/endian.h /usr/include/sys/select.h
heartbeat.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
heartbeat.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
heartbeat.o: /usr/include/alloca.h /usr/include/string.h
heartbeat.o: /usr/include/sys/socket.h /usr/include/sys/uio.h
heartbeat.o: /usr/include/bits/uio.h /usr/include/bits/socket.h
heartbeat.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
heartbeat.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
heartbeat.o: /usr/include/bits/posix2_lim.h /usr/include/bits/sockaddr.h
heartbeat.o: /usr/include/asm/socket.h /usr/include/asm/sockios.h
heartbeat.o: /usr/include/netdb.h /usr/include/netinet/in.h
heartbeat.o: /usr/include/stdint.h /usr/include/bits/wchar.h
heartbeat.o: /usr/include/bits/in.h /usr/include/bits/byteswap.h
heartbeat.o: /usr/include/rpc/netdb.h /usr/include/bits/netdb.h
heartbeat.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
heartbeat.o: /usr/include/bits/confname.h /usr/include/getopt.h
heartbeat.o: /usr/include/errno.h /usr/include/bits/errno.h
heartbeat.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
heartbeat.o: /usr/include/asm-generic/errno.h
heartbeat.o: /usr/include/asm-generic/errno-base.h mcc.h network.h timer.h
irc.o: /usr/include/stdio.h /usr/include/features.h /usr/include/sys/cdefs.h
irc.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
irc.o: /usr/include/gnu/stubs-32.h /usr/include/bits/types.h
irc.o: /usr/include/bits/typesizes.h /usr/include/libio.h
irc.o: /usr/include/_G_config.h /usr/include/wchar.h
irc.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
irc.o: /usr/include/string.h /usr/include/stdlib.h /usr/include/sys/types.h
irc.o: /usr/include/time.h /usr/include/bits/time.h /usr/include/endian.h
irc.o: /usr/include/bits/endian.h /usr/include/sys/select.h
irc.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
irc.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
irc.o: /usr/include/alloca.h /usr/include/sys/socket.h /usr/include/sys/uio.h
irc.o: /usr/include/bits/uio.h /usr/include/bits/socket.h
irc.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
irc.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
irc.o: /usr/include/bits/posix2_lim.h /usr/include/bits/sockaddr.h
irc.o: /usr/include/asm/socket.h /usr/include/asm/sockios.h
irc.o: /usr/include/netdb.h /usr/include/netinet/in.h /usr/include/stdint.h
irc.o: /usr/include/bits/wchar.h /usr/include/bits/in.h
irc.o: /usr/include/bits/byteswap.h /usr/include/rpc/netdb.h
irc.o: /usr/include/bits/netdb.h /usr/include/unistd.h
irc.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
irc.o: /usr/include/getopt.h /usr/include/errno.h /usr/include/bits/errno.h
irc.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
irc.o: /usr/include/asm-generic/errno.h /usr/include/asm-generic/errno-base.h
irc.o: colour.h commands.h hook.h mcc.h network.h timer.h
airlayer.o: level.h /usr/include/pthread.h /usr/include/features.h
airlayer.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
airlayer.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
airlayer.o: /usr/include/endian.h /usr/include/bits/endian.h
airlayer.o: /usr/include/sched.h /usr/include/bits/types.h
airlayer.o: /usr/include/bits/typesizes.h /usr/include/time.h
airlayer.o: /usr/include/bits/time.h /usr/include/bits/sched.h
airlayer.o: /usr/include/signal.h /usr/include/bits/sigset.h
airlayer.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/setjmp.h
airlayer.o: /usr/include/string.h block.h /usr/include/stdint.h
airlayer.o: /usr/include/bits/wchar.h /usr/include/limits.h
airlayer.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
airlayer.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
airlayer.o: bitstuff.h list.h /usr/include/stdlib.h /usr/include/sys/types.h
airlayer.o: /usr/include/sys/select.h /usr/include/bits/select.h
airlayer.o: /usr/include/sys/sysmacros.h /usr/include/alloca.h mcc.h
airlayer.o: /usr/include/stdio.h /usr/include/libio.h
airlayer.o: /usr/include/_G_config.h /usr/include/wchar.h
airlayer.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
airlayer.o: rank.h physics.h position.h
ipc.o: /usr/include/stdio.h /usr/include/features.h /usr/include/sys/cdefs.h
ipc.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
ipc.o: /usr/include/gnu/stubs-32.h /usr/include/bits/types.h
ipc.o: /usr/include/bits/typesizes.h /usr/include/libio.h
ipc.o: /usr/include/_G_config.h /usr/include/wchar.h
ipc.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
ipc.o: /usr/include/stdlib.h /usr/include/sys/types.h /usr/include/time.h
ipc.o: /usr/include/bits/time.h /usr/include/endian.h
ipc.o: /usr/include/bits/endian.h /usr/include/sys/select.h
ipc.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
ipc.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
ipc.o: /usr/include/alloca.h /usr/include/sys/socket.h /usr/include/sys/uio.h
ipc.o: /usr/include/bits/uio.h /usr/include/bits/socket.h
ipc.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
ipc.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
ipc.o: /usr/include/bits/posix2_lim.h /usr/include/bits/sockaddr.h
ipc.o: /usr/include/asm/socket.h /usr/include/asm/sockios.h
ipc.o: /usr/include/sys/un.h /usr/include/string.h /usr/include/unistd.h
ipc.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
ipc.o: /usr/include/getopt.h mcc.h network.h
setrank.o: mcc.h /usr/include/stdio.h /usr/include/features.h
setrank.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
setrank.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
setrank.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
setrank.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
setrank.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
setrank.o: /usr/include/time.h /usr/include/bits/time.h player.h
setrank.o: /usr/include/stdint.h /usr/include/bits/wchar.h rank.h block.h
setrank.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
setrank.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
setrank.o: /usr/include/bits/posix2_lim.h bitstuff.h list.h
setrank.o: /usr/include/stdlib.h /usr/include/sys/types.h
setrank.o: /usr/include/endian.h /usr/include/bits/endian.h
setrank.o: /usr/include/sys/select.h /usr/include/bits/select.h
setrank.o: /usr/include/bits/sigset.h /usr/include/sys/sysmacros.h
setrank.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h colour.h
setrank.o: position.h playerdb.h /usr/include/string.h util.h
banip.o: mcc.h /usr/include/stdio.h /usr/include/features.h
banip.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
banip.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
banip.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
banip.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
banip.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
banip.o: /usr/include/time.h /usr/include/bits/time.h playerdb.h
banip.o: /usr/include/string.h
portal.o: /usr/include/stdio.h /usr/include/features.h
portal.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
portal.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
portal.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
portal.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
portal.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
portal.o: /usr/include/string.h bitstuff.h /usr/include/stdint.h
portal.o: /usr/include/bits/wchar.h block.h /usr/include/limits.h
portal.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
portal.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h list.h
portal.o: /usr/include/stdlib.h /usr/include/sys/types.h /usr/include/time.h
portal.o: /usr/include/bits/time.h /usr/include/endian.h
portal.o: /usr/include/bits/endian.h /usr/include/sys/select.h
portal.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
portal.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
portal.o: /usr/include/alloca.h mcc.h rank.h colour.h client.h
portal.o: /usr/include/netinet/in.h /usr/include/sys/socket.h
portal.o: /usr/include/sys/uio.h /usr/include/bits/uio.h
portal.o: /usr/include/bits/socket.h /usr/include/bits/sockaddr.h
portal.o: /usr/include/asm/socket.h /usr/include/asm/sockios.h
portal.o: /usr/include/bits/in.h /usr/include/bits/byteswap.h hook.h packet.h
portal.o: position.h level.h /usr/include/pthread.h /usr/include/sched.h
portal.o: /usr/include/bits/sched.h /usr/include/signal.h
portal.o: /usr/include/bits/setjmp.h physics.h player.h
doors.o: block.h /usr/include/stdint.h /usr/include/features.h
doors.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
doors.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
doors.o: /usr/include/bits/wchar.h /usr/include/limits.h
doors.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
doors.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
doors.o: bitstuff.h list.h /usr/include/stdlib.h /usr/include/sys/types.h
doors.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
doors.o: /usr/include/time.h /usr/include/bits/time.h /usr/include/endian.h
doors.o: /usr/include/bits/endian.h /usr/include/sys/select.h
doors.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
doors.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
doors.o: /usr/include/alloca.h mcc.h /usr/include/stdio.h
doors.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
doors.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h rank.h
render.o: /usr/include/png.h /usr/include/zlib.h /usr/include/zconf.h
render.o: /usr/include/zlibdefs.h /usr/include/sys/types.h
render.o: /usr/include/features.h /usr/include/sys/cdefs.h
render.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
render.o: /usr/include/gnu/stubs-32.h /usr/include/bits/types.h
render.o: /usr/include/bits/typesizes.h /usr/include/time.h
render.o: /usr/include/bits/time.h /usr/include/endian.h
render.o: /usr/include/bits/endian.h /usr/include/sys/select.h
render.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
render.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
render.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
render.o: /usr/include/bits/confname.h /usr/include/getopt.h
render.o: /usr/include/pngconf.h /usr/include/stdio.h /usr/include/libio.h
render.o: /usr/include/_G_config.h /usr/include/wchar.h
render.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
render.o: /usr/include/setjmp.h /usr/include/bits/setjmp.h
render.o: /usr/include/string.h block.h /usr/include/stdint.h
render.o: /usr/include/bits/wchar.h /usr/include/limits.h
render.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
render.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
render.o: bitstuff.h list.h /usr/include/stdlib.h /usr/include/alloca.h mcc.h
render.o: rank.h level.h /usr/include/pthread.h /usr/include/sched.h
render.o: /usr/include/bits/sched.h /usr/include/signal.h physics.h
render.o: position.h util.h
render_chunked.o: /usr/include/png.h /usr/include/zlib.h /usr/include/zconf.h
render_chunked.o: /usr/include/zlibdefs.h /usr/include/sys/types.h
render_chunked.o: /usr/include/features.h /usr/include/sys/cdefs.h
render_chunked.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
render_chunked.o: /usr/include/gnu/stubs-32.h /usr/include/bits/types.h
render_chunked.o: /usr/include/bits/typesizes.h /usr/include/time.h
render_chunked.o: /usr/include/bits/time.h /usr/include/endian.h
render_chunked.o: /usr/include/bits/endian.h /usr/include/sys/select.h
render_chunked.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
render_chunked.o: /usr/include/sys/sysmacros.h
render_chunked.o: /usr/include/bits/pthreadtypes.h /usr/include/unistd.h
render_chunked.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
render_chunked.o: /usr/include/getopt.h /usr/include/pngconf.h
render_chunked.o: /usr/include/stdio.h /usr/include/libio.h
render_chunked.o: /usr/include/_G_config.h /usr/include/wchar.h
render_chunked.o: /usr/include/bits/stdio_lim.h
render_chunked.o: /usr/include/bits/sys_errlist.h /usr/include/setjmp.h
render_chunked.o: /usr/include/bits/setjmp.h /usr/include/string.h block.h
render_chunked.o: /usr/include/stdint.h /usr/include/bits/wchar.h
render_chunked.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
render_chunked.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
render_chunked.o: /usr/include/bits/posix2_lim.h bitstuff.h list.h
render_chunked.o: /usr/include/stdlib.h /usr/include/alloca.h mcc.h rank.h
render_chunked.o: chunk.h chunked_level.h /usr/include/pthread.h
render_chunked.o: /usr/include/sched.h /usr/include/bits/sched.h
render_chunked.o: /usr/include/signal.h landscape.h queue.h util.h
