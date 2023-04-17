TARGET = trurl
OBJS = trurl.o
LDLIBS != curl-config --libs
CFLAGS != curl-config --cflags
CFLAGS += -W -Wall -pedantic -g
MANUAL = trurl.1

PREFIX ?= /usr/local
BINDIR ?= $(DESTDIR)$(PREFIX)/bin
MANDIR ?= $(DESTDIR)$(PREFIX)/share/man/man1

INSTALL = install

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDLIBS) $(LDFLAGS)

trurl.o:trurl.c version.h

install:
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -m 0755 $(TARGET) $(BINDIR)
	$(INSTALL) -d $(MANDIR)
	$(INSTALL) -m 0644 $(MANUAL) $(MANDIR)

clean:
	rm -f $(OBJS) $(TARGET)

test:
	@perl test.pl

checksrc:
	./checksrc.pl trurl.c version.h
