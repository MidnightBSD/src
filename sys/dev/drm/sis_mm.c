/* sis_mm.c -- Private header for Direct Rendering Manager -*- linux-c -*-
 * Created: Mon Jan  4 10:05:05 1999 by sclin@sis.com.tw
 *
 * Copyright 2000 Silicon Integrated Systems Corp, Inc., HsinChu, Taiwan.
 * All rights reserved.
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#if defined(__linux__) && defined(CONFIG_FB_SIS)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <video/sisfb.h>
#else
#include <linux/sisfb.h>
#endif
#endif
#include "dev/drm/drmP.h"
#include "dev/drm/sis_drm.h"
#include "dev/drm/sis_drv.h"
#include "dev/drm/sis_ds.h"

#define MAX_CONTEXT 100
#define VIDEO_TYPE 0
#define AGP_TYPE 1

typedef struct {
	int used;
	int context;
	set_t *sets[2];		/* 0 for video, 1 for AGP */
} sis_context_t;

static sis_context_t global_ppriv[MAX_CONTEXT];

static int add_alloc_set(int context, int type, unsigned int val)
{
	int i, retval = 0;

	for (i = 0; i < MAX_CONTEXT; i++) {
		if (global_ppriv[i].used && global_ppriv[i].context == context) {
			retval = setAdd(global_ppriv[i].sets[type], val);
			break;
		}
	}
	return retval;
}

static int del_alloc_set(int context, int type, unsigned int val)
{
	int i, retval = 0;

	for (i = 0; i < MAX_CONTEXT; i++) {
		if (global_ppriv[i].used && global_ppriv[i].context == context) {
			retval = setDel(global_ppriv[i].sets[type], val);
			break;
		}
	}
	return retval;
}

/* fb management via fb device */
#if defined(__linux__) && defined(CONFIG_FB_SIS)

static int sis_fb_init(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	return 0;
}

static int sis_fb_alloc(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_sis_mem_t *fb = data;
	struct sis_memreq req;
	int retval = 0;

	req.size = fb->size;
	sis_malloc(&req);
	if (req.offset) {
		/* TODO */
		fb->offset = req.offset;
		fb->free = req.offset;
		if (!add_alloc_set(fb->context, VIDEO_TYPE, fb->free)) {
			DRM_DEBUG("adding to allocation set fails\n");
			sis_free(req.offset);
			retval = -EINVAL;
		}
	} else {
		fb->offset = 0;
		fb->size = 0;
		fb->free = 0;
	}

	DRM_DEBUG("alloc fb, size = %d, offset = %ld\n", fb->size, req.offset);

	return retval;
}

static int sis_fb_free(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_sis_mem_t fb;
	int retval = 0;

	if (!fb->free)
		return -EINVAL;

	if (!del_alloc_set(fb->context, VIDEO_TYPE, fb->free))
		retval = -EINVAL;
	sis_free(fb->free);

	DRM_DEBUG("free fb, offset = 0x%lx\n", fb->free);

	return retval;
}

#else

/* Called by the X Server to initialize the FB heap.  Allocations will fail
 * unless this is called.  Offset is the beginning of the heap from the
 * framebuffer offset (MaxXFBMem in XFree86).
 *
 * Memory layout according to Thomas Winischofer:
 * |------------------|DDDDDDDDDDDDDDDDDDDDDDDDDDDDD|HHHH|CCCCCCCCCCC|
 *
 *    X driver/sisfb                                  HW-   Command-
 *  framebuffer memory           DRI heap           Cursor   queue
 */
static int sis_fb_init(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_sis_private_t *dev_priv = dev->dev_private;
	drm_sis_fb_t *fb = data;

	if (dev_priv == NULL) {
		dev->dev_private = drm_calloc(1, sizeof(drm_sis_private_t),
					      DRM_MEM_DRIVER);
		dev_priv = dev->dev_private;
		if (dev_priv == NULL)
			return ENOMEM;
	}

	if (dev_priv->FBHeap != NULL)
		return -EINVAL;

	dev_priv->FBHeap = mmInit(fb->offset, fb->size);

	DRM_DEBUG("offset = %u, size = %u", fb->offset, fb->size);

	return 0;
}

static int sis_fb_alloc(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_sis_private_t *dev_priv = dev->dev_private;
	drm_sis_mem_t *fb = data;
	PMemBlock block;
	int retval = 0;

	if (dev_priv == NULL || dev_priv->FBHeap == NULL)
		return -EINVAL;

	block = mmAllocMem(dev_priv->FBHeap, fb->size, 0, 0);
	if (block) {
		/* TODO */
		fb->offset = block->ofs;
		fb->free = (unsigned long)block;
		if (!add_alloc_set(fb->context, VIDEO_TYPE, fb->free)) {
			DRM_DEBUG("adding to allocation set fails\n");
			mmFreeMem((PMemBlock) fb->free);
			retval = -EINVAL;
		}
	} else {
		fb->offset = 0;
		fb->size = 0;
		fb->free = 0;
	}

	DRM_DEBUG("alloc fb, size = %d, offset = %d\n", fb->size, fb->offset);

	return retval;
}

