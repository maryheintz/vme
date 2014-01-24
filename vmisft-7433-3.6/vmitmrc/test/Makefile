
CC:=gcc
CFLAGS=-Wall

LD:=ld
LDFLAGS=

TARGETS=tmr_interval_test tmr_dropout_test tmr_read_test tmr_example tmr_access

# Install paths
prefix:=
exec_prefix:=$(prefix)/usr
bindir:=$(exec_prefix)/bin

.PHONY: all clean install uninstall

all: $(TARGETS)

tmr_interval_test: tmr_interval_test.o

tmr_interval_test.o: tmr_interval_test.c Makefile

tmr_dropout_test: tmr_dropout_test.o

tmr_dropout_test.o: tmr_dropout_test.c Makefile

tmr_read_test: tmr_read_test.o

tmr_read_test.o: tmr_read_test.c Makefile

tmr_example: tmr_example.o

tmr_example.o: tmr_example.c Makefile

tmr_access: tmr_access.o

tmr_access.o: tmr_access.c Makefile

clean:
	find . -name "*\.o" | xargs rm -f
	rm $(TARGETS)

install: all
	install -m0755 -oroot -groot -D $(TARGETS) $(bindir)/

uninstall: clean
