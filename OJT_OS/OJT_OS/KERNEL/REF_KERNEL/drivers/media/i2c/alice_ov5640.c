/*
 * &i2c1 {
 *	ov5640: camera@3c {
 *		compatible = "alice,ov5640";
 *		reg = <0x3c>;
 *		clocks = <&clks IMX6QDL_CLK_CKO>;
 *		clock-names = "xclk";
 *		mode = <2>;    // 0 - 720P 60fps
 *		               // 1 - 1080p 15fps
 *		               // 2 - 1080p 30fps    <*>
 *                             // 3 - 1080p 30fps 336M MIPI
 *                             // 4 - 1080p 30fps 336M 1LANE MIPI
 *		awb = <2>;     // 0 - DISABLE
 *		               // 1 - SIMPLE
 *                             // 2 - ADVANCED        <*>
 *		powerdown-gpios = <&gpio1 19 GPIO_ACTIVE_HIGH>;
 *		reset-gpios = <&gpio1 20 GPIO_ACTIVE_LOW>;
 *		rotation = <180>;
 *	};
 * };
 *
 */
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

#define OV5640_ID_H             0x56
#define OV5640_ID_L             0x40

#define SIZEOF_ARRAY(x) sizeof(x)/sizeof(x[0])
#define MAP_ENUM_TO_CFG(en, cfg) en, cfg, SIZEOF_ARRAY(cfg)

enum _mode_t_ {
	 MODE_720P_1280_720_60fps = 0,
	 MODE_1080P_1920_1080_15fps,
	 MODE_1080P_1920_1080_30fps,
	 MODE_1080P_1920_1080_30fps_336M_MIPI,
	 MODE_1080P_1920_1080_30fps_336M_1LANE_MIPI,
	 MODE_END
};

enum _awb_t_{
 	AWB_DISABLED = 0,
 	AWB_SIMPLE,
 	AWB_ADVANCED,
 	AWB_END
};

typedef struct {
	uint16_t addr;
	uint8_t data;
} config_word_t;

typedef struct {
	int mode;
	config_word_t const* cfg;
	size_t cfg_size;
}config_modes_t;

typedef struct {
	int awb;
	config_word_t const* cfg;
	size_t cfg_size;
}config_awb_t;

config_word_t const cfg_advanced_awb_[] =
{
	// Enable Advanced AWB
	{0x3406 ,0x00}, {0x5192 ,0x04}, {0x5191 ,0xf8}, {0x518d ,0x26},
	{0x518f ,0x42},	{0x518e ,0x2b},	{0x5190 ,0x42},	{0x518b ,0xd0},
	{0x518c ,0xbd},	{0x5187 ,0x18},	{0x5188 ,0x18},	{0x5189 ,0x56},
	{0x518a ,0x5c},	{0x5186 ,0x1c},	{0x5181 ,0x50},	{0x5184 ,0x20},
	{0x5182 ,0x11},	{0x5183 ,0x00},	{0x5001 ,0x03}
};

config_word_t const cfg_simple_awb_[] =
{
	// Disable Advanced AWB
	{0x518d ,0x00},	{0x518f ,0x20},	{0x518e ,0x00},	{0x5190 ,0x20},
	{0x518b ,0x00},	{0x518c ,0x00},	{0x5187 ,0x10},	{0x5188 ,0x10},
	{0x5189 ,0x40},	{0x518a ,0x40},	{0x5186 ,0x10},	{0x5181 ,0x58},
	{0x5184 ,0x25},	{0x5182 ,0x11},
	// Enable simple AWB
	{0x3406 ,0x00},	{0x5183 ,0x80},	{0x5191 ,0xff},	{0x5192 ,0x00},
	{0x5001 ,0x03}
};

config_word_t const cfg_disable_awb_[] =
{
	{0x5001 ,0x02}
};

