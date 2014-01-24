
CC:=gcc
CFLAGS=-Wall

LD:=ld
LDFLAGS=

TARGETS=interface_test nvram_test

.PHONY: all clean install uninstall

# Install paths
prefix:=
exec_prefix:=$(prefix)/usr
bindir:=$(exec_prefix)/bin

all: $(TARGETS)

interface_test: interface_test.o

interface_test.o: interface_test.c Makefile

nvram_test: nvram_test.o

nvram_test.o: nvram_test.c Makefile

clean:
	find . -name "*\.o" | xargs rm -f
	rm -f $(TARGETS)

install: all
	install -m0755 -oroot -groot -D $(TARGETS) $(bindir)/

uninstall: clean
