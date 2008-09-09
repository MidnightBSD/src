#!/bin/sh
# beerware license, written by Dario Freni (saturnero@freesbie.org)
#
# You can set some variables here. Edit them to fit your needs.

# Set serial variable to 0 if you don't want serial console at all,
# 1 if you want comconsole and 2 if you want comconsole and vidconsole
serial=3D0

set -u

if [ $# -lt 2 ]; then
    echo "Usage: $0 source-iso-path output-img-path"
    exit 1
fi

isoimage=3D$1; shift
imgoutfile=3D$1; shift

export tmpdir=3D$(mktemp -d -t fbsdmount)
# Temp file and directory to be used later
export tmpfile=3D$(mktemp -t bsdmount)

export isodev=3D$(mdconfig -a -t vnode -f ${isoimage})

echo "#### Building bootable UFS image ####"

ISOSIZE=3D$(du -k ${isoimage} | awk '{print $1}')
SECTS=3D$((($ISOSIZE + ($ISOSIZE/5))*2))

# Root partition size

echo "Initializing image..."
dd if=3D/dev/zero of=3D${imgoutfile} count=3D${SECTS}
ls -l ${imgoutfile}
export imgdev=3D$(mdconfig -a -t vnode -f ${imgoutfile})

bsdlabel -w -B ${imgdev}
newfs -O1 /dev/${imgdev}a

mkdir -p ${tmpdir}/iso ${tmpdir}/img

mount -t cd9660 /dev/${isodev} ${tmpdir}/iso
mount /dev/${imgdev}a ${tmpdir}/img

echo "Copying files to the image..."
( cd ${tmpdir}/iso && find . -print -depth | cpio -dump ${tmpdir}/img )
#bzcat ${tmpdir}/iso/dist/root.dist.bz2 | mtree -PUr -p ${tmpdir}/img 2>&=
1 > /dev/null

#echo "/dev/ufs/${UFS_LABEL} / ufs ro 1 1" > ${tmpdir}/img/etc/fstab

if [ ${serial} -eq 2 ]; then
        echo "-D" > ${tmpdir}/img/boot.config
        echo 'console=3D"comconsole, vidconsole"' >> ${tmpdir}/img/boot/l=
oader.conf
elif [ ${serial} -eq 1 ]; then
        echo "-h" > ${tmpdir}/img/boot.config
        echo 'console=3D"comconsole"' >> ${tmpdir}/img/boot/loader.conf
fi

cleanup() {
    umount ${tmpdir}/iso
    mdconfig -d -u ${isodev}
    umount ${tmpdir}/img
    mdconfig -d -u ${imgdev}
    rm -rf ${tmpdir} ${tmpfile}
}

cleanup

ls -lh ${imgoutfile}