config_word_t const cfg_720p_60fps_[] =
{
	{0x3035, 0x21}, {0x3036, 0x46},	{0x3037, 0x05},	{0x3108, 0x11},
	{0x3034, 0x1A},                 {0x3800, (0 >> 8) & 0x0F},
       	{0x3801, 0 & 0xFF},             {0x3802, (8 >> 8) & 0x07},
       	{0x3803, 8 & 0xFF},             {0x3804, (2619 >> 8) & 0x0F}, 
	{0x3805, 2619 & 0xFF},          {0x3806, (1947 >> 8) & 0x07},
	{0x3807, 1947 & 0xFF},          {0x3810, (0 >> 8) & 0x0F},
	{0x3811, 0 & 0xFF},             {0x3812, (0 >> 8) & 0x07},
	{0x3813, 0 & 0xFF},             {0x3808, (1280 >> 8) & 0x0F},
	{0x3809, 1280 & 0xFF},          {0x380a, (720 >> 8) & 0x7F},
	{0x380b, 720 & 0xFF},           {0x380c, (1896 >> 8) & 0x1F},
	{0x380d, 1896 & 0xFF},          {0x380e, (984 >> 8) & 0xFF},
	{0x380f, 984 & 0xFF},           {0x3814, 0x31},
	{0x3815, 0x31},                 {0x3821, 0x01},
	{0x4837, 36},                   {0x3618, 0x00},
	{0x3612, 0x59},                 {0x3708, 0x64},
	{0x3709, 0x52},                 {0x370c, 0x03},
	{0x4300, 0x00},                 {0x501f, 0x03}
};

config_word_t const cfg_1080p_15fps_[] =
{
	{0x3035, 0x41},                 {0x3036, 0x69},
	{0x3037, 0x05},                 {0x3108, 0x11},
	{0x3034, 0x1A},                 {0x3800, (336 >> 8) & 0x0F},
	{0x3801, 336 & 0xFF},           {0x3802, (426 >> 8) & 0x07},
	{0x3803, 426 & 0xFF},           {0x3804, (2287 >> 8) & 0x0F},
	{0x3805, 2287 & 0xFF},          {0x3806, (1529 >> 8) & 0x07},
	{0x3807, 1529 & 0xFF},          {0x3810, (16 >> 8) & 0x0F},
	{0x3811, 16 & 0xFF},            {0x3812, (12 >> 8) & 0x07},
	{0x3813, 12 & 0xFF},            {0x3808, (1920 >> 8) & 0x0F},
	{0x3809, 1920 & 0xFF},          {0x380a, (1080 >> 8) & 0x7F},
	{0x380b, 1080 & 0xFF},          {0x380c, (2500 >> 8) & 0x1F},
	{0x380d, 2500 & 0xFF},          {0x380e, (1120 >> 8) & 0xFF},
	{0x380f, 1120 & 0xFF},          {0x3814, 0x11},
	{0x3815, 0x11},                 {0x3821, 0x00},
	{0x4837, 48},                   {0x3618, 0x00},
	{0x3612, 0x59},                 {0x3708, 0x64},
	{0x3709, 0x52},                 {0x370c, 0x03},
	{0x4300, 0x00},                 {0x501f, 0x03}
};

config_word_t const cfg_1080p_30fps_[] =
{
	{0x3035, 0x21},                 {0x3036, 0x69},
	{0x3037, 0x05},                 {0x3108, 0x11},
	{0x3034, 0x1A},                 {0x3800, (336 >> 8) & 0x0F},
	{0x3801, 336 & 0xFF},           {0x3802, (426 >> 8) & 0x07},
	{0x3803, 426 & 0xFF},           {0x3804, (2287 >> 8) & 0x0F},
	{0x3805, 2287 & 0xFF},          {0x3806, (1529 >> 8) & 0x07},
	{0x3807, 1529 & 0xFF},          {0x3810, (16 >> 8) & 0x0F},
	{0x3811, 16 & 0xFF},            {0x3812, (12 >> 8) & 0x07},
	{0x3813, 12 & 0xFF},            {0x3808, (1920 >> 8) & 0x0F},
	{0x3809, 1920 & 0xFF},          {0x380a, (1080 >> 8) & 0x7F},
	{0x380b, 1080 & 0xFF},          {0x380c, (2500 >> 8) & 0x1F},
	{0x380d, 2500 & 0xFF},          {0x380e, (1120 >> 8) & 0xFF},
	{0x380f, 1120 & 0xFF},          {0x3814, 0x11},
	{0x3815, 0x11},                 {0x3821, 0x00},
	{0x4837, 24},                   {0x3618, 0x00},
	{0x3612, 0x59},                 {0x3708, 0x64},
	{0x3709, 0x52},                 {0x370c, 0x03},
	{0x4300, 0x00},                 {0x501f, 0x03}
};

