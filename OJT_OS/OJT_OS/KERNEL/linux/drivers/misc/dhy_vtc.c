// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>

#define DEVICE_NAME "dhy,vtc"
#define CLASS_NAME  "vtc"

// Register offsets (xvtc_hw.h 기준)
#define XVTC_CTL_OFFSET         0x000
#define XVTC_VER_OFFSET         0x010
#define XVTC_GASIZE_OFFSET      0x060  /* Generator Active Size */
#define XVTC_GFENC_OFFSET       0x068  /* Generator Encoding */
#define XVTC_GPOL_OFFSET        0x06C  /* Generator Polarity */
#define XVTC_GHSIZE_OFFSET      0x070  /* Generator HTotal */
#define XVTC_GVSIZE_OFFSET      0x074  /* Generator VTotal (F0|F1) */
#define XVTC_GHSYNC_OFFSET      0x078  /* Generator HSync Start|End */
#define XVTC_GVBHOFF_OFFSET     0x07C  /* Generator F0 VBlank H Offset */
#define XVTC_GVSYNC_OFFSET      0x080  /* Generator F0 VSync Start|End */
#define XVTC_GVSHOFF_OFFSET     0x084  /* Generator F0 VSync H Offset */
#define XVTC_GVBHOFF_F1_OFFSET  0x088  /* Generator F1 VBlank H Offset */
#define XVTC_GVSYNC_F1_OFFSET   0x08C  /* Generator F1 VSync Start|End */
#define XVTC_GVSHOFF_F1_OFFSET  0x090  /* Generator F1 VSync H Offset */
#define XVTC_GASIZE_F1_OFFSET   0x094  /* Generator Active Size F1 */
#define XVTC_GSRCSEL_OFFSET     0x0A0
#define XVTC_REG_SIZE           0x1000


// Register Bitfield Masks
#define XVTC_REGUPDATE_MASK  0x00000002
#define XVTC_CTL_GEN_ENABLE  0x00000004
#ifndef XVTC_CTL_ALLSS_MASK
#define XVTC_CTL_ALLSS_MASK      0x03FDEF00
#endif
#ifndef XVTC_CTL_FIPSS_MASK
#define XVTC_CTL_FIPSS_MASK      0x04000000
#endif
#ifndef XVTC_CTL_ACPSS_MASK
#define XVTC_CTL_ACPSS_MASK      0x02000000
#endif
#ifndef XVTC_CTL_AVPSS_MASK
#define XVTC_CTL_AVPSS_MASK      0x01000000
#endif
#ifndef XVTC_CTL_HSPSS_MASK
#define XVTC_CTL_HSPSS_MASK      0x00800000
#endif
#ifndef XVTC_CTL_VSPSS_MASK
#define XVTC_CTL_VSPSS_MASK      0x00400000
#endif
#ifndef XVTC_CTL_HBPSS_MASK
#define XVTC_CTL_HBPSS_MASK      0x00200000
#endif
#ifndef XVTC_CTL_VBPSS_MASK
#define XVTC_CTL_VBPSS_MASK      0x00100000
#endif
#ifndef XVTC_CTL_VCSS_MASK
#define XVTC_CTL_VCSS_MASK       0x00040000
#endif
#ifndef XVTC_CTL_VASS_MASK
#define XVTC_CTL_VASS_MASK       0x00020000
#endif
#ifndef XVTC_CTL_VBSS_MASK
#define XVTC_CTL_VBSS_MASK       0x00010000
#endif
#ifndef XVTC_CTL_VSSS_MASK
#define XVTC_CTL_VSSS_MASK       0x00008000
#endif
#ifndef XVTC_CTL_VFSS_MASK
#define XVTC_CTL_VFSS_MASK       0x00004000
#endif
#ifndef XVTC_CTL_VTSS_MASK
#define XVTC_CTL_VTSS_MASK       0x00002000
#endif
#ifndef XVTC_CTL_HBSS_MASK
#define XVTC_CTL_HBSS_MASK       0x00000800
#endif
#ifndef XVTC_CTL_HSSS_MASK
#define XVTC_CTL_HSSS_MASK       0x00000400
#endif
#ifndef XVTC_CTL_HFSS_MASK
#define XVTC_CTL_HFSS_MASK       0x00000200
#endif
#ifndef XVTC_CTL_HTSS_MASK
#define XVTC_CTL_HTSS_MASK       0x00000100
#endif
#ifndef XVTC_CTL_INTERLACE_MASK
#define XVTC_CTL_INTERLACE_MASK  0x00080000
#endif

