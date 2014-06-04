CC = gcc
CFLAGS = -g
LDFLAGS = -lm
BINS = iobench
DOCOPT = python ../docopt.c/docopt_c.py

all: $(BINS)

iobench: iobench.o 
	$(CC) $^ -o $@ $(LDFLAGS)

iobench.o: iobench.c docopt.c

docopt.c: iobench.docopt
	$(DOCOPT) $< -o $@ 