config_word_t const cfg_1080p_30fps_336M_mipi_[] =
{
	{0x3035, 0x11},                 {0x3036, 0x54},
	{0x3037, 0x15},                 {0x3108, 0x01},
	{0x3034, 0x1A},                 {0x3800, (336 >> 8) & 0x0F},
	{0x3801, 336 & 0xFF},           {0x3802, (426 >> 8) & 0x07},
	{0x3803, 426 & 0xFF},           {0x3804, (2287 >> 8) & 0x0F},
	{0x3805, 2287 & 0xFF},          {0x3806, (1529 >> 8) & 0x07},
	{0x3807, 1529 & 0xFF},          {0x3810, (16 >> 8) & 0x0F},
	{0x3811, 16 & 0xFF},            {0x3812, (12 >> 8) & 0x07},
	{0x3813, 12 & 0xFF},            {0x3808, (1920 >> 8) & 0x0F},
	{0x3809, 1920 & 0xFF},          {0x380a, (1080 >> 8) & 0x7F},
	{0x380b, 1080 & 0xFF},          {0x380c, (2500 >> 8) & 0x1F},
	{0x380d, 2500 & 0xFF},          {0x380e, (1120 >> 8) & 0xFF},
	{0x380f, 1120 & 0xFF},          {0x3814, 0x11},
	{0x3815, 0x11},                 {0x3821, 0x00},
	{0x4837, 14},                   {0x3618, 0x00},
	{0x3612, 0x59},                 {0x3708, 0x64},
	{0x3709, 0x52},                 {0x370c, 0x03},
	{0x4300, 0x00},                 {0x501f, 0x03}
};

config_word_t const cfg_1080p_30fps_336M_1lane_mipi_[] =
{
	{0x3035, 0x11},                 {0x3036, 0x54},
	{0x3037, 0x15},                 {0x3108, 0x01},
	{0x3034, 0x1A},                 {0x300e, 0x25},
	{0x3800, (336 >> 8) & 0x0F},    {0x3801, 336 & 0xFF},
	{0x3802, (426 >> 8) & 0x07},    {0x3803, 426 & 0xFF},
	{0x3804, (2287 >> 8) & 0x0F},   {0x3805, 2287 & 0xFF},
	{0x3806, (1529 >> 8) & 0x07},   {0x3807, 1529 & 0xFF},
	{0x3810, (16 >> 8) & 0x0F},     {0x3811, 16 & 0xFF},
	{0x3812, (12 >> 8) & 0x07},     {0x3813, 12 & 0xFF},
	{0x3808, (1920 >> 8) & 0x0F},   {0x3809, 1920 & 0xFF},
	{0x380a, (1080 >> 8) & 0x7F},   {0x380b, 1080 & 0xFF},
	{0x380c, (2500 >> 8) & 0x1F},   {0x380d, 2500 & 0xFF},
	{0x380e, (1120 >> 8) & 0xFF},   {0x380f, 1120 & 0xFF},
	{0x3814, 0x11},                 {0x3815, 0x11},
	{0x3821, 0x00},                 {0x4837, 28},
	{0x3618, 0x00},                 {0x3612, 0x59},
	{0x3708, 0x64},                 {0x3709, 0x52},
	{0x370c, 0x03},                 {0x4300, 0x00},
	{0x501f, 0x03}
};

