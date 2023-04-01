TARGET = urler
OBJS = urler.o
LDLIBS = -lcurl
CFLAGS := $(CFLAGS) -W -Wall -pedantic -g
MANUAL = urler.1

PREFIX ?= /usr/local
BINDIR ?= $(DESTDIR)$(PREFIX)/bin
MANDIR ?= $(DESTDIR)$(PREFIX)/share/man/man1

$(TARGET): $(OBJS)

install:
	install -d $(BINDIR)
	install -m 0755 $(TARGET) $(BINDIR)
	install -d $(MANDIR)
	install -m 0744 $(MANUAL) $(MANDIR)

clean:
	rm -f $(OBJS) $(TARGET)

test:
	@perl test.pl