// IOCTL Commands
#define XVTC_IOCTL_LOOKUPCONFIG     _IOR('V', 1, unsigned int)      /* 레지스터 base 주소 조회 (커널→유저) */
#define XVTC_IOCTL_CFINITIALIZE     _IO('V', 2)                     /* 초기화 (probe에서 완료, no-op) */
#define XVTC_IOCTL_SELFTEST         _IOR('V', 3, unsigned int)      /* VTC 버전 레지스터 읽기 (커널→유저) */
#define XVTC_IOCTL_SETGEN_TIMING    _IOW('V', 4, struct vtc_timing) /* 타이밍 값 설정 (유저→커널) */
#define XVTC_IOCTL_SETSOURCE        _IOW('V', 5, struct vtc_source) /* 소스 선택 설정 (유저→커널) */
#define XVTC_IOCTL_REGUPDATEENABLE  _IO('V', 6)                     /* 레지스터 업데이트 허용 */
#define XVTC_IOCTL_ENABLEGENERATOR  _IO('V', 7)                     /* VTC generator 시작 */

static int major;
static struct class *vtc_class;
static struct xvtc_dev *g_xvtc;
static DEFINE_MUTEX(vtc_mutex);

struct xvtc_dev {
    void __iomem *regs;
    struct device *dev;
};

struct vtc_timing {
    u32 HActiveVideo;   /* 수평 유효 영상 픽셀 수 (예: 1280) */
    u32 HFrontPorch;    /* 영상 끝 → HSync 시작 구간 (예: 48) */
    u32 HSyncWidth;     /* HSync 펄스 폭 (예: 112) */
    u32 HBackPorch;     /* HSync 끝 → 다음 라인 시작 구간 (예: 248) */
    u32 HSyncPolarity;  /* HSync 극성 (1=positive, 0=negative) */
    u32 VActiveVideo;   /* 수직 유효 영상 라인 수 (예: 1024) */
    u32 V0FrontPorch;   /* 마지막 라인 → VSync 시작 구간 (예: 1) */
    u32 V0SyncWidth;    /* VSync 펄스 폭 (예: 3) */
    u32 V0BackPorch;    /* VSync 끝 → 다음 프레임 시작 구간 (예: 38) */
    u32 VSyncPolarity;  /* VSync 극성 (1=positive, 0=negative) */
    u32 Interlaced;     /* 인터레이스 모드 (0=progressive, 1=interlaced) */
};

struct vtc_source {
    u32 VBlankPolSrc;        /* VBlank 극성 소스 (1=generator) */
    u32 VSyncPolSrc;         /* VSync 극성 소스 (1=generator) */
    u32 HBlankPolSrc;        /* HBlank 극성 소스 (1=generator) */
    u32 HSyncPolSrc;         /* HSync 극성 소스 (1=generator) */
    u32 ActiveVideoPolSrc;   /* Active Video 극성 소스 (1=generator) */
    u32 ActiveChromaPolSrc;  /* Active Chroma 극성 소스 (1=generator) */
    u32 VChromaSrc;          /* 수직 Chroma 소스 (1=generator) */
    u32 VActiveSrc;          /* 수직 Active 크기 소스 (1=generator) */
    u32 VBackPorchSrc;       /* 수직 Back Porch 소스 (1=generator) */
    u32 VSyncSrc;            /* VSync 타이밍 소스 (1=generator) */
    u32 VFrontPorchSrc;      /* 수직 Front Porch 소스 (1=generator) */
    u32 VTotalSrc;           /* 수직 Total 소스 (1=generator) */
    u32 HActiveSrc;          /* 수평 Active 크기 소스 (1=generator) */
    u32 HBackPorchSrc;       /* 수평 Back Porch 소스 (1=generator) */
    u32 HSyncSrc;            /* HSync 타이밍 소스 (1=generator) */
    u32 HFrontPorchSrc;      /* 수평 Front Porch 소스 (1=generator) */
    u32 HTotalSrc;           /* 수평 Total 소스 (1=generator) */
    u32 InterlacedMode;      /* 인터레이스 모드 (0=progressive) */
    u32 FieldIdPolSrc;       /* Field ID 극성 소스 (1=generator) */
};

static u32 xvtc_read(struct xvtc_dev *xvtc, u32 offset)
{
    return ioread32(xvtc->regs + offset);
}
static void xvtc_write(struct xvtc_dev *xvtc, u32 offset, u32 value)
{
    iowrite32(value, xvtc->regs + offset);
}

