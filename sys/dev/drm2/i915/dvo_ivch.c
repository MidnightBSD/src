/*
 * Copyright © 2006 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include <sys/cdefs.h>

#include "dvo.h"

/*
 * register definitions for the i82807aa.
 *
 * Documentation on this chipset can be found in datasheet #29069001 at
 * intel.com.
 */

/*
 * VCH Revision & GMBus Base Addr
 */
#define VR00		0x00
# define VR00_BASE_ADDRESS_MASK		0x007f

/*
 * Functionality Enable
 */
#define VR01		0x01

/*
 * Enable the panel fitter
 */
# define VR01_PANEL_FIT_ENABLE		(1 << 3)
/*
 * Enables the LCD display.
 *
 * This must not be set while VR01_DVO_BYPASS_ENABLE is set.
 */
# define VR01_LCD_ENABLE		(1 << 2)
/** Enables the DVO repeater. */
# define VR01_DVO_BYPASS_ENABLE		(1 << 1)
/** Enables the DVO clock */
# define VR01_DVO_ENABLE		(1 << 0)

/*
 * LCD Interface Format
 */
#define VR10		0x10
/** Enables LVDS output instead of CMOS */
# define VR10_LVDS_ENABLE		(1 << 4)
/** Enables 18-bit LVDS output. */
# define VR10_INTERFACE_1X18		(0 << 2)
/** Enables 24-bit LVDS or CMOS output */
# define VR10_INTERFACE_1X24		(1 << 2)
/** Enables 2x18-bit LVDS or CMOS output. */
# define VR10_INTERFACE_2X18		(2 << 2)
/** Enables 2x24-bit LVDS output */
# define VR10_INTERFACE_2X24		(3 << 2)

/*
 * VR20 LCD Horizontal Display Size
 */
#define VR20	0x20

/*
 * LCD Vertical Display Size
 */
#define VR21	0x20

/*
 * Panel power down status
 */
#define VR30		0x30
/** Read only bit indicating that the panel is not in a safe poweroff state. */
# define VR30_PANEL_ON			(1 << 15)

#define VR40		0x40
# define VR40_STALL_ENABLE		(1 << 13)
# define VR40_VERTICAL_INTERP_ENABLE	(1 << 12)
# define VR40_ENHANCED_PANEL_FITTING	(1 << 11)
# define VR40_HORIZONTAL_INTERP_ENABLE	(1 << 10)
# define VR40_AUTO_RATIO_ENABLE		(1 << 9)
# define VR40_CLOCK_GATING_ENABLE	(1 << 8)

/*
 * Panel Fitting Vertical Ratio
 * (((image_height - 1) << 16) / ((panel_height - 1))) >> 2
 */
#define VR41		0x41

/*
 * Panel Fitting Horizontal Ratio
 * (((image_width - 1) << 16) / ((panel_width - 1))) >> 2
 */
#define VR42		0x42

/*
 * Horizontal Image Size
 */
#define VR43		0x43

/* VR80 GPIO 0
 */
#define VR80	    0x80
#define VR81	    0x81
#define VR82	    0x82
#define VR83	    0x83
#define VR84	    0x84
#define VR85	    0x85
#define VR86	    0x86
#define VR87	    0x87

/* VR88 GPIO 8
 */
#define VR88	    0x88

/* Graphics BIOS scratch 0
 */
#define VR8E	    0x8E
# define VR8E_PANEL_TYPE_MASK		(0xf << 0)
# define VR8E_PANEL_INTERFACE_CMOS	(0 << 4)
# define VR8E_PANEL_INTERFACE_LVDS	(1 << 4)
# define VR8E_FORCE_DEFAULT_PANEL	(1 << 5)

/* Graphics BIOS scratch 1
 */
#define VR8F	    0x8F
# define VR8F_VCH_PRESENT		(1 << 0)
# define VR8F_DISPLAY_CONN		(1 << 1)
# define VR8F_POWER_MASK		(0x3c)
# define VR8F_POWER_POS			(2)


struct ivch_priv {
	bool quiet;

	uint16_t width, height;
};


static void ivch_dump_regs(struct intel_dvo_device *dvo);

/**
 * Reads a register on the ivch.
 *
 * Each of the 256 registers are 16 bits long.
 */
