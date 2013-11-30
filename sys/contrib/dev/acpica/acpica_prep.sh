#!/bin/sh
# $FreeBSD: src/sys/contrib/dev/acpica/acpica_prep.sh,v 1.7 2004/04/14 02:03:33 njl Exp $
#
# Unpack an ACPI CA drop and restructure it to fit the FreeBSD layout
#

if [ ! $# -eq 1 ]; then
	echo "usage: $0 acpica_archive"
	exit
fi

src=$1
wrk=./_acpi_ca_unpack
dst=./acpi_ca_destination

# files that should keep their full directory path
fulldirs="common compiler"
# files to remove
stripdirs="generate acpisrc"
stripfiles="16bit.h Makefile README a16find.c a16utils.asm	\
	a16utils.lst a16utils.obj adisasm.h acdos16.h acintel.h	\
	aclinux.h acmsvc.h acnetbsd.h acwin.h acwin64.h readme.txt"
	
# files to update paths in
src_update_files="acpi.h acpiosxf.h"

# pre-clean
echo pre-clean
rm -rf ${wrk}
rm -rf ${dst}
mkdir -p ${wrk}
mkdir -p ${dst}

# fetch document
echo fetch document
fetch http://developer.intel.com/technology/iapc/acpi/downloads/CHANGES.txt
tr -d '\r' < CHANGES.txt > CHANGES.txt.tmp
mv CHANGES.txt.tmp CHANGES.txt

# unpack
echo unpack
tar -x -z -f ${src} -C ${wrk}

# strip files
echo strip
for i in ${stripdirs}; do
    find ${wrk} -name ${i} -type d | xargs rm -r
done
for i in ${stripfiles}; do
    find ${wrk} -name ${i} -type f -delete
done

echo copying full dirs
for i in ${fulldirs}; do
    find ${wrk} -name ${i} -type d | xargs -J % mv % ${dst}
done

# move files to destination
echo copying flat dirs
find ${wrk} -type f | xargs -J % mv % ${dst}
mv CHANGES.txt ${dst}

# update src/headers for appropriate paths
echo updating paths
for i in ${src_update_files}; do
    i=${dst}/$i
    sed -e 's/platform\///' $i > $i.new && mv $i.new $i
done

# post-clean
echo post-clean
rm -rf ${wrk}

# assist the developer in generating a diff
echo "Directories you may want to 'cvs diff':"
echo "    src/sys/dev/acpica src/sys/i386/acpica src/sys/ia64/acpica \\"
echo "    src/sys/modules/acpi src/sys/boot src/sys/i386/include \\"
echo "    src/usr.sbin/acpi src/sys/contrib/dev/acpica"