/*
 * 1920x1080@60Hz 표준 타이밍 (CEA-861, VESA DMT, HDMI 등 참조)
 *   width=1920, height=1080
 *   hps=2008, hpe=2052, hmax=2200, hpol=1
 *   vps=1084, vpe=1089, vmax=1124, vpol=1
 *
 * bare-metal VTC_Setting() 변환:
 *   HFrontPorch  = hps - width   = 2008 - 1920 = 88
 *   HSyncWidth   = hpe - hps     = 2052 - 2008 = 44
 *   HBackPorch   = hmax - hpe +1 = 2200 - 2052 + 1 = 149 (실제 HW는 148 사용)
 *   VFrontPorch  = vps - height  = 1084 - 1080 = 4
 *   VSyncWidth   = vpe - vps     = 1089 - 1084 = 5dd if=/dev/zero of=/dev/fb0 bs=5760 count=1080
 *   VBackPorch   = vmax - vpe +1 = 1124 - 1089 + 1 = 36
 *
 * 참고: HDMI/FPGA/디스플레이 표준 타이밍은 CEA-861, VESA DMT, Xilinx PG, 리눅스 커널 소스(drivers/gpu/drm/drm_edid.c) 등에서 확인 가능
 */
/* --- 1920x1080@60Hz (CEA-861 / VESA DMT) --- */
#define DEFAULT_1080P_HACTIVE   1920
#define DEFAULT_1080P_VACTIVE   1080
#define DEFAULT_1080P_HFPORCH   88
#define DEFAULT_1080P_HSYNCW    44
#define DEFAULT_1080P_HBPORCH   148
#define DEFAULT_1080P_HPOL      1
#define DEFAULT_1080P_VFPORCH   4
#define DEFAULT_1080P_VSYNCW    5
#define DEFAULT_1080P_VBPORCH   36
#define DEFAULT_1080P_VPOL      1

/*
 * 1280x1024@60Hz (VESA DMT, 108 MHz pixel clock)
 *   width=1280, height=1024
 *   hps=1328, hpe=1440, hmax=1687, hpol=1
 *   vps=1025, vpe=1028, vmax=1065, vpol=1
 *
 * bare-metal VTC_Setting() 변환:
 *   HFrontPorch  = hps - width   = 1328 - 1280 = 48
 *   HSyncWidth   = hpe - hps     = 1440 - 1328 = 112
 *   HBackPorch   = hmax - hpe +1 = 1687 - 1440 + 1 = 248
 *   VFrontPorch  = vps - height  = 1025 - 1024 = 1
 *   VSyncWidth   = vpe - vps     = 1028 - 1025 = 3
 *   VBackPorch   = vmax - vpe +1 = 1065 - 1028 + 1 = 38
 */
#define DEFAULT_1024P_HACTIVE   1280
#define DEFAULT_1024P_VACTIVE   1024
#define DEFAULT_1024P_HFPORCH   48
#define DEFAULT_1024P_HSYNCW    112
#define DEFAULT_1024P_HBPORCH   248
#define DEFAULT_1024P_HPOL      1
#define DEFAULT_1024P_VFPORCH   1
#define DEFAULT_1024P_VSYNCW    3
#define DEFAULT_1024P_VBPORCH   38
#define DEFAULT_1024P_VPOL      1

/* 현재 기본값: 1920x1080 (dtsi에서 오버라이드 가능) */
#define DEFAULT_HACTIVE     DEFAULT_1080P_HACTIVE
#define DEFAULT_VACTIVE     DEFAULT_1080P_VACTIVE
#define DEFAULT_HFPORCH     DEFAULT_1080P_HFPORCH
#define DEFAULT_HSYNCW      DEFAULT_1080P_HSYNCW
#define DEFAULT_HBPORCH     DEFAULT_1080P_HBPORCH
#define DEFAULT_HPOL        DEFAULT_1080P_HPOL
#define DEFAULT_VFPORCH     DEFAULT_1080P_VFPORCH
#define DEFAULT_VSYNCW      DEFAULT_1080P_VSYNCW
#define DEFAULT_VBPORCH     DEFAULT_1080P_VBPORCH
#define DEFAULT_VPOL        DEFAULT_1080P_VPOL

