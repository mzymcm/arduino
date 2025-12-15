CC = gcc
CFLAGS = -Wall -O2 -I/usr/local/include
LIBS = -L/usr/local/lib -lbcm2835 -lm

all: test_st7735

st7735.o: st7735.c st7735.h
	$(CC) $(CFLAGS) -c st7735.c

test_st7735: test_st7735.c st7735.o
	$(CC) $(CFLAGS) -o test_st7735 test_st7735.c st7735.o $(LIBS)

clean:
	rm -f *.o test_st7735

install:
	sudo cp test_st7735 /usr/local/bin/

.PHONY: all clean install
