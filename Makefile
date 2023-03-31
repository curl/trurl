TARGET = urler
OBJS = urler.o
LDLIBS = -lcurl
CFLAGS := $(CFLAGS) -W -Wall -pedantic -g
MANUAL = urler.1

BINDIR ?= /usr/bin
MANDIR ?= /usr/share/man

$(TARGET): $(OBJS)

install:
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(TARGET) $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(MANDIR)/man1/
	install -m 0744 $(MANUAL) $(DESTDIR)$(MANDIR)/man1/

clean:
	rm -f $(OBJS) $(TARGET)