static int sis_fb_free(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_sis_private_t *dev_priv = dev->dev_private;
	drm_sis_mem_t *fb = data;

	if (dev_priv == NULL || dev_priv->FBHeap == NULL)
		return -EINVAL;

	if (!mmBlockInHeap(dev_priv->FBHeap, (PMemBlock) fb->free))
		return -EINVAL;

	if (!del_alloc_set(fb->context, VIDEO_TYPE, fb->free))
		return -EINVAL;
	mmFreeMem((PMemBlock) fb->free);

	DRM_DEBUG("free fb, free = 0x%lx\n", fb->free);

	return 0;
}

#endif

/* agp memory management */

static int sis_ioctl_agp_init(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_sis_private_t *dev_priv = dev->dev_private;
	drm_sis_agp_t *agp = data;

	if (dev_priv == NULL) {
		dev->dev_private = drm_calloc(1, sizeof(drm_sis_private_t),
					      DRM_MEM_DRIVER);
		dev_priv = dev->dev_private;
		if (dev_priv == NULL)
			return ENOMEM;
	}

	if (dev_priv->AGPHeap != NULL)
		return -EINVAL;

	dev_priv->AGPHeap = mmInit(agp->offset, agp->size);

	DRM_DEBUG("offset = %u, size = %u", agp->offset, agp->size);

	return 0;
}

static int sis_ioctl_agp_alloc(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_sis_private_t *dev_priv = dev->dev_private;
	drm_sis_mem_t *agp = data;
	PMemBlock block;
	int retval = 0;

	if (dev_priv == NULL || dev_priv->AGPHeap == NULL)
		return -EINVAL;

	block = mmAllocMem(dev_priv->AGPHeap, agp->size, 0, 0);
	if (block) {
		/* TODO */
		agp->offset = block->ofs;
		agp->free = (unsigned long)block;
		if (!add_alloc_set(agp->context, AGP_TYPE, agp->free)) {
			DRM_DEBUG("adding to allocation set fails\n");
			mmFreeMem((PMemBlock) agp->free);
			retval = -1;
		}
	} else {
		agp->offset = 0;
		agp->size = 0;
		agp->free = 0;
	}

	DRM_DEBUG("alloc agp, size = %d, offset = %d\n", agp->size,
	    agp->offset);

	return retval;
}

static int sis_ioctl_agp_free(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_sis_private_t *dev_priv = dev->dev_private;
	drm_sis_mem_t *agp = data;

	if (dev_priv == NULL || dev_priv->AGPHeap == NULL)
		return -EINVAL;

	if (!mmBlockInHeap(dev_priv->AGPHeap, (PMemBlock) agp->free))
		return -EINVAL;

	mmFreeMem((PMemBlock) agp->free);
	if (!del_alloc_set(agp->context, AGP_TYPE, agp->free))
		return -EINVAL;

	DRM_DEBUG("free agp, free = 0x%lx\n", agp->free);

	return 0;
}

int sis_init_context(struct drm_device *dev, int context)
{
	int i;

	for (i = 0; i < MAX_CONTEXT; i++) {
		if (global_ppriv[i].used &&
		    (global_ppriv[i].context == context))
			break;
	}

	if (i >= MAX_CONTEXT) {
		for (i = 0; i < MAX_CONTEXT; i++) {
			if (!global_ppriv[i].used) {
				global_ppriv[i].context = context;
				global_ppriv[i].used = 1;
				global_ppriv[i].sets[0] = setInit();
				global_ppriv[i].sets[1] = setInit();
				DRM_DEBUG("init allocation set, socket=%d, "
					  "context = %d\n", i, context);
				break;
			}
		}
		if ((i >= MAX_CONTEXT) || (global_ppriv[i].sets[0] == NULL) ||
		    (global_ppriv[i].sets[1] == NULL)) {
			return 0;
		}
	}

	return 1;
}

int sis_final_context(struct drm_device *dev, int context)
{
	int i;

	for (i = 0; i < MAX_CONTEXT; i++) {
		if (global_ppriv[i].used &&
		    (global_ppriv[i].context == context))
			break;
	}

	if (i < MAX_CONTEXT) {
		set_t *set;
		ITEM_TYPE item;
		int retval;

		DRM_DEBUG("find socket %d, context = %d\n", i, context);

		/* Video Memory */
		set = global_ppriv[i].sets[0];
		retval = setFirst(set, &item);
		while (retval) {
			DRM_DEBUG("free video memory 0x%lx\n", item);
#if defined(__linux__) && defined(CONFIG_FB_SIS)
			sis_free(item);
#else
			mmFreeMem((PMemBlock) item);
#endif
			retval = setNext(set, &item);
		}
		setDestroy(set);

		/* AGP Memory */
		set = global_ppriv[i].sets[1];
		retval = setFirst(set, &item);
		while (retval) {
			DRM_DEBUG("free agp memory 0x%lx\n", item);
			mmFreeMem((PMemBlock) item);
			retval = setNext(set, &item);
		}
		setDestroy(set);

		global_ppriv[i].used = 0;
	}

	return 1;
}

drm_ioctl_desc_t sis_ioctls[] = {
	DRM_IOCTL_DEF(DRM_SIS_FB_ALLOC, sis_fb_alloc, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_SIS_FB_FREE, sis_fb_free, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_SIS_AGP_INIT, sis_ioctl_agp_init, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_SIS_AGP_ALLOC, sis_ioctl_agp_alloc, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_SIS_AGP_FREE, sis_ioctl_agp_free, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_SIS_FB_INIT, sis_fb_init, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY)
};

int sis_max_ioctl = DRM_ARRAY_SIZE(sis_ioctls);
