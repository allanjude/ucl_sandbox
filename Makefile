# Debugging on
CFLAGS=-g -O0
CFLAGS+=-Wall
CFLAGS+=`pkg-config --cflags libucl`
LIBS+=`pkg-config --libs libucl`
PREFIX?=/usr/local
SRCS=ucl_cap.c
OBJS=$(SRCS:.c=.o)
EXECUTABLE=uclcap

all: $(SRCS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -o $(EXECUTABLE) $(OBJS)

clean:
	rm -f *.o $(EXECUTABLE)

install: $(EXECUTABLE)
	$(INSTALL) -m0755 $(EXECUTABLE) $(DESTDIR)$(PREFIX)/bin/$(EXECUTABLE)
