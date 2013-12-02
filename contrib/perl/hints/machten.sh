#! /bin/bash
# machten.sh
# This is for MachTen 4.1.4.  It might work on other versions and variants
# too.  MachTen is now obsolete, lacks many features expected in modern UNIX
# implementations, and suffers from a number of bugs which are likely never
# to be fixed. This means that, in the absence of extensive work on
# this file and on the perl source code, versions of perl later than 5.6.x
# cannot successfully be built on MachTen. This file enforces this
# restriction. Should you wish to port a later version of perl to MachTen,
# feel free to contact me for pointers.
#                      -- Dominic Dunlop <domo@computer.org> 040213
#
# Users of earlier MachTen versions might need a fixed tr from ftp.tenon.com.
# This should be described in the MachTen release notes.
#
# MachTen 2.x has its own hint file.
#
# The original version of this file was put together by Andy Dougherty
# <doughera@lafayette.edu> based on comments from lots of
# folks, especially 
# 	Mark Pease <peasem@primenet.com>
#	Martijn Koster <m.koster@webcrawler.com>
#	Richard Yeh <rcyeh@cco.caltech.edu>
#
# Prevent building of perls later than 5.6.x, stating why -- see above.
#                      -- Dominic Dunlop <domo@computer.org> 040213
# Deny system's false claims to support mmap() and munmap(); note
# also that Sys V IPC (re)disabled by jhi due to continuing inadequacy
#                      -- Dominic Dunlop <domo@computer.org> 001111
# Remove dynamic loading libraries from search; enable SysV IPC with
# MachTen 4.1.4 and above; define SYSTEM_ALIGN_BYTES for old MT versions
#                      -- Dominic Dunlop <domo@computer.org> 000224
# Disable shadow password file access: MT 4.1.1 has necessary library
# functions, but not header file (or documentation)
#                      -- Dominic Dunlop <domo@computer.org> 990804
# For now, explicitly disable dynamic loading -- MT 4.1.1 has it,
# but these hints do not yet support it.
# Define NOTEDEF_MACHTEN to undo gratuitous Tenon hack to signal.h.
#                      -- Dominic Dunlop <domo@computer.org> 9800802
# Completely disable SysV IPC pending more complete support from Tenon
#                      -- Dominic Dunlop <domo@computer.org> 980712
# Use vfork and perl's malloc by default
#                      -- Dominic Dunlop <domo@computer.org> 980630
# Raise perl's stack size again; cut down reg_infty; document
#                      -- Dominic Dunlop <domo@computer.org> 980619
# Use of semctl() can crash system: disable -- Dominic Dunlop 980506
# Raise stack size further; slight tweaks to accomodate MT 4.1
#                      -- Dominic Dunlop <domo@computer.org> 980211
# Raise perl's stack size -- Dominic Dunlop <domo@tcp.ip.lu> 970922
# Reinstate sigsetjmp iff version is 4.0.3 or greater; use nm
# (assumes Configure change); prune libswanted -- Dominic Dunlop 970113
# Warn about test failure due to old Berkeley db -- Dominic Dunlop 970105
# Do not use perl's malloc; SysV IPC OK -- Neil Cutcliffe, Tenon 961030
# File::Find's use of link count disabled by Dominic Dunlop 960528
# Perl's use of sigsetjmp etc. disabled by Dominic Dunlop 960521

# Assume that PERL_REVISON in patchlevel.h is 5.
# If you want to try building perl-5.8.x or later, set PERL_VERSION_SAFE_MAX
# appropriately in your environment before running Configure.
if [ `awk '$1=="#define" && $2=="PERL_VERSION"{print $3}' patchlevel.h` \
      -gt ${PERL_VERSION_SAFE_MAX:-6} ]
then
    cat <<EOF >&4

Perl versions greater than 5.6.x have not been ported to MachTen. If you
wish to build a version from the 5.6 track, please see the notes in
README.machten
EOF
    exit 1
fi
#
# MachTen 4.1.1's support for shadow password file access is incomplete:
# disable its use completely.
d_getspnam=${d_getspnam:-undef}

# MachTen 4.1.1 does support dynamic loading, but perl doesn't
# know how to use it yet.
usedl=${usedl:-undef}

