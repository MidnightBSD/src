#!/bin/sh
# $FreeBSD: release/7.0.0/share/examples/startslip/slip.sh 137882 2004-11-19 03:51:12Z obrien $
startslip -b 57600 -U ./slup.sh -D ./sldown.sh \
	-s atd<phone1> -s atd<phone2> -s atd<phone3> \
	-h -t 60 -w 2 -W 20 /dev/cuad1 <login> <password>
