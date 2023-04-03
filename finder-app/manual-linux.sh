#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
SYSROOT=`${CROSS_COMPILE}gcc -print-sysroot`

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig
    make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/Image

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

mkdir rootfs
cd rootfs
mkdir -p \
    bin \
    dev \
    etc \
    home \
    lib \
    lib64 \
    proc \
    sbin \
    sys \
    tmp \
    usr \
    var \
    usr/bin \
    usr/lib \
    usr/sbin \
    var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    make distclean
    make defconfig
else
    cd busybox
fi

make \
    ARCH=${ARCH} \
    CROSS_COMPILE=${CROSS_COMPILE}
make \
    CONFIG_PREFIX=${OUTDIR}/rootfs \
    ARCH=${ARCH} \
    CROSS_COMPILE=${CROSS_COMPILE} \
    install


echo "Library dependencies"
cd "${OUTDIR}/rootfs"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

INTERPRETER=`${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter" | awk -F ':' '{ print substr($NF, 3, length($NF) - 3) }' | sed -E 's/^ +//' | sed -E 's/ +$//' | sed -E 's/ +/\n/'`
for ELEM in ${INTERPRETER}; do
    cp ${SYSROOT}/${ELEM} ${OUTDIR}/rootfs/${ELEM}
done

SHARED=`${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library" | awk -F ':' '{ print substr($NF, 3, length($NF) - 3) }' | sed -E 's/^ +//' | sed -E 's/ +$//' | sed -E 's/ +/\n/'`
for ELEM in ${SHARED}; do
    cp ${SYSROOT}/lib64/${ELEM} ${OUTDIR}/rootfs/lib64/${ELEM}
done

cd "${OUTDIR}/rootfs"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

cd "$FINDER_APP_DIR"
make clean
make CROSS_COMPILE=${CROSS_COMPILE}
make DST_DIR="${OUTDIR}/rootfs/home" install

cp \
    finder.sh \
    finder-test.sh \
    autorun-qemu.sh \
    "${OUTDIR}/rootfs/home"

mkdir -p "${OUTDIR}/rootfs/home/conf"
cp conf/assignment.txt conf/username.txt "${OUTDIR}/rootfs/home/conf"

sudo chown root:root -R "${OUTDIR}/rootfs"

cd "${OUTDIR}/rootfs"
find . | cpio \
    --create \
    --verbose \
    --format newc \
    --owner root:root \
    > ${OUTDIR}/initramfs.cpio
cd "$OUTDIR"
gzip -f initramfs.cpio

