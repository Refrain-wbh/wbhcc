CFLAGS=-std=c11 -g -static -fno-common
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
CC=gcc

wcc: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(OBJS): wcc.h

test: wcc
	./test.sh

clean:
	rm -f wcc *.o *~ tmp* *.S *.s *.txt

.PHONY: test clean
