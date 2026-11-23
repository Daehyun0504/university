/*
 * framebuffer@0x70000000 {
 * 	compatible = "simple-framebuffer";
 * 	reg = <0x0 0x70000000 0x0 0x384000>;
 * 	clocks = <&clk>;
 * 	width = <0x500>;
 * 	height = <0x2d0>;
 * 	strude = <0xf00>;
 * 	format = "x8r8g8b8";
 * 	status = "okay";
 * };
 *
 * display@0xA0220000 {
 * 	#dma-cells = <1>;
 * 	compatible = "alice,vdma";
 * 	reg = <0x0 0xa0220000 0x0 0x10000>;
 * 	mm2s_hsize = <1280>;
 * 	mm2s_vsize = <720>;
 * 	mm2s_width = <3>;
 * 	mm2s_Addr = <0x70000000>;
 * 	s2mm_hsize = <1280>;
 * 	s2mm_vsize = <720>;
 * 	s2mm_width = <3>;
 * 	s2mm_Addr = <0x70000000>;
 * 	status = "okay";
 * };
 *
 */

#include <linux/bitops.h>
#include <linux/dmapool.h>
#include <linux/dma/xilinx_dma.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_dma.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include <linux/debugfs.h>

/*
Address Space Offset    Name                    Description
00h                     MM2S_VDMACR             MM2S VDMA Control Register
04h                     MM2S_VDMASR             MM2S VDMA Status Register
08h to 10h              Reserved                N/A
14h                     MM2S_REG_INDEX          MM2S Register Index
18h to 24h              Reserved                See Additional Design Information in
                                                Chapter 4 for more information.
28h                     PARK_PTR_REG            MM2S and S2MM Park Pointer Register
2Ch                     VDMA_VERSION            Video DMA Version Register
30h                     S2MM_VDMACR             S2MM VDMA Control Register
34h                     S2MM_VDMASR             S2MM VDMA Status Register
38h                     Reserved                N/A
3Ch                     S2MM_VDMA_IRQ_MASK      S2MM Error Interrupt Mask Register
40h                     Reserved                N/A
44h                     S2MM_REG_INDEX          S2MM Register Index
48h to 4Ch              Reserved                N/A
50h                     MM2S_VSIZE              MM2S Vertical Size Register
54h                     MM2S_HSIZE              MM2S Horizontal Size Register
58h                     MM2S_FRMDLY_STRIDE      MM2S Frame Delay and Stride Register
5Ch to 98h              MM2S_START_ADDRESS      MM2S Start Address (1 to 16)
                                                (1 to 16)
9Ch                     Reserved                N/A
A0h                     S2MM_VSIZE              S2MM Vertical Size Register
A4h                     S2MM_HSIZE              S2MM Horizontal Size Register
A8h                     S2MM_FRMDLY_STRIDE      S2MM Frame Delay and Stride Register
ACh to E8h              S2MM_START_ADDRESS      S2MM Start Address
                                                (1 to 16)           (1 to 16)
ECh to F4h              Reserved                N/A
*/


#if defined(CONFIG_ARCH_ZYNQ) || defined(CONFIG_X86)
# define vdma_readreg(offset)          readl(offset)
# define vdma_writereg(offset, val)    writel(val, offset)
#else
# define vdma_readreg(offset)          __raw_readl(offset)
# define vdma_writereg(offset, val)    __raw_writel(val, offset)
#endif

#define DEFAULT_HSIZE      1920
#define DEFAULT_VSIZE      1080
#define DEFAULT_VDMAWIDTH  4

/****
 * Regs Define
 ***/