// IP Driver Layer
/**
 * xvtc_set_timing - VTC 타이밍 레지스터 설정
 *
 * bare-metal의 XVtc_SetGeneratorTiming() 전체 경로를 통합:
 *   XVtc_ConvTiming2Signal() → XVtc_SetPolarity()
 *                             → XVtc_SetGenerator() (OriginMode=1)
 *                             → XVtc_SetGeneratorHoriOffset()
 *
 * 레지스터 맵 (xvtc_hw.h):
 *   0x060 GASIZE       : Active Size (F0)
 *   0x068 GFENC        : Encoding (chroma parity, interlace)
 *   0x06C GPOL         : Polarity
 *   0x070 GHSIZE       : HTotal
 *   0x074 GVSIZE       : VTotal (F0|F1)
 *   0x078 GHSYNC       : HSync Start|End
 *   0x07C GVBHOFF      : F0 VBlank Horizontal Offset
 *   0x080 GVSYNC       : F0 VSync Start|End
 *   0x084 GVSHOFF      : F0 VSync Horizontal Offset
 *   0x088 GVBHOFF_F1   : F1 VBlank Horizontal Offset
 *   0x08C GVSYNC_F1    : F1 VSync Start|End
 *   0x090 GVSHOFF_F1   : F1 VSync Horizontal Offset
 *   0x094 GASIZE_F1    : Active Size (F1)
 */
static void xvtc_set_timing(struct xvtc_dev *xvtc, struct vtc_timing *t)
{
    /*
     * Step 1: ConvTiming2Signal (OriginMode=1)
     * 타이밍 파라미터 → Signal 구조체 값 변환
     */
    u32 hactive         = t->HActiveVideo;
    u32 vactive         = t->VActiveVideo;
    u32 hfp_start       = hactive;                              /* HFrontPorchStart */
    u32 hsync_start     = hfp_start + t->HFrontPorch;          /* HSyncStart */
    u32 hbp_start       = hsync_start + t->HSyncWidth;         /* HBackPorchStart */
    u32 htotal          = hbp_start + t->HBackPorch;           /* HTotal */

    u32 vfp_start       = vactive;                              /* V0FrontPorchStart */
    u32 vsync_start     = vfp_start + t->V0FrontPorch - 1;     /* V0SyncStart (FW: -1) */
    u32 vbp_start       = vsync_start + t->V0SyncWidth;        /* V0BackPorchStart */
    u32 vtotal          = vbp_start + t->V0BackPorch + 1;      /* V0Total (FW: +1) */

    u32 reg;

    /*
     * Step 2: SetPolarity (ConvTiming2Signal에서 설정)
     * FW: ActiveChroma=1, ActiveVideo=1, FieldId=1
     *     VBlank=VSyncPol, VSyncPol, HBlank=HSyncPol, HSyncPol
     * → 전체 0x3F 또는 극성에 따라 0x7F
     */
    reg = 0;
    reg |= (1 << 0);                     /* ActiveChromaPol = 1 */
    reg |= (1 << 1);                     /* ActiveVideoPol  = 1 */
    reg |= (t->HSyncPolarity << 2);      /* HSyncPol */
    reg |= (t->HSyncPolarity << 3);      /* HBlankPol = HSyncPol */
    reg |= (t->VSyncPolarity << 4);      /* VSyncPol */
    reg |= (t->VSyncPolarity << 5);      /* VBlankPol = VSyncPol */
    reg |= (1 << 6);                     /* FieldIdPol = 1 */
    xvtc_write(xvtc, XVTC_GPOL_OFFSET, reg);

    /*
     * Step 3: SetGenerator (OriginMode=1 경로)
     */

    /* GASIZE (0x060): hactive[12:0] | vactive[28:16] */
    reg = (hactive & 0x1FFF) | ((vactive & 0x1FFF) << 16);
    xvtc_write(xvtc, XVTC_GASIZE_OFFSET, reg);

    /* GASIZE_F1 (0x094): FW는 vactive만 상위에 기록, hactive=0 */
    reg = (vactive & 0x1FFF) << 16;
    xvtc_write(xvtc, XVTC_GASIZE_F1_OFFSET, reg);

    /* GHSIZE (0x070): HTotal */
    xvtc_write(xvtc, XVTC_GHSIZE_OFFSET, htotal & 0x1FFF);

    /* GVSIZE (0x074): VTotal_F0[12:0] | VTotal_F1[28:16] */
    reg = (vtotal & 0x1FFF) | ((vtotal & 0x1FFF) << 16);
    xvtc_write(xvtc, XVTC_GVSIZE_OFFSET, reg);

    /* GHSYNC (0x078): HSyncStart[12:0] | HBackPorchStart[28:16] */
    reg = (hsync_start & 0x1FFF) | ((hbp_start & 0x1FFF) << 16);
    xvtc_write(xvtc, XVTC_GHSYNC_OFFSET, reg);

    /* GVSYNC (0x080): V0SyncStart[12:0] | V0BackPorchStart[28:16] */
    reg = (vsync_start & 0x1FFF) | ((vbp_start & 0x1FFF) << 16);
    xvtc_write(xvtc, XVTC_GVSYNC_OFFSET, reg);

    /* GVSYNC_F1 (0x08C): progressive이므로 F0와 동일 */
    xvtc_write(xvtc, XVTC_GVSYNC_F1_OFFSET, reg);

    /* GFENC (0x068): Encoding — chroma parity=0, interlace=0 (progressive) */
    reg = xvtc_read(xvtc, XVTC_GFENC_OFFSET);
    reg &= ~0x00000002;  /* CPARITY 클리어 */
    reg &= ~0x00000020;  /* PROG(interlace) 클리어 */
    xvtc_write(xvtc, XVTC_GFENC_OFFSET, reg);

    /*
     * Step 4: SetGeneratorHoriOffset
     * FW (OriginMode=1, progressive):
     *   V0BlankHoriStart = V0BlankHoriEnd = hactive (= HFrontPorchStart)
     *   V0SyncHoriStart  = V0SyncHoriEnd  = HSyncStart
     *   F1 = F0 (progressive)
     */

    /* GVBHOFF (0x07C): V0BlankHoriStart[12:0] | V0BlankHoriEnd[28:16] */
    reg = (hfp_start & 0x1FFF) | ((hfp_start & 0x1FFF) << 16);
    xvtc_write(xvtc, XVTC_GVBHOFF_OFFSET, reg);

    /* GVSHOFF (0x084): V0SyncHoriStart[12:0] | V0SyncHoriEnd[28:16] */
    reg = (hsync_start & 0x1FFF) | ((hsync_start & 0x1FFF) << 16);
    xvtc_write(xvtc, XVTC_GVSHOFF_OFFSET, reg);

    /* GVBHOFF_F1 (0x088): progressive이므로 F0와 동일 */
    reg = (hfp_start & 0x1FFF) | ((hfp_start & 0x1FFF) << 16);
    xvtc_write(xvtc, XVTC_GVBHOFF_F1_OFFSET, reg);

    /* GVSHOFF_F1 (0x090): progressive이므로 F0와 동일 */
    reg = (hsync_start & 0x1FFF) | ((hsync_start & 0x1FFF) << 16);
    xvtc_write(xvtc, XVTC_GVSHOFF_F1_OFFSET, reg);
}

