#!/bin/sh
# beerware license, written by Dario Freni (saturnero@freesbie.org)

# Set serial variable to 0 if you don't want serial console at all,
# 1 if you want comconsole and 2 if you want comconsole and vidconsole
serial=0

set -u

if [ $# -lt 2 ]; then
    echo "Usage: $0 source-iso-path output-img-path"
    exit 1
fi

isoimage=$1; shift
imgoutfile=$1; shift

export tmpdir=$(mktemp -d -t fbsdmount)
export isodev=$(mdconfig -a -t vnode -f ${isoimage})

echo "#### Building bootable UFS image ####"

ISOSIZE=$(du -k ${isoimage} | awk '{print $1}')
SECTS=$((($ISOSIZE + ($ISOSIZE/4))*4))

# Root partition size

echo "Initializing image..."
dd if=/dev/zero of=${imgoutfile} count=${SECTS}
ls -l ${imgoutfile}
export imgdev=$(mdconfig -a -t vnode -f ${imgoutfile})

bsdlabel -w -B ${imgdev}
newfs -O1 /dev/${imgdev}a

mkdir -p ${tmpdir}/iso ${tmpdir}/img

mount -t cd9660 /dev/${isodev} ${tmpdir}/iso
mount /dev/${imgdev}a ${tmpdir}/img

echo "Copying files to the image..."
( cd ${tmpdir}/iso && pax -rw . ${tmpdir}/img )

#echo "/dev/ufs/${UFS_LABEL} / ufs ro 1 1" > ${tmpdir}/img/etc/fstab

if [ ${serial} -eq 2 ]; then
        echo "-D" > ${tmpdir}/img/boot.config
        echo 'console="comconsole, vidconsole"' >> ${tmpdir}/img/boot/loader.conf
elif [ ${serial} -eq 1 ]; then
        echo "-h" > ${tmpdir}/img/boot.config
        echo 'console="comconsole"' >> ${tmpdir}/img/boot/loader.conf
fi

cleanup() {
    umount ${tmpdir}/iso
    mdconfig -d -u ${isodev}
    umount ${tmpdir}/img
    mdconfig -d -u ${imgdev}
}

cleanup

ls -lh ${imgoutfile}
