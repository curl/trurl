TARGET = trurl
OBJS = trurl.o
LDLIBS = -lcurl
CFLAGS := $(CFLAGS) -W -Wall -pedantic -g
MANUAL = trurl.1

PREFIX ?= /usr/local
BINDIR ?= $(DESTDIR)$(PREFIX)/bin
MANDIR ?= $(DESTDIR)$(PREFIX)/share/man/man1

INSTALL = install

$(TARGET): $(OBJS)

trurl.o:trurl.c version.h

install:
	[ -d $(BINDIR) ] || $(INSTALL) -d $(BINDIR)
	$(INSTALL) -m 0755 $(TARGET) $(BINDIR)
	[ -d $(MANDIR) ] || $(INSTALL) -d $(MANDIR)
	$(INSTALL) -m 0644 $(MANUAL) $(MANDIR)

uninstall:
	[ -f $(BINDIR)/$(TARGET) ] && rm $(BINDIR)/$(TARGET)
	[ -f $(MANDIR)/$(MANUAL) ] && rm $(MANDIR)/$(MANUAL)

clean:
	rm -f $(OBJS) $(TARGET)

test:
	@perl test.pl
