#
# Copyright (c) 1998 Robert Nordier
# All rights reserved.
#
# Redistribution and use in source and binary forms are freely
# permitted provided that the above copyright notice and this
# paragraph and the following disclaimer are duplicated in all
# such forms.
#
# This software is provided "AS IS" and without any express or
# implied warranties, including, without limitation, the implied
# warranties of merchantability and fitness for a particular
# purpose.
#

# $FreeBSD: release/7.0.0/sys/boot/pc98/btx/lib/btxsys.s 50477 1999-08-28 01:08:13Z peter $

#
# BTX system calls.
#

#
# Globals.
#
		.global __exit
		.global __exec
#
# Constants.
#
		.set INT_SYS,0x30		# Interrupt number
#
# System call: exit
#
__exit: 	xorl %eax,%eax			# BTX system
		int $INT_SYS			#  call 0x0
#
# System call: exec
#
__exec: 	movl $0x1,%eax			# BTX system
		int $INT_SYS			#  call 0x1
