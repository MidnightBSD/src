/* $FreeBSD: stable/10/contrib/amd/conf/trap/trap_default.h 174313 2007-12-05 16:57:05Z obrien $ */
/* $srcdir/conf/trap/trap_default.h */
#define MOUNT_TRAP(type, mnt, flags, mnt_data) mount(type, mnt->mnt_dir, flags, mnt_data)