static bool ivch_read(struct intel_dvo_device *dvo, int addr, uint16_t *data)
{
	struct ivch_priv *priv = dvo->dev_priv;
	device_t adapter = dvo->i2c_bus;
	u8 out_buf[1];
	u8 in_buf[2];

	struct iic_msg msgs[] = {
		{
			.slave = dvo->slave_addr << 1,
			.flags = I2C_M_RD,
			.len = 0,
		},
		{
			.slave = 0 << 1,
			.flags = I2C_M_NOSTART,
			.len = 1,
			.buf = out_buf,
		},
		{
			.slave = dvo->slave_addr << 1,
			.flags = I2C_M_RD | I2C_M_NOSTART,
			.len = 2,
			.buf = in_buf,
		}
	};

	out_buf[0] = addr;

	if (-iicbus_transfer(adapter, msgs, 3) == 0) {
		*data = (in_buf[1] << 8) | in_buf[0];
		return true;
	}

	if (!priv->quiet) {
		DRM_DEBUG_KMS("Unable to read register 0x%02x from "
				"%s:%02x.\n",
			  addr, device_get_nameunit(adapter), dvo->slave_addr);
	}
	return false;
}

/** Writes a 16-bit register on the ivch */
static bool ivch_write(struct intel_dvo_device *dvo, int addr, uint16_t data)
{
	struct ivch_priv *priv = dvo->dev_priv;
	device_t adapter = dvo->i2c_bus;
	u8 out_buf[3];
	struct iic_msg msg = {
		.slave = dvo->slave_addr << 1,
		.flags = 0,
		.len = 3,
		.buf = out_buf,
	};

	out_buf[0] = addr;
	out_buf[1] = data & 0xff;
	out_buf[2] = data >> 8;

	if (-iicbus_transfer(adapter, &msg, 1) == 0)
		return true;

	if (!priv->quiet) {
		DRM_DEBUG_KMS("Unable to write register 0x%02x to %s:%d.\n",
			  addr, device_get_nameunit(adapter), dvo->slave_addr);
	}

	return false;
}

/** Probes the given bus and slave address for an ivch */
static bool ivch_init(struct intel_dvo_device *dvo,
		      device_t adapter)
{
	struct ivch_priv *priv;
	uint16_t temp;

	priv = malloc(sizeof(struct ivch_priv), DRM_MEM_KMS, M_NOWAIT | M_ZERO);
	if (priv == NULL)
		return false;

	dvo->i2c_bus = adapter;
	dvo->dev_priv = priv;
	priv->quiet = true;

	if (!ivch_read(dvo, VR00, &temp))
		goto out;
	priv->quiet = false;

	/* Since the identification bits are probably zeroes, which doesn't seem
	 * very unique, check that the value in the base address field matches
	 * the address it's responding on.
	 */
	if ((temp & VR00_BASE_ADDRESS_MASK) != dvo->slave_addr) {
		DRM_DEBUG_KMS("ivch detect failed due to address mismatch "
			  "(%d vs %d)\n",
			  (temp & VR00_BASE_ADDRESS_MASK), dvo->slave_addr);
		goto out;
	}

	ivch_read(dvo, VR20, &priv->width);
	ivch_read(dvo, VR21, &priv->height);

	return true;

out:
	free(priv, DRM_MEM_KMS);
	return false;
}

static enum drm_connector_status ivch_detect(struct intel_dvo_device *dvo)
{
	return connector_status_connected;
}

static enum drm_mode_status ivch_mode_valid(struct intel_dvo_device *dvo,
					    struct drm_display_mode *mode)
{
	if (mode->clock > 112000)
		return MODE_CLOCK_HIGH;

	return MODE_OK;
}

/** Sets the power state of the panel connected to the ivch */
static void ivch_dpms(struct intel_dvo_device *dvo, bool enable)
{
	int i;
	uint16_t vr01, vr30, backlight;

	/* Set the new power state of the panel. */
	if (!ivch_read(dvo, VR01, &vr01))
		return;

	if (enable)
		backlight = 1;
	else
		backlight = 0;
	ivch_write(dvo, VR80, backlight);

	if (enable)
		vr01 |= VR01_LCD_ENABLE | VR01_DVO_ENABLE;
	else
		vr01 &= ~(VR01_LCD_ENABLE | VR01_DVO_ENABLE);

	ivch_write(dvo, VR01, vr01);

	/* Wait for the panel to make its state transition */
	for (i = 0; i < 100; i++) {
		if (!ivch_read(dvo, VR30, &vr30))
			break;

		if (((vr30 & VR30_PANEL_ON) != 0) == enable)
			break;
		udelay(1000);
	}
	/* wait some more; vch may fail to resync sometimes without this */
	udelay(16 * 1000);
}

