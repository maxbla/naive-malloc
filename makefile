.POSIX:
# Eliminate default known suffixes
.SUFFIXES:

CC:=gcc
CFLAGS:=-Wall -Wextra -I. -L.
# This is a lot of flags. For an explaination, see
# https://stackoverflow.com/a/3376483/6677437
# https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
DEBUGCFLAGS:=-Werror -pedantic -g -Og -DDEBUG -Wfloat-equal -Wundef \
-Wshadow -Wpointer-arith -Wstrict-prototypes \
-Wwrite-strings -Wcast-qual -Wswitch-default -Wswitch-enum \
-Wconversion -Wstrict-overflow=3 -march=native
RELEASECFLAGS:=-O3

all: ; @echo -e "This makefile uses 'debug' and 'release' targets, but \
intentionally excludes all.\n Please make release or make debug."



# Target-specific variable value, see
# https://stackoverflow.com/questions/1079832/how-can-i-configure-my-makefile-for-debug-and-release-builds
debug: CFLAGS += $(DEBUGCFLAGS)
debug: test.o

release: CFLAGS += $(RELEASECFLAGS)
release: test.o

test.o: test.c naive_malloc.o
	$(CC) $(CFLAGS) $? -o $@

naive_malloc.o: naive_malloc.c
	$(CC) -c $(CFLAGS) $? -o $@

clean:
	rm -f test.o naive_malloc.o
