.POSIX:
.SUFFIXES:

CC:=gcc
CFLAGS:=-Wall -Wextra
DEBUGCFLAGS:=-Werror -pedantic -g -DDEBUG

all: ; @echo -e "This makefile uses 'debug' and 'release' targets, but \n\
intentionally excludes all. Please make release or make debug."

debug: CFLAGS += $(DEBUGCFLAGS)
debug: release

release: naive_malloc.o

naive_malloc.o: naive_malloc.c
	$(CC) $(CFLAGS) $? -o $@

#https://blog.jgc.org/2015/04/the-one-line-you-should-add-to-every.html
#Example use: make print-CC
#To print the value of the CC variable
print-%: ; @echo $*=$($*)
	
clean:
	rm -f naive_malloc.o