# MachTen 4.1.1 may have an unhelpful hack in /usr/include/signal.h.
# Undo it if so.
if grep NOTDEF_MACHTEN /usr/include/signal.h > /dev/null
then
    ccflags="$ccflags -DNOTDEF_MACHTEN"
fi

# Power MachTen is a real memory system and its standard malloc
# has been optimized for this. Using this malloc instead of Perl's
# malloc may result in significant memory savings.  In particular,
# unlike most UNIX memory allocation subsystems, MachTen's free()
# really does return unneeded process data memory to the system.
# However, MachTen's malloc() is woefully slow -- maybe 100 times
# slower than perl's own, so perl's own is usually the better
# choice.  In order to use perl's malloc(), the sbrk() system call
# must be simulated using MachTen's malloc().  See malloc.c for
# precise details of how this is achieved.  Recent improvements
# to perl's malloc() currently crash MachTen, and so are disabled
# by -DPLAIN_MALLOC and -DNO_FANCY_MALLOC.
usemymalloc=${usemymalloc:-y}

# Older versions of MachTen malloc() data on a two-byte boundary, which
# works, but slows down operations on long, float and double data.
# Perl's malloc() can compensate if SYSTEM_ALLOC_ALIGNMENT is suitably
# defined.
if expr "$osvers" \< "4.1" >/dev/null
then
system_alloc_alignment=" -DSYSTEM_ALLOC_ALIGNMENT=2"
fi
# Do not wrap the following long line
malloc_cflags='ccflags="$ccflags -DPLAIN_MALLOC -DNO_FANCY_MALLOC -DUSE_PERL_SBRK$system_alloc_alignment"'

# When MachTen does a fork(), it immediately copies the whole of
# the parent process' data space for the child.  This can be
# expensive.  Using vfork() where appropriate avoids this cost.
d_vfork=${d_vfork:-define}

# Specify a high level of optimization (-O3 wouldn't do much more)
optimize=${optimize:--O2 -fomit-frame-pointer}

# Make symbol table listings less voluminous
nmopts=-gp

# Set reg_infty -- the maximum allowable number of repeats in regular
# expressions such as  /a{1,$max_repeats}/, and the maximum number of
# times /a*/ will match.  Setting this too high without having a stack
# large enough to accommodate deep recursion in the regular expression
# engine allows perl to crash your Mac due to stack overrun if it
# encounters a pathological regular expression.  The default is a
# compromise between capability and required stack size (see below).
# You may override the default value from the Configure command-line
# like this:
#
#   Configure -Dreg_infty=16368 ...

reg_infty=${reg_infty:-2047}

# If you want to have many perl processes active simultaneously --
# processing CGI forms -- for example, you should opt for a small stack.
# For safety, you should set reg_infty no larger than the corresponding
# value given in this table:
#
# Stack size  reg_infty value supported
# ----------  -------------------------
# 128k        2**8-1    (256)
# 256k        2**9-1    (511)
# 512k        2**10-1  (1023)
#   1M        2**11-1  (2047)
# ...
#  16M        2**15-1 (32767) (perl's default value)

# This script selects a safe stack size based on the value of reg_infty
# specified above.  However, you may choose to take a risk and set
# stack size lower: pathological regular expressions are rare in real-world
# programs.  But be aware that, if perl does encounter one, it WILL
# crash your system.  Do not set stack size lower than 96k unless
# you want perl's installation tests ( make test ) to crash your system.
#
# You may override the default value from the Configure command-line
# by specifying the required size in kilobytes like this:
#
#   Configure -Dstack_size=96

if [ "X$stack_size" = 'X' ]
then
    stack_size=128
    X=`expr $reg_infty / 256`

    while [ $X -gt 0 ]
    do
	X=`expr $X / 2`
	stack_size=`expr $stack_size \* 2`
    done
    X=`expr $stack_size \* 1024`
fi

ldflags="$ldflags -Xlstack=$X"
ccflags="$ccflags -DREG_INFTY=$reg_infty"

# Install in /usr/local by default
prefix='/usr/local'