/**
 * xvtc_set_source - VTC 소스 선택 설정
 * bare-metal의 XVtc_SetSource()에 해당
 * 모든 소스를 generator로 설정 (bare-metal과 동일)
 */
static void xvtc_set_source(struct xvtc_dev *xvtc)
{
    u32 val;

    val = xvtc_read(xvtc, XVTC_CTL_OFFSET);
    val &= ~(XVTC_CTL_ALLSS_MASK | XVTC_CTL_FIPSS_MASK);

    /* 모든 Source Select 비트를 1로 (Generator 소스 선택, 펌웨어와 동일) */
    val |= XVTC_CTL_ACPSS_MASK;   /* Bit 25: ActiveChromaPolSrc */
    val |= XVTC_CTL_AVPSS_MASK;   /* Bit 24: ActiveVideoPolSrc */
    val |= XVTC_CTL_HSPSS_MASK;   /* Bit 23: HSyncPolSrc */
    val |= XVTC_CTL_VSPSS_MASK;   /* Bit 22: VSyncPolSrc */
    val |= XVTC_CTL_HBPSS_MASK;   /* Bit 21: HBlankPolSrc */
    val |= XVTC_CTL_VBPSS_MASK;   /* Bit 20: VBlankPolSrc */
    val |= XVTC_CTL_VCSS_MASK;    /* Bit 18: VChromaSrc */
    val |= XVTC_CTL_VBSS_MASK;    /* Bit 17: VBlankHOffSrc */
    val |= XVTC_CTL_VSSS_MASK;    /* Bit 16: VSyncEndSrc */
    val |= XVTC_CTL_VFSS_MASK;    /* Bit 15: VSyncStartSrc */
    val |= XVTC_CTL_VASS_MASK;    /* Bit 14: VActiveSizeSrc */
    val |= XVTC_CTL_VTSS_MASK;    /* Bit 13: VFrameSizeSrc */
    val |= XVTC_CTL_HBSS_MASK;    /* Bit 11: HSyncEndSrc */
    val |= XVTC_CTL_HSSS_MASK;    /* Bit 10: HSyncStartSrc */
    val |= XVTC_CTL_HFSS_MASK;    /* Bit  9: HActiveSizeSrc */
    val |= XVTC_CTL_HTSS_MASK;    /* Bit  8: HFrameSizeSrc */

    xvtc_write(xvtc, XVTC_CTL_OFFSET, val);
}

