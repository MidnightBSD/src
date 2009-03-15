# hints/amigaos.sh
#
# talk to pueschel@imsdd.meb.uni-bonn.de if you want to change this file.
#
# misc stuff
archname='m68k-amigaos'
cc='gcc'
firstmakefile='GNUmakefile'
usenm='true'
d_fork='undef' # available but ENOSYS

usemymalloc='n'
useperlio='true'
d_eofnblk='define'
groupstype='int'

# libs

libpth="$prefix/lib /local/lib"
glibpth="$libpth"
xlibpth="$libpth"

# This should remove unwanted libraries instead of limiting the set
# to just these few.  E.g. what about Berkeley DB?
libswanted='gdbm m dld'
so=' '
libs='-lm'

# compiler & linker flags
# Respect command-line values.

ccflags="$ccflags -DAMIGAOS"
case "$optimize" in
'') optimize='-O2 -fomit-frame-pointer';;
esac
dlext='o'
# Are these two different from the defaults?
cccdlflags='none'
ccdlflags='none'
lddlflags='-oformat a.out-amiga -r'

# uncomment the following settings if you are compiling for an 68020+ system
# and want a residentable executable instead of dynamic loading

# usedl='n'
# ccflags='-DAMIGAOS -mstackextend -m68020 -resident32'
# ldflags='-m68020 -resident32'

# AmigaOS always reports only two links to directories, even if they
# contain subdirectories.  Consequently, we use this variable to stop
# File::Find using the link count to determine whether there are
# subdirectories to be searched.  This will generate a harmless message:
# Hmm...You had some extra variables I don't know about...I'll try to keep 'em.
#	Propagating recommended variable dont_use_nlink
dont_use_nlink='define'
