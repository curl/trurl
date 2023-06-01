TARGET = trurl
OBJS = trurl.o
LDLIBS = $$(curl-config --libs)
CFLAGS += $$(curl-config --cflags) -W -Wall -Wshadow -Werror -pedantic -g -std=gnu99
MANUAL = trurl.1

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man/man1

INSTALL ?= install
PYTHON3 ?= python3

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDLIBS) $(LDFLAGS)

trurl.o:trurl.c version.h

.PHONY: install
install:
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(TARGET) $(DESTDIR)$(BINDIR)
	$(INSTALL) -d $(DESTDIR)$(MANDIR)
	$(INSTALL) -m 0644 $(MANUAL) $(DESTDIR)$(MANDIR)

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: test
test: $(TARGET)
	@$(PYTHON3) test.py

.PHONY: test-memory
test-memory: $(TARGET)
	@$(PYTHON3) test.py --with-valgrind

.PHONY: checksrc
checksrc:
	./checksrc.pl trurl.c version.h
