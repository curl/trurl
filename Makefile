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
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -m 0755 $(TARGET) $(BINDIR)
	$(INSTALL) -d $(MANDIR)
	$(INSTALL) -m 0644 $(MANUAL) $(MANDIR)

clean:
	rm -f $(OBJS) $(TARGET)

test:
	@perl test.pl
	@perl test-json.pl
