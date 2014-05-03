#!/usr/bin/make -f

# Installation options
INSTALL=/usr/bin/install
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
BINPREFIX=

# Compiler options
CC=gcc
CFLAGS=-Wall -Werror -c -g
LDFLAGS=

# Source file details
SOURCES=mu0.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=mu0

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

install: $(EXECUTABLE)
	$(INSTALL) $(EXECUTABLE) $(BINDIR)/$(BINPREFIX)$(EXECUTABLE)

uninstall:
	rm -f $(BINDIR)/$(BINPREFIX)$(EXECUTABLE)
