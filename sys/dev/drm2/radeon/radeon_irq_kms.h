/* $MidnightBSD$ */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: stable/10/sys/dev/drm2/radeon/radeon_irq_kms.h 282199 2015-04-28 19:35:05Z dumbbell $");

#ifndef __RADEON_IRQ_KMS_H__
#define	__RADEON_IRQ_KMS_H__

irqreturn_t radeon_driver_irq_handler_kms(DRM_IRQ_ARGS);
void radeon_driver_irq_preinstall_kms(struct drm_device *dev);
int radeon_driver_irq_postinstall_kms(struct drm_device *dev);
void radeon_driver_irq_uninstall_kms(struct drm_device *dev);

#endif /* !defined(__RADEON_IRQ_KMS_H__) */