/* MM2S(READ) */
#define MM2S_VDMACR            0x00        // MM2S VDMA Control Register
#define MM2S_VDMASR            0x04        // MM2S VDMA Status Register
#define MM2S_REG_INDEX         0x14        // MM2S Register Index
#define PARK_PTR_REG           0x28        // MM2S and S2MM Park Pointer Register
#define VDMA_VERSION           0x2C        // Video DMA Version Register
#define MM2S_VSIZE             0x50        // MM2S Vertical Size Register
#define MM2S_HSIZE             0x54        // MM2S Horizontal Size Register
#define MM2S_FRMDLY_STRIDE     0x58        // MM2S Frame Delay and Stride Register
#define MM2S_START_ADDRESS     0x5C        // 0x5C ~ 0x98 MM2S Start Address (1 to 16)
/* S2MM (WRITE) */
#define S2MM_VDMACR            0x30        // S2MM VDMA Control Register
#define S2MM_VDMASR            0x34        // S2MM VDMA Status Register
#define S2MM_REG_INDEX         0x44        // S2MM Register Index
#define PARK_PTR_REG           0x28        // S2MM and S2MM Park Pointer Register
#define VDMA_VERSION           0x2C        // Video DMA Version Register
#define S2MM_VSIZE             0xA0        // S2MM Vertical Size Register
#define S2MM_HSIZE             0xA4        // S2MM Horizontal Size Register
#define S2MM_FRMDLY_STRIDE     0xA8        // S2MM Frame Delay and Stride Register
#define S2MM_START_ADDRESS     0xAC        // 0x5C ~ 0x98 S2MM Start Address (1 to 16)

typedef struct XAxiVdma {
	void __iomem *regs;             /**< Memory address for this device */
        int MaxNumFrames;               /**< Number of frames to work on */
	int AddrWidth;                  /**< Address Width */
	unsigned int u32VSize;
	unsigned int u32HSize;
	unsigned int u32Width;
	unsigned int u32BufferAddr;
}stVdma, *pstVdma;

struct XAxiVdmaCtrl {
	void __iomem *regs;
	stVdma mm2s; /* Read */
	stVdma s2mm; /* Write */
};

static void alice_vdma_mm2s_init(pstVdma pvdma)
{
    if(pvdma)
    {
        printk("ALICE : MM2S init start\n");
        iowrite32(0, pvdma->regs + PARK_PTR_REG);                       // 파킹 포인터 초기화 (비파킹 모드)
        printk("ALICE : MM2S PARK_PTR_REG done\n");
        iowrite32(0x11003, pvdma->regs + MM2S_VDMACR);                  // MM2S 제어: DMA enable, circular, genlock 등
        printk("ALICE : MM2S VDMACR done\n");
        iowrite32(0x7770, pvdma->regs + MM2S_VDMASR);                   // MM2S 상태 레지스터 클리어 (에러/인터럽트 등)
        iowrite32((pvdma->u32HSize * pvdma->u32Width), pvdma->regs + MM2S_HSIZE);           // 한 라인 바이트 수(해상도*픽셀당 바이트)
        iowrite32((pvdma->u32HSize * pvdma->u32Width), pvdma->regs + MM2S_FRMDLY_STRIDE);   // 프레임 딜레이/stride(한 라인 바이트 수)
        iowrite32((pvdma->u32BufferAddr), pvdma->regs + MM2S_START_ADDRESS);                // 프레임버퍼 시작 주소
        iowrite32(0, pvdma->regs + MM2S_REG_INDEX);                      // 레지스터 인덱스(멀티프레임/멀티버퍼용, 단일=0)
        iowrite32(pvdma->u32VSize, pvdma->regs + MM2S_VSIZE);        

        // 레지스터 값 읽어서 출력
        printk("ALICE : MM2S_VDMACR = 0x%08X\n", ioread32(pvdma->regs + MM2S_VDMACR));
        printk("ALICE : MM2S_VDMASR = 0x%08X\n", ioread32(pvdma->regs + MM2S_VDMASR));
        printk("ALICE : MM2S_HSIZE = 0x%08X\n", ioread32(pvdma->regs + MM2S_HSIZE));
        printk("ALICE : MM2S_FRMDLY_STRIDE = 0x%08X\n", ioread32(pvdma->regs + MM2S_FRMDLY_STRIDE));
        printk("ALICE : MM2S_START_ADDRESS = 0x%08X\n", ioread32(pvdma->regs + MM2S_START_ADDRESS));
        printk("ALICE : MM2S_REG_INDEX = 0x%08X\n", ioread32(pvdma->regs + MM2S_REG_INDEX));
        printk("ALICE : MM2S_VSIZE = 0x%08X\n", ioread32(pvdma->regs + MM2S_VSIZE));
        printk("ALICE : Display : %X\n", pvdma->u32BufferAddr );
        printk("ALICE : MM2S init done\n");
    }
}