# At least on PowerMac, doubles must be aligned on 8 byte boundaries.
# I don't know if this is true for all MachTen systems, or how to
# determine this automatically.
alignbytes=8

# 4.0.2 and earlier had a problem with perl's use of sigsetjmp and
# friends.  Use setjmp and friends instead.
expr "$osvers" \< "4.0.3" > /dev/null && d_sigsetjmp='undef'

# System V IPC before MachTen 4.1.4 is incomplete (missing msg function
# prototypes, no ftok()), buggy (semctl(.., ..,  IPC_STATUS, ..) hangs
# system), and undocumented.  Claim it's not there at all before 4.1.4.
if expr "$osvers" \< "4.1.4" >/dev/null
then
d_msg=${d_msg:-undef}
d_sem=${d_sem:-undef}
d_shm=${d_shm:-undef}
fi


# As of MachTen 4.1.4 the msg* and shm* are in libc but unimplemented
# (an attempt to use them causes a runtime error)
# XXX Configure probe for really functional msg*() is needed XXX
# XXX Configure probe for really functional shm*() is needed XXX
if test "$d_msg" = ""; then
    d_msgget=${d_msgget:-undef}
    d_msgctl=${d_msgctl:-undef}
    d_msgsnd=${d_msgsnd:-undef}
    d_msgrcv=${d_msgrcv:-undef}
    case "$d_msgget$d_msgsnd$d_msgctl$d_msgrcv" in
    *"undef"*) d_msg="$undef" ;;
    esac
fi
if test "$d_shm" = ""; then
    d_shmat=${d_shmat:-undef}
    d_shmdt=${d_shmdt:-undef}
    d_shmget=${d_shmget:-undef}
    d_shmctl=${d_shmctl:-undef}
    case "$d_shmat$d_shmctl$d_shmdt$d_shmget" in
    *"undef"*) d_shm="$undef" ;;
    esac
fi

# MachTen has stubs for mmap and munmap(), but they just result in the
# caller being killed on the grounds of "Bad system call"
d_mmap=${d_mmap:-undef}
d_munmap=${d_munmap:-undef}

# Get rid of some extra libs which it takes Configure a tediously
# long time never to find on MachTen, or which break perl
set `echo X "$libswanted "|sed -e 's/ net / /' -e 's/ socket / /' \
    -e 's/ inet / /' -e 's/ nsl / /' -e 's/ nm / /' -e 's/ malloc / /' \
    -e 's/ ld / /' -e 's/ sun / /' -e 's/ posix / /' \
    -e 's/ cposix / /' -e 's/ crypt / /' -e 's/ dl / /' -e 's/ dld / /' \
    -e 's/ ucb / /' -e 's/ bsd / /' -e 's/ BSD / /' -e 's/ PW / /'`
shift
libswanted="$*"

# While link counts on MachTen 4.1's fast file systems work correctly,
# on Macintosh Heirarchical File Systems, (and on HFS+)
# MachTen always reports ony two links to directories, even if they
# contain subdirectories.  Consequently, we use this variable to stop
# File::Find using the link count to determine whether there are
# subdirectories to be searched.  This will generate a harmless message:
# Hmm...You had some extra variables I don't know about...I'll try to keep 'em.
#	Propagating recommended variable dont_use_nlink
dont_use_nlink=define

cat <<EOM >&4

At the end of Configure, you will see a harmless message

Hmm...You had some extra variables I don't know about...I'll try to keep 'em.
	Propagating recommended variable dont_use_nlink
        Propagating recommended variable nmopts
        Propagating recommended variable malloc_cflags...
        Propagating recommended variable reg_infty
        Propagating recommended variable system_alloc_alignment
Read the File::Find documentation for more information about dont_use_nlink

Your perl will be built with a stack size of ${stack_size}k and a regular
expression repeat count limit of $reg_infty.  If you want alternative
values, see the file hints/machten.sh for advice on how to change them.

Tests
	io/fs test 4  and
	op/stat test 3
may fail since MachTen may not return a useful nlinks field to stat
on directories.

EOM
expr "$osvers" \< "4.1" >/dev/null && test -r ./broken-db.msg && \
    . ./broken-db.msg

unset stack_size X