/**
 * xvtc_reg_update_enable - 레지스터 업데이트 활성화
 * bare-metal의 XVtc_RegUpdateEnable()에 해당
 */
static void xvtc_reg_update_enable(struct xvtc_dev *xvtc)
{
    u32 val = xvtc_read(xvtc, XVTC_CTL_OFFSET);
    xvtc_write(xvtc, XVTC_CTL_OFFSET, val | XVTC_REGUPDATE_MASK);
}

/**
 * xvtc_enable_generator - Generator 활성화
 * bare-metal의 XVtc_EnableGenerator()에 해당
 */
static void xvtc_enable_generator(struct xvtc_dev *xvtc)
{
    u32 val = xvtc_read(xvtc, XVTC_CTL_OFFSET);
    xvtc_write(xvtc, XVTC_CTL_OFFSET, val | XVTC_CTL_GEN_ENABLE);
}

// File Operation Layer
static long vtc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct xvtc_dev *xvtc = g_xvtc;
    struct vtc_timing timing;
    struct vtc_source source;
    u32 val;

    mutex_lock(&vtc_mutex);

    switch (cmd) {
    case XVTC_IOCTL_LOOKUPCONFIG:                       /* 레지스터 base 주소 조회 → 유저로 전달 */
        val = (u32)(uintptr_t)xvtc->regs;
        if (copy_to_user((void __user *)arg, &val, sizeof(val)))
            goto err;
        break;
    case XVTC_IOCTL_CFINITIALIZE:                       /* 초기화 (probe에서 완료, 여기선 no-op) */
        break;
    case XVTC_IOCTL_SELFTEST:                           /* VTC 버전 레지스터 읽기 → 유저로 전달 */
        val = xvtc_read(xvtc, XVTC_VER_OFFSET);
        if (copy_to_user((void __user *)arg, &val, sizeof(val)))
            goto err;
        break;
    case XVTC_IOCTL_SETGEN_TIMING:                      /* 유저에서 받은 타이밍 값으로 HW 레지스터 설정 */
        if (copy_from_user(&timing, (void __user *)arg, sizeof(timing)))
            goto err;
        xvtc_set_timing(xvtc, &timing);
        break;
    case XVTC_IOCTL_SETSOURCE:                          /* 유저에서 받은 소스 선택 값으로 CTL 레지스터 설정 */
        if (copy_from_user(&source, (void __user *)arg, sizeof(source)))
            goto err;
        // Control Register 값 읽기 및 SourceSelect 관련 비트 클리어
        val = xvtc_read(xvtc, XVTC_CTL_OFFSET);
        val &= ~XVTC_CTL_ALLSS_MASK;

        if (source.FieldIdPolSrc)
            val |= XVTC_CTL_FIPSS_MASK;
        if (source.ActiveChromaPolSrc)
            val |= XVTC_CTL_ACPSS_MASK;
        if (source.ActiveVideoPolSrc)
            val |= XVTC_CTL_AVPSS_MASK;
        if (source.HSyncPolSrc)
            val |= XVTC_CTL_HSPSS_MASK;
        if (source.VSyncPolSrc)
            val |= XVTC_CTL_VSPSS_MASK;
        if (source.HBlankPolSrc)
            val |= XVTC_CTL_HBPSS_MASK;
        if (source.VBlankPolSrc)
            val |= XVTC_CTL_VBPSS_MASK;
        if (source.VChromaSrc)
            val |= XVTC_CTL_VCSS_MASK;
        if (source.VActiveSrc)
            val |= XVTC_CTL_VASS_MASK;
        if (source.VBackPorchSrc)
            val |= XVTC_CTL_VBSS_MASK;
        if (source.VSyncSrc)
            val |= XVTC_CTL_VSSS_MASK;
        if (source.VFrontPorchSrc)
            val |= XVTC_CTL_VFSS_MASK;
        if (source.VTotalSrc)
            val |= XVTC_CTL_VTSS_MASK;
        if (source.HBackPorchSrc)
            val |= XVTC_CTL_HBSS_MASK;
        if (source.HSyncSrc)
            val |= XVTC_CTL_HSSS_MASK;
        if (source.HFrontPorchSrc)
            val |= XVTC_CTL_HFSS_MASK;
        if (source.HTotalSrc)
            val |= XVTC_CTL_HTSS_MASK;
        if (source.InterlacedMode)
            val |= XVTC_CTL_INTERLACE_MASK;

        xvtc_write(xvtc, XVTC_CTL_OFFSET, val);
        break;
    case XVTC_IOCTL_REGUPDATEENABLE:                    /* 레지스터 업데이트 허용 (변경값 HW 반영) */
        xvtc_reg_update_enable(xvtc);
        break;
    case XVTC_IOCTL_ENABLEGENERATOR:                    /* VTC generator 시작 (타이밍 신호 출력 개시) */
        xvtc_enable_generator(xvtc);
        break;
    default:                                             /* 알 수 없는 명령 → ENOTTY 반환 */
        mutex_unlock(&vtc_mutex);
        return -ENOTTY;
    }

    mutex_unlock(&vtc_mutex);
    return 0;
