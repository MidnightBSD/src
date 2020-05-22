# $FreeBSD: stable/11/share/examples/libusb20/Makefile 299293 2016-05-10 00:51:50Z gjb $

PACKAGE=examples
FILESDIR=${SHAREDIR}/examples/libusb20
TARGETS=	bulk control
CFLAGS+=	-Wall

all: $(TARGETS)

bulk: bulk.o util.o
	$(CC) $(CFLAGS) -o bulk bulk.o util.o -lusb

control: control.o util.o
	$(CC) $(CFLAGS) -o control control.o util.o -lusb

clean:
	rm -f $(TARGETS) *.o *~
