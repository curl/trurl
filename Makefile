TARGET = trurl
OBJS = trurl.o
LDLIBS = -lcurl
CFLAGS := $(CFLAGS) -W -Wall -pedantic -g
MANUAL = trurl.1

PREFIX ?= /usr/local
BINDIR ?= $(DESTDIR)$(PREFIX)/bin
MANDIR ?= $(DESTDIR)$(PREFIX)/share/man/man1

$(TARGET): $(OBJS)

trurl.o:trurl.c version.h

install:
	install -d $(BINDIR)
	install -m 0755 $(TARGET) $(BINDIR)
	install -d $(MANDIR)
	install -m 0744 $(MANUAL) $(MANDIR)

clean:
	rm -f $(OBJS) $(TARGET)

test:
	@perl test.pl
