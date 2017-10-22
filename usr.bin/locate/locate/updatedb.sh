#!/bin/sh
#
# Copyright (c) September 1995 Wolfram Schneider <wosch@FreeBSD.org>. Berlin.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# updatedb - update locate database for local mounted filesystems
#
# $FreeBSD: release/7.0.0/usr.bin/locate/locate/updatedb.sh 173633 2007-11-15 15:14:16Z se $

if [ "$(id -u)" = "0" ]; then
	echo ">>> WARNING" 1>&2
	echo ">>> Executing updatedb as root.  This WILL reveal all filenames" 1>&2
	echo ">>> on your machine to all login users, which is a security risk." 1>&2
fi
: ${LOCATE_CONFIG="/etc/locate.rc"}
if [ -f "$LOCATE_CONFIG" -a -r "$LOCATE_CONFIG" ]; then
       . $LOCATE_CONFIG
fi

# The directory containing locate subprograms
: ${LIBEXECDIR:=/usr/libexec}; export LIBEXECDIR
: ${TMPDIR:=/tmp}; export TMPDIR
if ! TMPDIR=`mktemp -d $TMPDIR/locateXXXXXXXXXX`; then
	exit 1
fi

PATH=$LIBEXECDIR:/bin:/usr/bin:$PATH; export PATH


: ${mklocatedb:=locate.mklocatedb}	 # make locate database program
: ${FCODES:=/var/db/locate.database}	 # the database
: ${SEARCHPATHS:="/"}		# directories to be put in the database
: ${PRUNEPATHS:="/tmp /usr/tmp /var/tmp /var/db/portsnap"} # unwanted directories
: ${FILESYSTEMS:="ufs ext2fs zfs"}	 # allowed filesystems 
: ${find:=find}

case X"$SEARCHPATHS" in 
	X) echo "$0: empty variable SEARCHPATHS"; exit 1;; esac
case X"$FILESYSTEMS" in 
	X) echo "$0: empty variable FILESYSTEMS"; exit 1;; esac

# Make a list a paths to exclude in the locate run
excludes="! (" or=""
for fstype in $FILESYSTEMS
do
       excludes="$excludes $or -fstype $fstype"
       or="-or"
done
excludes="$excludes ) -prune"

case X"$PRUNEPATHS" in
	X) ;;
	*) for path in $PRUNEPATHS
           do 
		excludes="$excludes -or -path $path -prune"
	   done;;
esac

tmp=$TMPDIR/_updatedb$$
trap 'rm -f $tmp; rmdir $TMPDIR' 0 1 2 3 5 10 15
		
# search locally
# echo $find $SEARCHPATHS $excludes -or -print && exit
if $find -s $SEARCHPATHS $excludes -or -print 2>/dev/null |
        $mklocatedb -presort > $tmp
then
	case X"`$find $tmp -size -257c -print`" in
		X) cat $tmp > $FCODES;;
		*) echo "updatedb: locate database $tmp is empty"
		   exit 1
	esac
fi
