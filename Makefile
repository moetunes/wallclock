CFLAGS+= -Wall
LDADD+= -lX11 
LDFLAGS=
EXEC=wallclock

PREFIX?= /usr
BINDIR?= $(PREFIX)/bin

CC=gcc

all: $(EXEC)

wallclock: wallclock.o
	$(CC) $(LDFLAGS) -s -O2 -ffast-math -fno-unit-at-a-time -o $@ $+ $(LDADD)

install: all
	install -Dm 755 wallclock $(DESTDIR)$(BINDIR)/wallclock

clean:
	rm -fv wallclock *.o
