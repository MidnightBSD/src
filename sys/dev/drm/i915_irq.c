/* i915_irq.c -- IRQ support for the I915 -*- linux-c -*-
 */
/*-
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/drm/i915_irq.c,v 1.1.2.1 2005/12/14 00:52:58 anholt Exp $");

#include "dev/drm/drmP.h"
#include "dev/drm/drm.h"
#include "dev/drm/i915_drm.h"
#include "dev/drm/i915_drv.h"

#define USER_INT_FLAG 0x2
#define MAX_NOPID ((u32)~0)
#define READ_BREADCRUMB(dev_priv)  (((u32*)(dev_priv->hw_status_page))[5])

irqreturn_t i915_driver_irq_handler(DRM_IRQ_ARGS)
{
	drm_device_t *dev = (drm_device_t *) arg;
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	u16 temp;

	temp = I915_READ16(I915REG_INT_IDENTITY_R);
	temp &= USER_INT_FLAG;

	DRM_DEBUG("%s flag=%08x\n", __FUNCTION__, temp);

	if (temp == 0)
		return IRQ_NONE;

	I915_WRITE16(I915REG_INT_IDENTITY_R, temp);
	DRM_WAKEUP(&dev_priv->irq_queue);

	return IRQ_HANDLED;
}

static int i915_emit_irq(drm_device_t * dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 ret;
	RING_LOCALS;

	i915_kernel_lost_context(dev);

	DRM_DEBUG("%s\n", __FUNCTION__);

	ret = dev_priv->counter;

	BEGIN_LP_RING(2);
	OUT_RING(0);
	OUT_RING(GFX_OP_USER_INTERRUPT);
	ADVANCE_LP_RING();

	return ret;
}

static int i915_wait_irq(drm_device_t * dev, int irq_nr)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	int ret = 0;

	DRM_DEBUG("%s irq_nr=%d breadcrumb=%d\n", __FUNCTION__, irq_nr,
		  READ_BREADCRUMB(dev_priv));

	if (READ_BREADCRUMB(dev_priv) >= irq_nr)
		return 0;

	dev_priv->sarea_priv->perf_boxes |= I915_BOX_WAIT;

	DRM_WAIT_ON(ret, dev_priv->irq_queue, 3 * DRM_HZ,
		    READ_BREADCRUMB(dev_priv) >= irq_nr);

	if (ret == DRM_ERR(EBUSY)) {
		DRM_ERROR("%s: EBUSY -- rec: %d emitted: %d\n",
			  __FUNCTION__,
			  READ_BREADCRUMB(dev_priv), (int)dev_priv->counter);
	}

	dev_priv->sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);
	return ret;
}

/* Needs the lock as it touches the ring.
 */
int i915_irq_emit(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_irq_emit_t emit;
	int result;

	LOCK_TEST_WITH_RETURN(dev, filp);

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL(emit, (drm_i915_irq_emit_t __user *) data,
				 sizeof(emit));

	result = i915_emit_irq(dev);

	if (DRM_COPY_TO_USER(emit.irq_seq, &result, sizeof(int))) {
		DRM_ERROR("copy_to_user\n");
		return DRM_ERR(EFAULT);
	}

	return 0;
}

/* Doesn't need the hardware lock.
 */
int i915_irq_wait(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_irq_wait_t irqwait;

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL(irqwait, (drm_i915_irq_wait_t __user *) data,
				 sizeof(irqwait));

	return i915_wait_irq(dev, irqwait.irq_seq);
}

/* drm_dma.h hooks
*/
void i915_driver_irq_preinstall(drm_device_t * dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	I915_WRITE16(I915REG_HWSTAM, 0xfffe);
	I915_WRITE16(I915REG_INT_MASK_R, 0x0);
	I915_WRITE16(I915REG_INT_ENABLE_R, 0x0);
}

void i915_driver_irq_postinstall(drm_device_t * dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	I915_WRITE16(I915REG_INT_ENABLE_R, USER_INT_FLAG);
	DRM_INIT_WAITQUEUE(&dev_priv->irq_queue);
}

void i915_driver_irq_uninstall(drm_device_t * dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	if (!dev_priv)
		return;

	I915_WRITE16(I915REG_HWSTAM, 0xffff);
	I915_WRITE16(I915REG_INT_MASK_R, 0xffff);
	I915_WRITE16(I915REG_INT_ENABLE_R, 0x0);
}
