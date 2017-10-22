/* $FreeBSD: release/7.0.0/contrib/amd/conf/trap/trap_default.h 131712 2004-07-06 14:14:26Z mbr $ */
/* $srcdir/conf/trap/trap_default.h */
#define MOUNT_TRAP(type, mnt, flags, mnt_data) mount(type, mnt->mnt_dir, flags, mnt_data)