static void alice_vdma_s2mm_init(pstVdma pvdma)
{
	if(pvdma)
        {
		printk("ALICE : S2MM init start\n");
                iowrite32(0,                                pvdma->regs + PARK_PTR_REG);
		printk("ALICE : S2MM PARK_PTR_REG done\n");
                iowrite32(0x11003,                          pvdma->regs + S2MM_VDMACR);
		printk("ALICE : S2MM VDMACR done\n");
                iowrite32(0x7770,                           pvdma->regs + S2MM_VDMASR);
                iowrite32((pvdma->u32HSize * pvdma->u32Width),  pvdma->regs + S2MM_HSIZE);
                iowrite32((pvdma->u32HSize * pvdma->u32Width),
			      	pvdma->regs + S2MM_FRMDLY_STRIDE);
                iowrite32((pvdma->u32BufferAddr),             pvdma->regs + S2MM_START_ADDRESS);
                iowrite32(0,                                pvdma->regs + S2MM_REG_INDEX);
                iowrite32(pvdma->u32VSize,                    pvdma->regs + S2MM_VSIZE);
                printk("ALICE : Display : %X\n", pvdma->u32BufferAddr );
		printk("ALICE : S2MM init done\n");
        }
}

static struct dentry *vdma_debugfs_dir;
static struct XAxiVdmaCtrl *g_xdev;

static int vdma_status_show(struct seq_file *m, void *v)
{
    struct XAxiVdmaCtrl *xdev = g_xdev; // 전역 포인터 필요
    if (!xdev || !xdev->regs) {
        seq_puts(m, "vdma not initialized\n");
        return 0;
    }
    u32 mm2s_sr = ioread32(xdev->regs + MM2S_VDMASR);
    u32 s2mm_sr = ioread32(xdev->regs + S2MM_VDMASR);
    seq_printf(m, "MM2S_VDMASR: 0x%08X\n", mm2s_sr);
    seq_printf(m, "S2MM_VDMASR: 0x%08X\n", s2mm_sr);
    return 0;
}

static int vdma_status_open(struct inode *inode, struct file *file)
{
    return single_open(file, vdma_status_show, NULL);
}

