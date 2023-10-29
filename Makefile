CC = gcc
CFLAGS = -c -Wall -Werror -pedantic -std=gnu18
LOGIN = surendra
SUBMITPATH = ~cs537-1/handin/$(LOGIN)/P3

all: wsh

wsh: wsh.o
	$(CC) $< -o $@

wsh.o: wsh.c
	$(CC) $(CFLAGS) wsh.c -o $@

clean:
	rm -rf *.o

run: wsh
	wsh

pack: wsh.h wsh.c Makefile README.md slipdays.txt
	tar cfz $(LOGIN).tar.gz $^

submit: pack
	cp $(LOGIN).tar.gz $(SUBMITPATH)