err:
    mutex_unlock(&vtc_mutex);
    return -EFAULT;
}

static int vtc_open(struct inode *inode, struct file *file)
{
    file->private_data = g_xvtc;
    return 0;
}

static int vtc_release(struct inode *inode, struct file *file)
{
    return 0;
}

static const struct file_operations vtc_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = vtc_ioctl,
    .open = vtc_open,
    .release = vtc_release,
};

static struct dentry *vtc_debugfs_dir;

static int vtc_status_show(struct seq_file *m, void *v)
{
    struct xvtc_dev *xvtc = g_xvtc;
    if (!xvtc || !xvtc->regs) {
        seq_puts(m, "vtc not initialized\n");
        return 0;
    }
    seq_printf(m, "CTL:         0x%08X\n", xvtc_read(xvtc, XVTC_CTL_OFFSET));
    seq_printf(m, "VER:         0x%08X\n", xvtc_read(xvtc, XVTC_VER_OFFSET));
    seq_printf(m, "GASIZE:      0x%08X  (0x060)\n", xvtc_read(xvtc, XVTC_GASIZE_OFFSET));
    seq_printf(m, "GFENC:       0x%08X  (0x068)\n", xvtc_read(xvtc, XVTC_GFENC_OFFSET));
    seq_printf(m, "GPOL:        0x%08X  (0x06C)\n", xvtc_read(xvtc, XVTC_GPOL_OFFSET));
    seq_printf(m, "GHSIZE:      0x%08X  (0x070)\n", xvtc_read(xvtc, XVTC_GHSIZE_OFFSET));
    seq_printf(m, "GVSIZE:      0x%08X  (0x074)\n", xvtc_read(xvtc, XVTC_GVSIZE_OFFSET));
    seq_printf(m, "GHSYNC:      0x%08X  (0x078)\n", xvtc_read(xvtc, XVTC_GHSYNC_OFFSET));
    seq_printf(m, "GVBHOFF:     0x%08X  (0x07C)\n", xvtc_read(xvtc, XVTC_GVBHOFF_OFFSET));
    seq_printf(m, "GVSYNC:      0x%08X  (0x080)\n", xvtc_read(xvtc, XVTC_GVSYNC_OFFSET));
    seq_printf(m, "GVSHOFF:     0x%08X  (0x084)\n", xvtc_read(xvtc, XVTC_GVSHOFF_OFFSET));
    seq_printf(m, "GVBHOFF_F1:  0x%08X  (0x088)\n", xvtc_read(xvtc, XVTC_GVBHOFF_F1_OFFSET));
    seq_printf(m, "GVSYNC_F1:   0x%08X  (0x08C)\n", xvtc_read(xvtc, XVTC_GVSYNC_F1_OFFSET));
    seq_printf(m, "GVSHOFF_F1:  0x%08X  (0x090)\n", xvtc_read(xvtc, XVTC_GVSHOFF_F1_OFFSET));
    seq_printf(m, "GASIZE_F1:   0x%08X  (0x094)\n", xvtc_read(xvtc, XVTC_GASIZE_F1_OFFSET));
    seq_printf(m, "GSRCSEL:     0x%08X  (0x0A0)\n", xvtc_read(xvtc, XVTC_GSRCSEL_OFFSET));
    return 0;
}

static int vtc_status_open(struct inode *inode, struct file *file)
{
    return single_open(file, vtc_status_show, NULL);
}

