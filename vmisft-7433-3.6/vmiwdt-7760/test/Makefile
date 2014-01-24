
CC:=gcc
CFLAGS=-Wall

LD:=ld
LDFLAGS=

TARGETS=wd_test

# Install paths
prefix:=
exec_prefix:=$(prefix)/usr
bindir:=$(exec_prefix)/bin

.PHONY: all clean

all: $(TARGETS)

wd_test: wd_test.o

wd_test.o: wd_test.c Makefile

clean:
	find . -name "*\.o" | xargs rm -f
	rm -f $(TARGETS)

install: all
	install -m0755 -oroot -groot $(TARGETS) $(bindir)/

uninstall: clean
