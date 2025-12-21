# ST7735驱动Makefile
CC = gcc
CFLAGS = -Wall -O2 -g
LIBS = -lm
TARGET = st7735_demo
SRCS = st7735.c main.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

st7735.o: st7735.c st7735.h
	$(CC) $(CFLAGS) -c st7735.c -o st7735.o

main.o: main.c st7735.h
	$(CC) $(CFLAGS) -c main.c -o main.o

clean:
	rm -f $(OBJS) $(TARGET)

install:
	sudo cp $(TARGET) /usr/local/bin/

.PHONY: all clean install