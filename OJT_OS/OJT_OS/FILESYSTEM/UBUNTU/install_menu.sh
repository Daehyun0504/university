# Rootfs 경로

ROOTFS=/home/yoondh/work/OJT_OS/FILESYSTEM/UBUNTU/rootfs

# 1. 디렉토리 생성

sudo mkdir -p $ROOTFS/proc
sudo mkdir -p $ROOTFS/sys
sudo mkdir -p $ROOTFS/dev
sudo mkdir -p $ROOTFS/dev/pts

# 2. mount

sudo mount -t proc /proc $ROOTFS/proc
sudo mount -t sysfs /sys $ROOTFS/sys
sudo mount --bind /dev $ROOTFS/dev
sudo mount -t devpts devpts $ROOTFS/dev/pts

# 3. DNS 

sudo cp /etc/resolv.conf $ROOTFS/etc/

# 4. qemu 설치 (없으면)

sudo apt-get update
sudo apt-get install -y qemu-user-static

# 5. qemu 복사

sudo cp /usr/bin/qemu-arm-static $ROOTFS/usr/bin/

# 6. chroot 진입

sudo chroot $ROOTFS

# ===== 여기부터 chroot 내부 =====

# chroot 내부에서 실행

apt-get update

DEBIAN_FRONTEND=noninteractive apt-get install -y gstreamer0.10-tools
DEBIAN_FRONTEND=noninteractive apt-get install -y gstreamer0.10-plugins-base
DEBIAN_FRONTEND=noninteractive apt-get install -y gstreamer0.10-plugins-good
DEBIAN_FRONTEND=noninteractive apt-get install -y gstreamer0.10-plugins-bad

DEBIAN_FRONTEND=noninteractive apt-get install -y vlc

DEBIAN_FRONTEND=noninteractive apt-get install -y xserver-xorg
DEBIAN_FRONTEND=noninteractive apt-get install -y xserver-xorg-video-fbdev
DEBIAN_FRONTEND=noninteractive apt-get install -y ubuntu-desktop

exit
# ===== 여기부터 다시 host =====

# 7. unmount (역순)

sudo umount $ROOTFS/dev/pts
sudo umount $ROOTFS/dev
sudo umount $ROOTFS/sys
sudo umount $ROOTFS/proc
