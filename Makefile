CC := gcc

pgm2xm: pgm2xm.c
	$(CC) pgm2xm.c -o pgm2xm

all: pgm2xm

clean:
	rm -f pgm2xm

.PHONY: clean all
