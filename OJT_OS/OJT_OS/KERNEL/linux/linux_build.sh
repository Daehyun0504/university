#!/bin/sh
set -eu

# @file linux_build.sh
# @brief Build Linux kernel (zImage + DTBs) for Zynq using a fixed defconfig.
# @details Provides commands for menuconfig/defconfig/clean/distclean/build/uimage using a
#          statically configured kernel defconfig 

echo "Linux Kernel Build Script"

# User config for uImage (edit here)
_ARCH=arm
_CROSS_COMPILE=arm-linux-gnueabihf-
_DEFCONFIG=multi_v7_defconfig
_DATE=`date +%Y%m%d`
_UIMAGE_LOAD_ADDR=0x2000000
_UIMAGE_ENTRY_ADDR=0x2000000

case ${1:-} in
  menuconfig)
    echo "[STEP] Open Menuconfig"
    make ARCH=$_ARCH menuconfig
    ;;
  defconfig)
    echo "[STEP] Set kernel defconfig: $_DEFCONFIG"
    make ARCH=$_ARCH CROSS_COMPILE=$_CROSS_COMPILE $_DEFCONFIG
    ;;
  clean)
    echo "[STEP] Clean kernel build (keep .config)"
    make ARCH=$_ARCH CROSS_COMPILE=$_CROSS_COMPILE clean
    ;;
  distclean)
    echo "[STEP] Distclean kernel build (remove .config and generated files)"
    make ARCH=$_ARCH CROSS_COMPILE=$_CROSS_COMPILE distclean
    ;;
  bak|back)
    echo "[STEP] Backup kernel config (.config -> config_$_DATE)"
    cp ./.config ./config_$_DATE
    ;;
  build)
    echo "[STEP] Build Linux kernel: zImage + dtbs"
    make -j"$(nproc)" \
      ARCH=$_ARCH \
      CROSS_COMPILE=$_CROSS_COMPILE \
      zImage dtbs
    ;;
  uimage)
    echo "[STEP] Build uImage directly via kernel build system"
    make -j"$(nproc)" \
      ARCH=$_ARCH \
      CROSS_COMPILE=$_CROSS_COMPILE \
      LOADADDR="${_UIMAGE_LOAD_ADDR}" \
      uImage
    ;;
  *)
    echo "CMD : menuconfig, defconfig, build, uimage, clean, distclean, bak"
    echo "  ex) $0 defconfig"
    exit 1
    ;;
esac

echo "[OK] Done."