static const struct file_operations vtc_status_fops = {
    .owner = THIS_MODULE,
    .open = vtc_status_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

// Platform Driver Layer
static int xvtc_probe(struct platform_device *pdev)
{
    struct xvtc_dev *xvtc;
    struct resource *res;
    struct device_node *node = pdev->dev.of_node;
    struct vtc_timing timing;

    xvtc = devm_kzalloc(&pdev->dev, sizeof(*xvtc), GFP_KERNEL);
    if (!xvtc)
        return -ENOMEM;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    xvtc->regs = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(xvtc->regs))
        return PTR_ERR(xvtc->regs);

    xvtc->dev = &pdev->dev;
    platform_set_drvdata(pdev, xvtc);
    g_xvtc = xvtc;

    /*
     * DTS에서 타이밍 값 읽기, 없으면 1280x1024@60Hz 기본값 사용
     * bare-metal VTC_Setting() 순서와 동일:
     *   1. RegUpdateEnable
     *   2. SetGeneratorTiming
     *   3. SetSource
     *   4. EnableGenerator
     */
    if (of_property_read_u32(node, "hactive", &timing.HActiveVideo))
        timing.HActiveVideo = DEFAULT_HACTIVE;
    if (of_property_read_u32(node, "hfporch", &timing.HFrontPorch))
        timing.HFrontPorch = DEFAULT_HFPORCH;
    if (of_property_read_u32(node, "hsyncw", &timing.HSyncWidth))
        timing.HSyncWidth = DEFAULT_HSYNCW;
    if (of_property_read_u32(node, "hbporch", &timing.HBackPorch))
        timing.HBackPorch = DEFAULT_HBPORCH;
    if (of_property_read_u32(node, "hpol", &timing.HSyncPolarity))
        timing.HSyncPolarity = DEFAULT_HPOL;
    if (of_property_read_u32(node, "vactive", &timing.VActiveVideo))
        timing.VActiveVideo = DEFAULT_VACTIVE;
    if (of_property_read_u32(node, "vfporch", &timing.V0FrontPorch))
        timing.V0FrontPorch = DEFAULT_VFPORCH;
    if (of_property_read_u32(node, "vsyncw", &timing.V0SyncWidth))
        timing.V0SyncWidth = DEFAULT_VSYNCW;
    if (of_property_read_u32(node, "vbporch", &timing.V0BackPorch))
        timing.V0BackPorch = DEFAULT_VBPORCH;
    if (of_property_read_u32(node, "vpol", &timing.VSyncPolarity))
        timing.VSyncPolarity = DEFAULT_VPOL;
    timing.Interlaced = 0;

    dev_info(&pdev->dev, "VTC auto-init: %ux%u@60Hz\n",
             timing.HActiveVideo, timing.VActiveVideo);

    /* bare-metal VTC_Setting() */
    xvtc_reg_update_enable(xvtc);       /* 1. 레지스터 업데이트 허용 (이후 쓰는 값이 HW에 반영) */
    xvtc_set_timing(xvtc, &timing);     /* 2. 타이밍 레지스터에 해상도/싱크 값 기록 */
    xvtc_set_source(xvtc);              /* 3. 모든 타이밍 소스를 내부 generator로 설정 (출력으로 쓰겠다) */
    xvtc_enable_generator(xvtc);        /* 4. VTC 출력 시작 (HSync/VSync 신호 생성 개시) */
    
    dev_info(&pdev->dev, "VTC generator enabled\n");
    
    /* Character device 등록 (ioctl로 런타임 변경 가능) */
    major = register_chrdev(0, DEVICE_NAME, &vtc_fops);
    if (major < 0) {
        dev_err(&pdev->dev, "Failed to register chrdev: %d\n", major);
        return major;
    }
    vtc_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(vtc_class)) {
        unregister_chrdev(major, DEVICE_NAME);
        dev_err(&pdev->dev, "Failed to create class\n");
        return PTR_ERR(vtc_class);
    }
    device_create(vtc_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
    mutex_init(&vtc_mutex);

    dev_info(&pdev->dev, "xvtc probed at %pa\n", &res->start);
    // debugfs 생성
    if (!vtc_debugfs_dir)
        vtc_debugfs_dir = debugfs_create_dir("dhy_vtc", NULL);
    debugfs_create_file("vtc_status", 0444, vtc_debugfs_dir, NULL, &vtc_status_fops);
    return 0;
}

static int xvtc_remove(struct platform_device *pdev)
{
    device_destroy(vtc_class, MKDEV(major, 0));
    class_destroy(vtc_class);
    unregister_chrdev(major, DEVICE_NAME);
    mutex_destroy(&vtc_mutex);
    debugfs_remove_recursive(vtc_debugfs_dir);
    vtc_debugfs_dir = NULL;
    return 0;
}

static const struct of_device_id xvtc_of_match[] = {
    { .compatible = "dhy,vtc", },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, xvtc_of_match);

static struct platform_driver xvtc_driver = {
    .probe  = xvtc_probe,
    .remove = xvtc_remove,
    .driver = {
        .name = "xvtc",
        .of_match_table = xvtc_of_match,
    },
};
module_platform_driver(xvtc_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AutoGen");
MODULE_DESCRIPTION("Xilinx Video Timing Controller (VTC) driver");

