#!/bin/bash

set -e

/** Rootfs path */
ROOTFS=/home/yoondh/work/OJT_OS/FILESYSTEM/UBUNTU/rootfs

echo "[INFO] Start rootfs package installation..."

# 1. 필수 디렉토리 생성
sudo mkdir -p $ROOTFS/{proc,sys,dev,dev/pts}

# 2. mount
echo "[INFO] Mounting filesystems..."
sudo mount -t proc /proc $ROOTFS/proc
sudo mount -t sysfs /sys $ROOTFS/sys
sudo mount --rbind /dev $ROOTFS/dev
sudo mount -t devpts devpts $ROOTFS/dev/pts

# 3. DNS 설정
echo "[INFO] Copy resolv.conf..."
sudo cp /etc/resolv.conf $ROOTFS/etc/

# 4. QEMU (ARM 대응)
if [ -f /usr/bin/qemu-arm-static ]; then
    echo "[INFO] Copy qemu-arm-static..."
    sudo cp /usr/bin/qemu-arm-static $ROOTFS/usr/bin/
fi

# 5. chroot 내부 실행
echo "[INFO] Installing packages inside rootfs..."
sudo chroot $ROOTFS /bin/bash -c "
apt-get update && \
DEBIAN_FRONTEND=noninteractive apt-get install -y \
gstreamer1.0-tools \
gstreamer1.0-plugins-base \
gstreamer1.0-plugins-good \
xserver-xorg \
xf86-video-modesetting \
ubuntu-desktop
"

echo "[INFO] Installation done."

# 6. cleanup (역순 unmount)
echo "[INFO] Unmounting..."
sudo umount -l $ROOTFS/dev/pts || true
sudo umount -l $ROOTFS/dev || true
sudo umount -l $ROOTFS/sys || true
sudo umount -l $ROOTFS/proc || true

echo "[INFO] Done."
