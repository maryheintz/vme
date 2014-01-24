CC:=cc
CFLAGS=


#CC:=gcc
#CFLAGS=-Wall

LD:=ld
LDFLAGS=

TARGETS=vmivme_endian

# Install paths
prefix:=
exec_prefix:=$(prefix)/usr
bindir:=$(exec_prefix)/bin

.PHONY: all clean install uninstall

all: $(TARGETS)

vmivme_endian: vmivme_endian.o

vmivme_endian.o: vmivme_endian.c Makefile

clean:
	find . -name "*\.o" | xargs rm -f
	rm -f $(TARGETS)

install: all
	install -m0755 -oroot -groot $(TARGETS) $(bindir)/

uninstall: clean










