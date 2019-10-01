CC = gcc
CFLAGS= -O4 -Wall -Iinc
DEST = bin
LDFLAGS = -Llib
LIBS = -lusb
OBJS = main.o
PROGRAM = keychk

all:            $(PROGRAM)

$(PROGRAM):     $(OBJS)
		$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $(PROGRAM)

clean:
		rm -f *.o *~ $(PROGRAM) *.stackdump

install:        $(PROGRAM)
		install -s $(PROGRAM) $(DEST)