config_word_t const cfg_init_[] =
{
	{0x3008, 0x42}, {0x3103, 0x03}, {0x3017, 0x00}, {0x3018, 0x00},
	{0x3034, 0x18}, {0x3035, 0x11}, {0x3036, 0x38}, {0x3037, 0x11},
	{0x3108, 0x01}, {0x303D, 0x10}, {0x303B, 0x19}, {0x3630, 0x2e},
	{0x3631, 0x0e}, {0x3632, 0xe2}, {0x3633, 0x23}, {0x3621, 0xe0},
	{0x3704, 0xa0}, {0x3703, 0x5a}, {0x3715, 0x78}, {0x3717, 0x01},
	{0x370b, 0x60}, {0x3705, 0x1a}, {0x3905, 0x02}, {0x3906, 0x10},
	{0x3901, 0x0a}, {0x3731, 0x02}, {0x3600, 0x37}, {0x3601, 0x33},
	{0x302d, 0x60}, {0x3620, 0x52}, {0x371b, 0x20}, {0x471c, 0x50},
	{0x3a13, 0x43}, {0x3a18, 0x00}, {0x3a19, 0xf8}, {0x3635, 0x13},
	{0x3636, 0x06}, {0x3634, 0x44}, {0x3622, 0x01}, {0x3c01, 0x34},
	{0x3c04, 0x28}, {0x3c05, 0x98}, {0x3c06, 0x00}, {0x3c07, 0x08},
	{0x3c08, 0x00}, {0x3c09, 0x1c}, {0x3c0a, 0x9c}, {0x3c0b, 0x40},
	{0x503d, 0x00}, {0x3820, 0x46}, {0x300e, 0x45}, {0x4800, 0x14},
	{0x302e, 0x08}, {0x4300, 0x6f}, {0x501f, 0x01}, {0x4713, 0x03},
	{0x4407, 0x04}, {0x440e, 0x00}, {0x460b, 0x35}, {0x460c, 0x20},
	{0x3824, 0x01}, {0x5000, 0x07}, {0x5001, 0x03}
};

config_modes_t const modes[] =
{
	{ MAP_ENUM_TO_CFG(MODE_720P_1280_720_60fps, cfg_720p_60fps_) },
	{ MAP_ENUM_TO_CFG(MODE_1080P_1920_1080_15fps, cfg_1080p_15fps_) },
	{ MAP_ENUM_TO_CFG(MODE_1080P_1920_1080_30fps, cfg_1080p_30fps_), },
	{ MAP_ENUM_TO_CFG(MODE_1080P_1920_1080_30fps_336M_MIPI, cfg_1080p_30fps_336M_mipi_) },
	{ MAP_ENUM_TO_CFG(MODE_1080P_1920_1080_30fps_336M_1LANE_MIPI, cfg_1080p_30fps_336M_1lane_mipi_) },
};

config_awb_t const awbs[] =
{
	{ MAP_ENUM_TO_CFG(AWB_DISABLED, cfg_disable_awb_) },
	{ MAP_ENUM_TO_CFG(AWB_SIMPLE, cfg_simple_awb_) },
	{ MAP_ENUM_TO_CFG(AWB_ADVANCED, cfg_advanced_awb_) }
};

struct alice_ov5640_dev {
	struct i2c_client *i2c_client;
	struct gpio_desc *reset_gpio;
        struct gpio_desc *pwdn_gpio;
	struct mutex lock;
	config_modes_t * cfg_mode;
	config_awb_t * cfg_awb;
	struct clk *xclk;
	u32 xclk_rfeq;
	u32 mode;
	u32 awb;
	u8 id_h;
	u8 id_l;
};

static int alice_ov5640_write_reg(struct alice_ov5640_dev *sensor, u16 reg, u8 val)
{
	struct i2c_client * client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[3];
	int ret;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if(ret < 0)
	{
		dev_err(&client->dev, "%s: error: reg=%x, val=%x\n",
                        __func__, reg, val);
                return ret;
	}
	return 0;
}

static int alice_ov5640_read_reg(struct alice_ov5640_dev *sensor, u16 reg, u8 *val)
{
        struct i2c_client *client = sensor->i2c_client;
        struct i2c_msg msg[2];
        u8 buf[2];
        int ret;

        buf[0] = reg >> 8;
        buf[1] = reg & 0xff;

        msg[0].addr = client->addr;
        msg[0].flags = client->flags;
        msg[0].buf = buf;
        msg[0].len = sizeof(buf);

        msg[1].addr = client->addr;
        msg[1].flags = client->flags | I2C_M_RD;
        msg[1].buf = buf;
        msg[1].len = 1;

        ret = i2c_transfer(client->adapter, msg, 2);
        if (ret < 0) {
                dev_err(&client->dev, "%s: error: reg=%x\n",
                        __func__, reg);
                return ret;
        }

        *val = buf[0];
        return 0;
}

