# Cyan Solitaire Makefile (Subdirectory Optimized)
CC      ?= gcc
CFLAGS  ?= -O2 -pipe
CFLAGS  += -Wall -Wextra $(shell pkg-config --cflags gtk4 glib-2.0)
LIBS    = $(shell pkg-config --libs gtk4 glib-2.0)

# Paths
SRCDIR  = src
TARGET  = cyansolitaire
PREFIX  ?= /usr/local
BINDIR  = $(PREFIX)/bin
DATADIR = $(PREFIX)/share

# Source and Objects
SRCS    = $(SRCDIR)/main.c $(SRCDIR)/logic.c
OBJS    = main.o logic.o

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

# Compile objects from src/ but keep .o files in root (or an obj/ dir)
%.o: $(SRCDIR)/%.c $(SRCDIR)/solitaire.h
	$(CC) $(CFLAGS) -I$(SRCDIR) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -d $(DESTDIR)$(DATADIR)/applications
	install -m 644 cyan-solitaire.desktop $(DESTDIR)$(DATADIR)/applications/

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(DATADIR)/applications/cyan-solitaire.desktop