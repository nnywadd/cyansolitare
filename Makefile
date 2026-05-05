# Cyan Solitaire Pro - Build Configuration

CC      ?= gcc
# Optimized for performance; -march=native leverages local CPU architecture
CFLAGS  ?= -O2 -march=native -pipe

# Professional Warning Flags for strict C compliance
CFLAGS  += -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wstrict-prototypes -Wmissing-prototypes
CFLAGS  += $(shell pkg-config --cflags gtk4 glib-2.0)

# Linker Flags (Explicitly linking math library for fabs/easing)
LDFLAGS += -lm
LIBS    = $(shell pkg-config --libs gtk4 glib-2.0)

# Directories
SRCDIR  = src
OBJDIR  = obj
BINDIR  = bin
TARGET  = $(BINDIR)/cyansolitaire

# Install Paths
PREFIX  ?= /usr/local
INSTALL_BIN = $(PREFIX)/bin
INSTALL_DAT = $(PREFIX)/share

# Source and Objects
SRCS    = $(wildcard $(SRCDIR)/*.c)
OBJS    = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

.PHONY: all clean install uninstall debug prep

all: prep $(TARGET)

prep:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(BINDIR)

# Dedicated debug target for GDB/Valgrind tracing
debug: CFLAGS += -g -O0 -DDEBUG
debug: prep $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/solitaire.h
	$(CC) $(CFLAGS) -I$(SRCDIR) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(BINDIR)

install: all
	install -d $(DESTDIR)$(INSTALL_BIN)
	install -m 755 $(TARGET) $(DESTDIR)$(INSTALL_BIN)/cyansolitaire
	install -d $(DESTDIR)$(INSTALL_DAT)/applications
	install -m 644 cyan-solitaire.desktop $(DESTDIR)$(INSTALL_DAT)/applications/

uninstall:
	rm -f $(DESTDIR)$(INSTALL_BIN)/cyansolitaire
	rm -f $(DESTDIR)$(INSTALL_DAT)/applications/cyan-solitaire.desktop