static int alice_ov5640_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct alice_ov5640_dev * sensor;
	int retry = 10;
	int i=0;

	printk("ALICE OV5640 probe \n");
	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if(!sensor)
		return -ENOMEM;

	sensor->i2c_client = client;
	
	sensor->pwdn_gpio = devm_gpiod_get_optional(dev, "powerdown",
	                                            GPIOD_OUT_HIGH);
	sensor->reset_gpio = devm_gpiod_get_optional(dev, "reset", 
			                             GPIOD_OUT_HIGH);

//	if (of_property_read_u32(node, "mode", &sensor->mode))
                sensor->mode = MODE_1080P_1920_1080_30fps;
        printk("ALICE : mode : %d\n", sensor->mode );
//	if (of_property_read_u32(node, "awb", &sensor->awb))
                sensor->awb = AWB_ADVANCED;
        printk("ALICE : awb : %d\n", sensor->awb );

	sensor->cfg_mode = &modes[sensor->mode];
	sensor->cfg_awb = &awbs[sensor->awb];
	
	/*
	 *  Init ov5640
	 */

	/* ID CHECK */
	while(retry){
		alice_ov5640_read_reg(sensor, 0x300A, &sensor->id_h);
		alice_ov5640_read_reg(sensor, 0x300B, &sensor->id_l);
		printk("ALICE OV5640 : %02X:%02X\n", sensor->id_h, sensor->id_l);
		if(OV5640_ID_H != sensor->id_h && OV5640_ID_L != sensor->id_l)
		{
			udelay(100);
			retry--;
		}
		else
		{
			break;
		}
	}

	if(retry)
	{
		/* [1]=0 System input clock from pad; Default read = 0x11 */
		alice_ov5640_write_reg(sensor, 0x3101, 0x11);
		/* [1]=0 System input clock from pad; Default read = 0x11 */
		alice_ov5640_write_reg(sensor, 0x3008, 0x82);

		udelay(1000);
		for(i=0; i<sizeof(cfg_init_)/sizeof(cfg_init_[0]); i++)
		{
			alice_ov5640_write_reg(sensor, 
					cfg_init_[i].addr,
				       	cfg_init_[i].data);
		}

		/* [7]=0 Software reset; [6]=1 Software power down; Default=0x02 */
		alice_ov5640_write_reg(sensor, 0x3008, 0x42);

		/* mode config */
		for(i=0; i<sensor->cfg_mode->cfg_size; i++)
		{
			alice_ov5640_write_reg(sensor, 
					sensor->cfg_mode->cfg[i].addr, 
					sensor->cfg_mode->cfg[i].data);
		}

		/* [7]=0 Software reset; [6]=0 Software power down; Default=0x02 */
		alice_ov5640_write_reg(sensor, 0x3008, 0x02);

		/* [7]=0 Software reset; [6]=1 Software power down; Default=0x02 */
		alice_ov5640_write_reg(sensor, 0x3008, 0x42);

		/* awb config */
		for(i=0; i<sensor->cfg_awb->cfg_size; i++)
                {
                        alice_ov5640_write_reg(sensor,
                                        sensor->cfg_awb->cfg[i].addr,
                                        sensor->cfg_awb->cfg[i].data);
                }

		/* [7]=0 Software reset; [6]=0 Software power down; Default=0x02 */
		alice_ov5640_write_reg(sensor, 0x3008, 0x02);

	}

	return 0;
}

static void alice_ov5640_remove(struct i2c_client *client)
{
}

static const struct i2c_device_id alice_ov5640_id[] = {
	{"alice_ov5640", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, alice_ov5640_id);

static const struct of_device_id alice_ov5640_dt_ids[] = {
	{ .compatible = "alice,ov5640" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, alice_ov5640_dt_ids);

static struct i2c_driver alice_ov5640_i2c_driver = {
	.driver = {
		.name = "alice_ov5640",
		.of_match_table = alice_ov5640_dt_ids,
	},
	.id_table = alice_ov5640_id,
	.probe_new = alice_ov5640_probe,
	.remove = alice_ov5640_remove,
};

module_i2c_driver(alice_ov5640_i2c_driver);

MODULE_AUTHOR("Alice, Inc.");
MODULE_DESCRIPTION("Alice OV5640 driver");
MODULE_LICENSE("GPL v2");
