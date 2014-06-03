CC = gcc
CFLAGS = -g
LDFLAGS =
BINS = iobench
DOCOPT = python ../docopt.c/docopt_c.py

all: $(BINS)

iobench: iobench.o 

iobench.o: iobench.c docopt.c

docopt.c: iobench.docopt
	$(DOCOPT) $< -o $@ 
