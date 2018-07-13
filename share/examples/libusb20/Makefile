# $MidnightBSD$
# $FreeBSD: stable/10/share/examples/libusb20/Makefile 269879 2014-08-12 14:53:02Z emaste $
TARGETS=	bulk control
CFLAGS+=	-Wall

all: $(TARGETS)

bulk: bulk.o util.o
	$(CC) $(CFLAGS) -o bulk bulk.o util.o -lusb

control: control.o util.o
	$(CC) $(CFLAGS) -o control control.o util.o -lusb

clean:
	rm -f $(TARGETS) *.o *~