static bool ivch_get_hw_state(struct intel_dvo_device *dvo)
{
	uint16_t vr01;

	/* Set the new power state of the panel. */
	if (!ivch_read(dvo, VR01, &vr01))
		return false;

	if (vr01 & VR01_LCD_ENABLE)
		return true;
	else
		return false;
}

static void ivch_mode_set(struct intel_dvo_device *dvo,
			  struct drm_display_mode *mode,
			  struct drm_display_mode *adjusted_mode)
{
	uint16_t vr40 = 0;
	uint16_t vr01;

	vr01 = 0;
	vr40 = (VR40_STALL_ENABLE | VR40_VERTICAL_INTERP_ENABLE |
		VR40_HORIZONTAL_INTERP_ENABLE);

	if (mode->hdisplay != adjusted_mode->hdisplay ||
	    mode->vdisplay != adjusted_mode->vdisplay) {
		uint16_t x_ratio, y_ratio;

		vr01 |= VR01_PANEL_FIT_ENABLE;
		vr40 |= VR40_CLOCK_GATING_ENABLE;
		x_ratio = (((mode->hdisplay - 1) << 16) /
			   (adjusted_mode->hdisplay - 1)) >> 2;
		y_ratio = (((mode->vdisplay - 1) << 16) /
			   (adjusted_mode->vdisplay - 1)) >> 2;
		ivch_write(dvo, VR42, x_ratio);
		ivch_write(dvo, VR41, y_ratio);
	} else {
		vr01 &= ~VR01_PANEL_FIT_ENABLE;
		vr40 &= ~VR40_CLOCK_GATING_ENABLE;
	}
	vr40 &= ~VR40_AUTO_RATIO_ENABLE;

	ivch_write(dvo, VR01, vr01);
	ivch_write(dvo, VR40, vr40);

	ivch_dump_regs(dvo);
}

static void ivch_dump_regs(struct intel_dvo_device *dvo)
{
	uint16_t val;

	ivch_read(dvo, VR00, &val);
	DRM_LOG_KMS("VR00: 0x%04x\n", val);
	ivch_read(dvo, VR01, &val);
	DRM_LOG_KMS("VR01: 0x%04x\n", val);
	ivch_read(dvo, VR30, &val);
	DRM_LOG_KMS("VR30: 0x%04x\n", val);
	ivch_read(dvo, VR40, &val);
	DRM_LOG_KMS("VR40: 0x%04x\n", val);

	/* GPIO registers */
	ivch_read(dvo, VR80, &val);
	DRM_LOG_KMS("VR80: 0x%04x\n", val);
	ivch_read(dvo, VR81, &val);
	DRM_LOG_KMS("VR81: 0x%04x\n", val);
	ivch_read(dvo, VR82, &val);
	DRM_LOG_KMS("VR82: 0x%04x\n", val);
	ivch_read(dvo, VR83, &val);
	DRM_LOG_KMS("VR83: 0x%04x\n", val);
	ivch_read(dvo, VR84, &val);
	DRM_LOG_KMS("VR84: 0x%04x\n", val);
	ivch_read(dvo, VR85, &val);
	DRM_LOG_KMS("VR85: 0x%04x\n", val);
	ivch_read(dvo, VR86, &val);
	DRM_LOG_KMS("VR86: 0x%04x\n", val);
	ivch_read(dvo, VR87, &val);
	DRM_LOG_KMS("VR87: 0x%04x\n", val);
	ivch_read(dvo, VR88, &val);
	DRM_LOG_KMS("VR88: 0x%04x\n", val);

	/* Scratch register 0 - AIM Panel type */
	ivch_read(dvo, VR8E, &val);
	DRM_LOG_KMS("VR8E: 0x%04x\n", val);

	/* Scratch register 1 - Status register */
	ivch_read(dvo, VR8F, &val);
	DRM_LOG_KMS("VR8F: 0x%04x\n", val);
}

static void ivch_destroy(struct intel_dvo_device *dvo)
{
	struct ivch_priv *priv = dvo->dev_priv;

	if (priv) {
		free(priv, DRM_MEM_KMS);
		dvo->dev_priv = NULL;
	}
}

struct intel_dvo_dev_ops ivch_ops = {
	.init = ivch_init,
	.dpms = ivch_dpms,
	.get_hw_state = ivch_get_hw_state,
	.mode_valid = ivch_mode_valid,
	.mode_set = ivch_mode_set,
	.detect = ivch_detect,
	.dump_regs = ivch_dump_regs,
	.destroy = ivch_destroy,
};
