#!/bin/sh
# $FreeBSD: release/7.0.0/sys/i386/acpica/genwakecode.sh 138725 2004-12-12 06:59:14Z njl $
#
file2c 'static char wakecode[] = {' '};' <acpi_wakecode.bin

nm -n --defined-only acpi_wakecode.o | while read offset dummy what
do
    echo "#define ${what}	0x${offset}"
done

exit 0