static const struct file_operations vdma_status_fops = {
    .owner = THIS_MODULE,
    .open = vdma_status_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int alice_vdma_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
        struct XAxiVdmaCtrl *xdev;
        bool enable_mm2s = of_property_read_bool(node, "enable-mm2s");
        bool enable_s2mm = of_property_read_bool(node, "enable-s2mm");
        printk("ALICE : alice_dma_probe \n" );

        /* Allocate and initialize the DMA engine structure */
        xdev = devm_kzalloc(&pdev->dev, sizeof(*xdev), GFP_KERNEL);
        if (!xdev)
                return -ENOMEM;

        xdev->regs = devm_platform_ioremap_resource(pdev, 0);
        if (IS_ERR(xdev->regs)) {
                dev_err(&pdev->dev, "failed to ioremap memory resource\n");
                return PTR_ERR(xdev->regs);
        }
        printk("ALICE : REG : %pK\n", xdev->regs);
	xdev->s2mm.regs = xdev->regs;
	xdev->mm2s.regs = xdev->regs;
        g_xdev = xdev;

	/* Test register access - read version register first */
	{
		unsigned int ver;
		printk("ALICE : Testing VDMA register access...\n");
		ver = ioread32(xdev->regs + VDMA_VERSION);
		printk("ALICE : VDMA VERSION : 0x%08X\n", ver);
	}

        if (enable_s2mm) { // S2MM(WRITE) Config
            if (of_property_read_u32(node, "s2mm_hsize", &xdev->s2mm.u32HSize))
                xdev->s2mm.u32HSize = DEFAULT_HSIZE;
            printk("ALICE : S2MM HSIZE : %d\n", xdev->s2mm.u32HSize );

            if (of_property_read_u32(node, "s2mm_vsize", &xdev->s2mm.u32VSize))
                xdev->s2mm.u32VSize = DEFAULT_VSIZE;
            printk("ALICE : S2MM VSIZE : %d\n", xdev->s2mm.u32VSize );

            if (of_property_read_u32(node, "s2mm_width", &xdev->s2mm.u32Width))
                xdev->s2mm.u32Width = DEFAULT_VDMAWIDTH;
            printk("ALICE : S2MM WIDTH : %d\n", xdev->s2mm.u32Width );

            if (of_property_read_u32(node, "s2mm_Addr", &xdev->s2mm.u32BufferAddr))
                xdev->s2mm.u32BufferAddr = 0x0;
            printk("ALICE : S2MM BufferStart : %X\n", xdev->s2mm.u32BufferAddr );

            alice_vdma_s2mm_init(&(xdev->s2mm));
        }
        if (enable_mm2s) { // MM2S(READ) Config
            if (of_property_read_u32(node, "mm2s_hsize", &xdev->mm2s.u32HSize))
                xdev->mm2s.u32HSize = DEFAULT_HSIZE;
            printk("ALICE : MM2S HSIZE : %d\n", xdev->mm2s.u32HSize );

            if (of_property_read_u32(node, "mm2s_vsize", &xdev->mm2s.u32VSize))
                xdev->mm2s.u32VSize = DEFAULT_VSIZE;
            printk("ALICE : MM2S VSIZE : %d\n", xdev->mm2s.u32VSize );

            if (of_property_read_u32(node, "mm2s_width", &xdev->mm2s.u32Width))
                xdev->mm2s.u32Width = DEFAULT_VDMAWIDTH;
            printk("ALICE : MM2S WIDTH : %d\n", xdev->mm2s.u32Width );

            if (of_property_read_u32(node, "mm2s_Addr", &xdev->mm2s.u32BufferAddr))
                xdev->mm2s.u32BufferAddr = 0x0;
            printk("ALICE : MM2S BufferStart : %X\n", xdev->mm2s.u32BufferAddr );

            alice_vdma_mm2s_init(&(xdev->mm2s));
        }
       
       
       
       
        if (!vdma_debugfs_dir)
            vdma_debugfs_dir = debugfs_create_dir("alice_vdma", NULL);
        debugfs_create_file("vdma_status", 0444, vdma_debugfs_dir, NULL, &vdma_status_fops);
        return 0;
}


static int alice_vdma_remove(struct platform_device *pdev)
{
    debugfs_remove_recursive(vdma_debugfs_dir);
    vdma_debugfs_dir = NULL;
    return 0;
}

static const struct of_device_id alice_vdma_of_match[] = {
        { .compatible = "alice,vdma", },
        { /* end of list */ },
};

static struct platform_driver alice_vdma_driver = {
        .driver = {
                .name = "alice-vdma",
                .of_match_table = alice_vdma_of_match,
        },
        .probe = alice_vdma_probe,
        .remove = alice_vdma_remove,
};

module_platform_driver(alice_vdma_driver);

MODULE_AUTHOR("Alice, Inc.");
MODULE_DESCRIPTION("Alice VDMA driver");
MODULE_LICENSE("GPL v2");

