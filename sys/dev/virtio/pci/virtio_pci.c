/*-
 * Copyright (c) 2011, Bryan Venteicher <bryanv@daemoninthecloset.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Driver for the VirtIO PCI interface. */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: stable/9/sys/dev/virtio/pci/virtio_pci.c 246582 2013-02-09 06:11:45Z bryanv $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
#include <dev/virtio/pci/virtio_pci.h>

#include "virtio_bus_if.h"
#include "virtio_if.h"

struct vtpci_softc {
	device_t			 vtpci_dev;
	struct resource			*vtpci_res;
	struct resource			*vtpci_msix_res;
	uint64_t			 vtpci_features;
	uint32_t			 vtpci_flags;
#define VTPCI_FLAG_NO_MSI		0x0001
#define VTPCI_FLAG_NO_MSIX		0x0002
#define VTPCI_FLAG_LEGACY		0x1000
#define VTPCI_FLAG_MSI			0x2000
#define VTPCI_FLAG_MSIX			0x4000
#define VTPCI_FLAG_SHARED_MSIX		0x8000
#define VTPCI_FLAG_ITYPE_MASK		0xF000

	/* This "bus" will only ever have one child. */
	device_t			 vtpci_child_dev;
	struct virtio_feature_desc	*vtpci_child_feat_desc;

	/*
	 * Ideally, each virtqueue that the driver provides a callback for
	 * will receive its own MSIX vector. If there are not sufficient
	 * vectors available, we will then attempt to have all the VQs
	 * share one vector. Note that when using MSIX, the configuration
	 * changed notifications must be on their own vector.
	 *
	 * If MSIX is not available, we will attempt to have the whole
	 * device share one MSI vector, and then, finally, one legacy
	 * interrupt.
	 */
	int				 vtpci_nvqs;
	struct vtpci_virtqueue {
		struct virtqueue *vq;
		/* Device did not provide a callback for this virtqueue. */
		int		  no_intr;
		/* Index into vtpci_intr_res[] below. Unused, then -1. */
		int		  ires_idx;
	} vtpci_vqx[VIRTIO_MAX_VIRTQUEUES];

	/*
	 * When using MSIX interrupts, the first element of vtpci_intr_res[]
	 * is always the configuration changed notifications. The remaining
	 * element(s) are used for the virtqueues.
	 *
	 * With MSI and legacy interrupts, only the first element of
	 * vtpci_intr_res[] is used.
	 */
	int				 vtpci_nintr_res;
	struct vtpci_intr_resource {
		struct resource	*irq;
		int		 rid;
		void		*intrhand;
	} vtpci_intr_res[1 + VIRTIO_MAX_VIRTQUEUES];
};

static int	vtpci_probe(device_t);
static int	vtpci_attach(device_t);
static int	vtpci_detach(device_t);
static int	vtpci_suspend(device_t);
static int	vtpci_resume(device_t);
static int	vtpci_shutdown(device_t);
static void	vtpci_driver_added(device_t, driver_t *);
static void	vtpci_child_detached(device_t, device_t);
static int	vtpci_read_ivar(device_t, device_t, int, uintptr_t *);
static int	vtpci_write_ivar(device_t, device_t, int, uintptr_t);

static uint64_t	vtpci_negotiate_features(device_t, uint64_t);
static int	vtpci_with_feature(device_t, uint64_t);
static int	vtpci_alloc_virtqueues(device_t, int, int,
		    struct vq_alloc_info *);
static int	vtpci_setup_intr(device_t, enum intr_type);
static void	vtpci_stop(device_t);
static int	vtpci_reinit(device_t, uint64_t);
static void	vtpci_reinit_complete(device_t);
static void	vtpci_notify_virtqueue(device_t, uint16_t);
static uint8_t	vtpci_get_status(device_t);
static void	vtpci_set_status(device_t, uint8_t);
static void	vtpci_read_dev_config(device_t, bus_size_t, void *, int);
static void	vtpci_write_dev_config(device_t, bus_size_t, void *, int);

static void	vtpci_describe_features(struct vtpci_softc *, const char *,
		    uint64_t);
static void	vtpci_probe_and_attach_child(struct vtpci_softc *);

static int 	vtpci_alloc_msix(struct vtpci_softc *, int);
static int 	vtpci_alloc_msi(struct vtpci_softc *);
static int 	vtpci_alloc_intr_msix_pervq(struct vtpci_softc *);
static int 	vtpci_alloc_intr_msix_shared(struct vtpci_softc *);
static int 	vtpci_alloc_intr_msi(struct vtpci_softc *);
static int 	vtpci_alloc_intr_legacy(struct vtpci_softc *);
static int	vtpci_alloc_intr_resources(struct vtpci_softc *);

static int 	vtpci_setup_legacy_interrupt(struct vtpci_softc *,
		    enum intr_type);
static int 	vtpci_setup_msix_interrupts(struct vtpci_softc *,
		    enum intr_type);
static int 	vtpci_setup_interrupts(struct vtpci_softc *, enum intr_type);

static int	vtpci_register_msix_vector(struct vtpci_softc *, int, int);
static int 	vtpci_set_host_msix_vectors(struct vtpci_softc *);
static int 	vtpci_reinit_virtqueue(struct vtpci_softc *, int);

static void	vtpci_free_interrupts(struct vtpci_softc *);
static void	vtpci_free_virtqueues(struct vtpci_softc *);
static void 	vtpci_cleanup_setup_intr_attempt(struct vtpci_softc *);
static void	vtpci_release_child_resources(struct vtpci_softc *);
static void	vtpci_reset(struct vtpci_softc *);

static void	vtpci_select_virtqueue(struct vtpci_softc *, int);

static int	vtpci_legacy_intr(void *);
static int	vtpci_vq_shared_intr(void *);
static int	vtpci_vq_intr(void *);
static int	vtpci_config_intr(void *);

#define vtpci_setup_msi_interrupt vtpci_setup_legacy_interrupt

/*
 * I/O port read/write wrappers.
 */
#define vtpci_read_config_1(sc, o)	bus_read_1((sc)->vtpci_res, (o))
#define vtpci_read_config_2(sc, o)	bus_read_2((sc)->vtpci_res, (o))
#define vtpci_read_config_4(sc, o)	bus_read_4((sc)->vtpci_res, (o))
#define vtpci_write_config_1(sc, o, v)	bus_write_1((sc)->vtpci_res, (o), (v))
#define vtpci_write_config_2(sc, o, v)	bus_write_2((sc)->vtpci_res, (o), (v))
#define vtpci_write_config_4(sc, o, v)	bus_write_4((sc)->vtpci_res, (o), (v))

/* Tunables. */
static int vtpci_disable_msix = 0;
TUNABLE_INT("hw.virtio.pci.disable_msix", &vtpci_disable_msix);

static device_method_t vtpci_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,			  vtpci_probe),
	DEVMETHOD(device_attach,		  vtpci_attach),
	DEVMETHOD(device_detach,		  vtpci_detach),
	DEVMETHOD(device_suspend,		  vtpci_suspend),
	DEVMETHOD(device_resume,		  vtpci_resume),
	DEVMETHOD(device_shutdown,		  vtpci_shutdown),

	/* Bus interface. */
	DEVMETHOD(bus_driver_added,		  vtpci_driver_added),
	DEVMETHOD(bus_child_detached,		  vtpci_child_detached),
	DEVMETHOD(bus_read_ivar,		  vtpci_read_ivar),
	DEVMETHOD(bus_write_ivar,		  vtpci_write_ivar),

	/* VirtIO bus interface. */
	DEVMETHOD(virtio_bus_negotiate_features,  vtpci_negotiate_features),
	DEVMETHOD(virtio_bus_with_feature,	  vtpci_with_feature),
	DEVMETHOD(virtio_bus_alloc_virtqueues,	  vtpci_alloc_virtqueues),
	DEVMETHOD(virtio_bus_setup_intr,	  vtpci_setup_intr),
	DEVMETHOD(virtio_bus_stop,		  vtpci_stop),
	DEVMETHOD(virtio_bus_reinit,		  vtpci_reinit),
	DEVMETHOD(virtio_bus_reinit_complete,	  vtpci_reinit_complete),
	DEVMETHOD(virtio_bus_notify_vq,		  vtpci_notify_virtqueue),
	DEVMETHOD(virtio_bus_read_device_config,  vtpci_read_dev_config),
	DEVMETHOD(virtio_bus_write_device_config, vtpci_write_dev_config),

	DEVMETHOD_END
};

static driver_t vtpci_driver = {
	"virtio_pci",
	vtpci_methods,
	sizeof(struct vtpci_softc)
};

devclass_t vtpci_devclass;

DRIVER_MODULE(virtio_pci, pci, vtpci_driver, vtpci_devclass, 0, 0);
MODULE_VERSION(virtio_pci, 1);
MODULE_DEPEND(virtio_pci, pci, 1, 1, 1);
MODULE_DEPEND(virtio_pci, virtio, 1, 1, 1);

static int
vtpci_probe(device_t dev)
{
	char desc[36];
	const char *name;

	if (pci_get_vendor(dev) != VIRTIO_PCI_VENDORID)
		return (ENXIO);

	if (pci_get_device(dev) < VIRTIO_PCI_DEVICEID_MIN ||
	    pci_get_device(dev) > VIRTIO_PCI_DEVICEID_MAX)
		return (ENXIO);

	if (pci_get_revid(dev) != VIRTIO_PCI_ABI_VERSION)
		return (ENXIO);

	name = virtio_device_name(pci_get_subdevice(dev));
	if (name == NULL)
		name = "Unknown";

	snprintf(desc, sizeof(desc), "VirtIO PCI %s adapter", name);
	device_set_desc_copy(dev, desc);

	return (BUS_PROBE_DEFAULT);
}

static int
vtpci_attach(device_t dev)
{
	struct vtpci_softc *sc;
	device_t child;
	int rid;

	sc = device_get_softc(dev);
	sc->vtpci_dev = dev;

	pci_enable_busmaster(dev);

	rid = PCIR_BAR(0);
	sc->vtpci_res = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid,
	    RF_ACTIVE);
	if (sc->vtpci_res == NULL) {
		device_printf(dev, "cannot map I/O space\n");
		return (ENXIO);
	}

	if (pci_find_extcap(dev, PCIY_MSI, NULL) != 0)
		sc->vtpci_flags |= VTPCI_FLAG_NO_MSI;

	if (pci_find_extcap(dev, PCIY_MSIX, NULL) == 0) {
		rid = PCIR_BAR(1);
		sc->vtpci_msix_res = bus_alloc_resource_any(dev,
		    SYS_RES_MEMORY, &rid, RF_ACTIVE);
	}

	if (sc->vtpci_msix_res == NULL)
		sc->vtpci_flags |= VTPCI_FLAG_NO_MSIX;

	vtpci_reset(sc);

	/* Tell the host we've noticed this device. */
	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_ACK);

	if ((child = device_add_child(dev, NULL, -1)) == NULL) {
		device_printf(dev, "cannot create child device\n");
		vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_FAILED);
		vtpci_detach(dev);
		return (ENOMEM);
	}

	sc->vtpci_child_dev = child;
	vtpci_probe_and_attach_child(sc);

	return (0);
}

static int
vtpci_detach(device_t dev)
{
	struct vtpci_softc *sc;
	device_t child;
	int error;

	sc = device_get_softc(dev);

	if ((child = sc->vtpci_child_dev) != NULL) {
		error = device_delete_child(dev, child);
		if (error)
			return (error);
		sc->vtpci_child_dev = NULL;
	}

	vtpci_reset(sc);

	if (sc->vtpci_msix_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BAR(1),
		    sc->vtpci_msix_res);
		sc->vtpci_msix_res = NULL;
	}

	if (sc->vtpci_res != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, PCIR_BAR(0),
		    sc->vtpci_res);
		sc->vtpci_res = NULL;
	}

	return (0);
}

static int
vtpci_suspend(device_t dev)
{

	return (bus_generic_suspend(dev));
}

static int
vtpci_resume(device_t dev)
{

	return (bus_generic_resume(dev));
}

static int
vtpci_shutdown(device_t dev)
{

	(void) bus_generic_shutdown(dev);
	/* Forcibly stop the host device. */
	vtpci_stop(dev);

	return (0);
}

static void
vtpci_driver_added(device_t dev, driver_t *driver)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	vtpci_probe_and_attach_child(sc);
}

static void
vtpci_child_detached(device_t dev, device_t child)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	vtpci_reset(sc);
	vtpci_release_child_resources(sc);
}

static int
vtpci_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	if (sc->vtpci_child_dev != child)
		return (ENOENT);

	switch (index) {
	case VIRTIO_IVAR_DEVTYPE:
	case VIRTIO_IVAR_SUBDEVICE:
		*result = pci_get_subdevice(dev);
		break;
	case VIRTIO_IVAR_VENDOR:
		*result = pci_get_vendor(dev);
		break;
	case VIRTIO_IVAR_DEVICE:
		*result = pci_get_device(dev);
		break;
	case VIRTIO_IVAR_SUBVENDOR:
		*result = pci_get_subdevice(dev);
		break;
	default:
		return (ENOENT);
	}

	return (0);
}

static int
vtpci_write_ivar(device_t dev, device_t child, int index, uintptr_t value)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	if (sc->vtpci_child_dev != child)
		return (ENOENT);

	switch (index) {
	case VIRTIO_IVAR_FEATURE_DESC:
		sc->vtpci_child_feat_desc = (void *) value;
		break;
	default:
		return (ENOENT);
	}

	return (0);
}

static uint64_t
vtpci_negotiate_features(device_t dev, uint64_t child_features)
{
	struct vtpci_softc *sc;
	uint64_t host_features, features;

	sc = device_get_softc(dev);

	host_features = vtpci_read_config_4(sc, VIRTIO_PCI_HOST_FEATURES);
	vtpci_describe_features(sc, "host", host_features);

	/*
	 * Limit negotiated features to what the driver, virtqueue, and
	 * host all support.
	 */
	features = host_features & child_features;
	features = virtqueue_filter_features(features);
	sc->vtpci_features = features;

	vtpci_describe_features(sc, "negotiated", features);
	vtpci_write_config_4(sc, VIRTIO_PCI_GUEST_FEATURES, features);

	return (features);
}

static int
vtpci_with_feature(device_t dev, uint64_t feature)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	return ((sc->vtpci_features & feature) != 0);
}

static int
vtpci_alloc_virtqueues(device_t dev, int flags, int nvqs,
    struct vq_alloc_info *vq_info)
{
	struct vtpci_softc *sc;
	struct virtqueue *vq;
	struct vtpci_virtqueue *vqx;
	struct vq_alloc_info *info;
	int idx, error;
	uint16_t size;

	sc = device_get_softc(dev);
	error = 0;

	if (sc->vtpci_nvqs != 0)
		return (EALREADY);
	if (nvqs <= 0 || nvqs > VIRTIO_MAX_VIRTQUEUES)
		return (EINVAL);

	if (flags & VIRTIO_ALLOC_VQS_DISABLE_MSIX)
		sc->vtpci_flags |= VTPCI_FLAG_NO_MSIX;

	for (idx = 0; idx < nvqs; idx++) {
		vqx = &sc->vtpci_vqx[idx];
		info = &vq_info[idx];

		vtpci_select_virtqueue(sc, idx);
		size = vtpci_read_config_2(sc, VIRTIO_PCI_QUEUE_NUM);

		error = virtqueue_alloc(dev, idx, size, VIRTIO_PCI_VRING_ALIGN,
		    0xFFFFFFFFUL, info, &vq);
		if (error) {
			device_printf(dev,
			    "cannot allocate virtqueue %d: %d\n", idx, error);
			break;
		}

		vtpci_write_config_4(sc, VIRTIO_PCI_QUEUE_PFN,
		    virtqueue_paddr(vq) >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);

		vqx->vq = *info->vqai_vq = vq;
		vqx->no_intr = info->vqai_intr == NULL;

		sc->vtpci_nvqs++;
	}

	return (error);
}

static int
vtpci_setup_intr(device_t dev, enum intr_type type)
{
	struct vtpci_softc *sc;
	int attempt, error;

	sc = device_get_softc(dev);

	for (attempt = 0; attempt < 5; attempt++) {
		/*
		 * Start with the most desirable interrupt configuration and
		 * fallback towards less desirable ones.
		 */
		switch (attempt) {
		case 0:
			error = vtpci_alloc_intr_msix_pervq(sc);
			break;
		case 1:
			error = vtpci_alloc_intr_msix_shared(sc);
			break;
		case 2:
			error = vtpci_alloc_intr_msi(sc);
			break;
		case 3:
			error = vtpci_alloc_intr_legacy(sc);
			break;
		default:
			device_printf(dev,
			    "exhausted all interrupt allocation attempts\n");
			return (ENXIO);
		}

		if (error == 0 && vtpci_setup_interrupts(sc, type) == 0)
			break;

		vtpci_cleanup_setup_intr_attempt(sc);
	}

	if (bootverbose) {
		if (sc->vtpci_flags & VTPCI_FLAG_LEGACY)
			device_printf(dev, "using legacy interrupt\n");
		else if (sc->vtpci_flags & VTPCI_FLAG_MSI)
			device_printf(dev, "using MSI interrupt\n");
		else if (sc->vtpci_flags & VTPCI_FLAG_SHARED_MSIX)
			device_printf(dev, "using shared MSIX interrupts\n");
		else
			device_printf(dev, "using per VQ MSIX interrupts\n");
	}

	return (0);
}

static void
vtpci_stop(device_t dev)
{

	vtpci_reset(device_get_softc(dev));
}

static int
vtpci_reinit(device_t dev, uint64_t features)
{
	struct vtpci_softc *sc;
	int idx, error;

	sc = device_get_softc(dev);

	/*
	 * Redrive the device initialization. This is a bit of an abuse of
	 * the specification, but VirtualBox, QEMU/KVM, and BHyVe seem to
	 * play nice.
	 *
	 * We do not allow the host device to change from what was originally
	 * negotiated beyond what the guest driver changed. MSIX state should
	 * not change, number of virtqueues and their size remain the same, etc.
	 * This will need to be rethought when we want to support migration.
	 */

	if (vtpci_get_status(dev) != VIRTIO_CONFIG_STATUS_RESET)
		vtpci_stop(dev);

	/*
	 * Quickly drive the status through ACK and DRIVER. The device
	 * does not become usable again until vtpci_reinit_complete().
	 */
	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_ACK);
	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER);

	vtpci_negotiate_features(dev, features);

	for (idx = 0; idx < sc->vtpci_nvqs; idx++) {
		error = vtpci_reinit_virtqueue(sc, idx);
		if (error)
			return (error);
	}

	if (sc->vtpci_flags & VTPCI_FLAG_MSIX) {
		error = vtpci_set_host_msix_vectors(sc);
		if (error)
			return (error);
	}

	return (0);
}

static void
vtpci_reinit_complete(device_t dev)
{

	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER_OK);
}

static void
vtpci_notify_virtqueue(device_t dev, uint16_t queue)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	vtpci_write_config_2(sc, VIRTIO_PCI_QUEUE_NOTIFY, queue);
}

static uint8_t
vtpci_get_status(device_t dev)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	return (vtpci_read_config_1(sc, VIRTIO_PCI_STATUS));
}

static void
vtpci_set_status(device_t dev, uint8_t status)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	if (status != VIRTIO_CONFIG_STATUS_RESET)
		status |= vtpci_get_status(dev);

	vtpci_write_config_1(sc, VIRTIO_PCI_STATUS, status);
}

static void
vtpci_read_dev_config(device_t dev, bus_size_t offset,
    void *dst, int length)
{
	struct vtpci_softc *sc;
	bus_size_t off;
	uint8_t *d;
	int size;

	sc = device_get_softc(dev);
	off = VIRTIO_PCI_CONFIG(sc) + offset;

	for (d = dst; length > 0; d += size, off += size, length -= size) {
		if (length >= 4) {
			size = 4;
			*(uint32_t *)d = vtpci_read_config_4(sc, off);
		} else if (length >= 2) {
			size = 2;
			*(uint16_t *)d = vtpci_read_config_2(sc, off);
		} else {
			size = 1;
			*d = vtpci_read_config_1(sc, off);
		}
	}
}

static void
vtpci_write_dev_config(device_t dev, bus_size_t offset,
    void *src, int length)
{
	struct vtpci_softc *sc;
	bus_size_t off;
	uint8_t *s;
	int size;

	sc = device_get_softc(dev);
	off = VIRTIO_PCI_CONFIG(sc) + offset;

	for (s = src; length > 0; s += size, off += size, length -= size) {
		if (length >= 4) {
			size = 4;
			vtpci_write_config_4(sc, off, *(uint32_t *)s);
		} else if (length >= 2) {
			size = 2;
			vtpci_write_config_2(sc, off, *(uint16_t *)s);
		} else {
			size = 1;
			vtpci_write_config_1(sc, off, *s);
		}
	}
}

static void
vtpci_describe_features(struct vtpci_softc *sc, const char *msg,
    uint64_t features)
{
	device_t dev, child;

	dev = sc->vtpci_dev;
	child = sc->vtpci_child_dev;

	if (device_is_attached(child) && bootverbose == 0)
		return;

	virtio_describe(dev, msg, features, sc->vtpci_child_feat_desc);
}

static void
vtpci_probe_and_attach_child(struct vtpci_softc *sc)
{
	device_t dev, child;

	dev = sc->vtpci_dev;
	child = sc->vtpci_child_dev;

	if (child == NULL)
		return;

	if (device_get_state(child) != DS_NOTPRESENT)
		return;

	if (device_probe(child) != 0)
		return;

	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER);
	if (device_attach(child) != 0) {
		vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_FAILED);
		vtpci_reset(sc);
		vtpci_release_child_resources(sc);
		/* Reset status for future attempt. */
		vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_ACK);
	} else
		vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER_OK);
}

static int
vtpci_alloc_msix(struct vtpci_softc *sc, int nvectors)
{
	device_t dev;
	int nmsix, cnt, required;

	dev = sc->vtpci_dev;

	/* Allocate an additional vector for the config changes. */
	required = nvectors + 1;

	nmsix = pci_msix_count(dev);
	if (nmsix < required)
		return (1);

	cnt = required;
	if (pci_alloc_msix(dev, &cnt) == 0 && cnt >= required) {
		sc->vtpci_nintr_res = required;
		return (0);
	}

	pci_release_msi(dev);

	return (1);
}

static int
vtpci_alloc_msi(struct vtpci_softc *sc)
{
	device_t dev;
	int nmsi, cnt, required;

	dev = sc->vtpci_dev;
	required = 1;

	nmsi = pci_msi_count(dev);
	if (nmsi < required)
		return (1);

	cnt = required;
	if (pci_alloc_msi(dev, &cnt) == 0 && cnt >= required) {
		sc->vtpci_nintr_res = required;
		return (0);
	}

	pci_release_msi(dev);

	return (1);
}

static int
vtpci_alloc_intr_msix_pervq(struct vtpci_softc *sc)
{
	int i, nvectors, error;

	if (vtpci_disable_msix != 0 ||
	    sc->vtpci_flags & VTPCI_FLAG_NO_MSIX)
		return (ENOTSUP);

	for (nvectors = 0, i = 0; i < sc->vtpci_nvqs; i++) {
		if (sc->vtpci_vqx[i].no_intr == 0)
			nvectors++;
	}

	error = vtpci_alloc_msix(sc, nvectors);
	if (error)
		return (error);

	sc->vtpci_flags |= VTPCI_FLAG_MSIX;

	return (0);
}

static int
vtpci_alloc_intr_msix_shared(struct vtpci_softc *sc)
{
	int error;

	if (vtpci_disable_msix != 0 ||
	    sc->vtpci_flags & VTPCI_FLAG_NO_MSIX)
		return (ENOTSUP);

	error = vtpci_alloc_msix(sc, 1);
	if (error)
		return (error);

	sc->vtpci_flags |= VTPCI_FLAG_MSIX | VTPCI_FLAG_SHARED_MSIX;

	return (0);
}

static int
vtpci_alloc_intr_msi(struct vtpci_softc *sc)
{
	int error;

	/* Only BHyVe supports MSI. */
	if (sc->vtpci_flags & VTPCI_FLAG_NO_MSI)
		return (ENOTSUP);

	error = vtpci_alloc_msi(sc);
	if (error)
		return (error);

	sc->vtpci_flags |= VTPCI_FLAG_MSI;

	return (0);
}

static int
vtpci_alloc_intr_legacy(struct vtpci_softc *sc)
{

	sc->vtpci_flags |= VTPCI_FLAG_LEGACY;
	sc->vtpci_nintr_res = 1;

	return (0);
}

static int
vtpci_alloc_intr_resources(struct vtpci_softc *sc)
{
	device_t dev;
	struct resource *irq;
	struct vtpci_virtqueue *vqx;
	int i, rid, flags, res_idx;

	dev = sc->vtpci_dev;

	if (sc->vtpci_flags & VTPCI_FLAG_LEGACY) {
		rid = 0;
		flags = RF_ACTIVE | RF_SHAREABLE;
	} else {
		rid = 1;
		flags = RF_ACTIVE;
	}

	for (i = 0; i < sc->vtpci_nintr_res; i++) {
		irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, flags);
		if (irq == NULL)
			return (ENXIO);

		sc->vtpci_intr_res[i].irq = irq;
		sc->vtpci_intr_res[i].rid = rid++;
	}

	/*
	 * Map the virtqueue into the correct index in vq_intr_res[]. The
	 * first index is reserved for configuration changed notifications.
	 */
	for (i = 0, res_idx = 1; i < sc->vtpci_nvqs; i++) {
		vqx = &sc->vtpci_vqx[i];

		if (sc->vtpci_flags & VTPCI_FLAG_MSIX) {
			if (vqx->no_intr != 0)
				vqx->ires_idx = -1;
			else if (sc->vtpci_flags & VTPCI_FLAG_SHARED_MSIX)
				vqx->ires_idx = res_idx;
			else
				vqx->ires_idx = res_idx++;
		} else
			vqx->ires_idx = -1;
	}

	return (0);
}

static int
vtpci_setup_legacy_interrupt(struct vtpci_softc *sc, enum intr_type type)
{
	device_t dev;
	struct vtpci_intr_resource *ires;
	int error;

	dev = sc->vtpci_dev;

	ires = &sc->vtpci_intr_res[0];
	error = bus_setup_intr(dev, ires->irq, type, vtpci_legacy_intr, NULL,
	    sc, &ires->intrhand);

	return (error);
}

static int
vtpci_setup_msix_interrupts(struct vtpci_softc *sc, enum intr_type type)
{
	device_t dev;
	struct vtpci_intr_resource *ires;
	struct vtpci_virtqueue *vqx;
	int i, error;

	dev = sc->vtpci_dev;

	/*
	 * The first resource is used for configuration changed interrupts.
	 */
	ires = &sc->vtpci_intr_res[0];
	error = bus_setup_intr(dev, ires->irq, type, vtpci_config_intr,
	    NULL, sc, &ires->intrhand);
	if (error)
		return (error);

	if (sc->vtpci_flags & VTPCI_FLAG_SHARED_MSIX) {
		ires = &sc->vtpci_intr_res[1];

		error = bus_setup_intr(dev, ires->irq, type,
		    vtpci_vq_shared_intr, NULL, sc, &ires->intrhand);
		if (error)
			return (error);
	} else {
		/*
		 * Each remaining resource is assigned to a specific virtqueue.
		 */
		for (i = 0; i < sc->vtpci_nvqs; i++) {
			vqx = &sc->vtpci_vqx[i];
			if (vqx->ires_idx < 1)
				continue;

			ires = &sc->vtpci_intr_res[vqx->ires_idx];
			error = bus_setup_intr(dev, ires->irq, type,
			    vtpci_vq_intr, NULL, vqx->vq, &ires->intrhand);
			if (error)
				return (error);
		}
	}

	error = vtpci_set_host_msix_vectors(sc);
	if (error)
		return (error);

	return (0);
}

static int
vtpci_setup_interrupts(struct vtpci_softc *sc, enum intr_type type)
{
	int error;

	type |= INTR_MPSAFE;
	KASSERT(sc->vtpci_flags & VTPCI_FLAG_ITYPE_MASK,
	    ("no interrupt type selected: %#x", sc->vtpci_flags));

	error = vtpci_alloc_intr_resources(sc);
	if (error)
		return (error);

	if (sc->vtpci_flags & VTPCI_FLAG_LEGACY)
		error = vtpci_setup_legacy_interrupt(sc, type);
	else if (sc->vtpci_flags & VTPCI_FLAG_MSI)
		error = vtpci_setup_msi_interrupt(sc, type);
	else
		error = vtpci_setup_msix_interrupts(sc, type);

	return (error);
}

static int
vtpci_register_msix_vector(struct vtpci_softc *sc, int offset, int res_idx)
{
	device_t dev;
	uint16_t vector, rdvector;

	dev = sc->vtpci_dev;

	if (res_idx != -1) {
		/* Map from guest rid to host vector. */
		vector = sc->vtpci_intr_res[res_idx].rid - 1;
	} else
		vector = VIRTIO_MSI_NO_VECTOR;

	/*
	 * Assert the first resource is always used for the configuration
	 * changed interrupts.
	 */
	if (res_idx == 0) {
		KASSERT(vector == 0 && offset == VIRTIO_MSI_CONFIG_VECTOR,
		    ("bad first res use vector:%d offset:%d", vector, offset));
	} else
		KASSERT(offset == VIRTIO_MSI_QUEUE_VECTOR, ("bad offset"));

	vtpci_write_config_2(sc, offset, vector);

	/* Read vector to determine if the host had sufficient resources. */
	rdvector = vtpci_read_config_2(sc, offset);
	if (rdvector != vector) {
		device_printf(dev,
		    "insufficient host resources for MSIX interrupts\n");
		return (ENODEV);
	}

	return (0);
}

static int
vtpci_set_host_msix_vectors(struct vtpci_softc *sc)
{
	struct vtpci_virtqueue *vqx;
	int idx, error;

	error = vtpci_register_msix_vector(sc, VIRTIO_MSI_CONFIG_VECTOR, 0);
	if (error)
		return (error);

	for (idx = 0; idx < sc->vtpci_nvqs; idx++) {
		vqx = &sc->vtpci_vqx[idx];

		vtpci_select_virtqueue(sc, idx);
		error = vtpci_register_msix_vector(sc, VIRTIO_MSI_QUEUE_VECTOR,
		    vqx->ires_idx);
		if (error)
			return (error);
	}

	return (0);
}

static int
vtpci_reinit_virtqueue(struct vtpci_softc *sc, int idx)
{
	struct vtpci_virtqueue *vqx;
	struct virtqueue *vq;
	int error;
	uint16_t size;

	vqx = &sc->vtpci_vqx[idx];
	vq = vqx->vq;

	KASSERT(vq != NULL, ("vq %d not allocated", idx));

	vtpci_select_virtqueue(sc, idx);
	size = vtpci_read_config_2(sc, VIRTIO_PCI_QUEUE_NUM);

	error = virtqueue_reinit(vq, size);
	if (error)
		return (error);

	vtpci_write_config_4(sc, VIRTIO_PCI_QUEUE_PFN,
	    virtqueue_paddr(vq) >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);

	return (0);
}

static void
vtpci_free_interrupts(struct vtpci_softc *sc)
{
	device_t dev;
	struct vtpci_intr_resource *ires;
	int i;

	dev = sc->vtpci_dev;

	for (i = 0; i < sc->vtpci_nintr_res; i++) {
		ires = &sc->vtpci_intr_res[i];

		if (ires->intrhand != NULL) {
			bus_teardown_intr(dev, ires->irq, ires->intrhand);
			ires->intrhand = NULL;
		}

		if (ires->irq != NULL) {
			bus_release_resource(dev, SYS_RES_IRQ, ires->rid,
			    ires->irq);
			ires->irq = NULL;
		}

		ires->rid = -1;
	}

	if (sc->vtpci_flags & (VTPCI_FLAG_MSI | VTPCI_FLAG_MSIX))
		pci_release_msi(dev);

	sc->vtpci_nintr_res = 0;
	sc->vtpci_flags &= ~VTPCI_FLAG_ITYPE_MASK;
}

static void
vtpci_free_virtqueues(struct vtpci_softc *sc)
{
	struct vtpci_virtqueue *vqx;
	int i;

	for (i = 0; i < sc->vtpci_nvqs; i++) {
		vqx = &sc->vtpci_vqx[i];

		virtqueue_free(vqx->vq);
		vqx->vq = NULL;
	}

	sc->vtpci_nvqs = 0;
}

static void
vtpci_cleanup_setup_intr_attempt(struct vtpci_softc *sc)
{
	int idx;

	if (sc->vtpci_flags & VTPCI_FLAG_MSIX) {
		vtpci_write_config_2(sc, VIRTIO_MSI_CONFIG_VECTOR,
		    VIRTIO_MSI_NO_VECTOR);

		for (idx = 0; idx < sc->vtpci_nvqs; idx++) {
			vtpci_select_virtqueue(sc, idx);
			vtpci_write_config_2(sc, VIRTIO_MSI_QUEUE_VECTOR,
			    VIRTIO_MSI_NO_VECTOR);
		}
	}

	vtpci_free_interrupts(sc);
}

static void
vtpci_release_child_resources(struct vtpci_softc *sc)
{

	vtpci_free_interrupts(sc);
	vtpci_free_virtqueues(sc);
}

static void
vtpci_reset(struct vtpci_softc *sc)
{

	/*
	 * Setting the status to RESET sets the host device to
	 * the original, uninitialized state.
	 */
	vtpci_set_status(sc->vtpci_dev, VIRTIO_CONFIG_STATUS_RESET);
}

static void
vtpci_select_virtqueue(struct vtpci_softc *sc, int idx)
{

	vtpci_write_config_2(sc, VIRTIO_PCI_QUEUE_SEL, idx);
}

static int
vtpci_legacy_intr(void *xsc)
{
	struct vtpci_softc *sc;
	struct vtpci_virtqueue *vqx;
	int i;
	uint8_t isr;

	sc = xsc;
	vqx = &sc->vtpci_vqx[0];

	/* Reading the ISR also clears it. */
	isr = vtpci_read_config_1(sc, VIRTIO_PCI_ISR);

	if (isr & VIRTIO_PCI_ISR_CONFIG)
		vtpci_config_intr(sc);

	if (isr & VIRTIO_PCI_ISR_INTR)
		for (i = 0; i < sc->vtpci_nvqs; i++, vqx++)
			virtqueue_intr(vqx->vq);

	return (isr ? FILTER_HANDLED : FILTER_STRAY);
}

static int
vtpci_vq_shared_intr(void *xsc)
{
	struct vtpci_softc *sc;
	struct vtpci_virtqueue *vqx;
	int i, rc;

	rc = 0;
	sc = xsc;
	vqx = &sc->vtpci_vqx[0];

	for (i = 0; i < sc->vtpci_nvqs; i++, vqx++)
		rc |= virtqueue_intr(vqx->vq);

	return (rc ? FILTER_HANDLED : FILTER_STRAY);
}

static int
vtpci_vq_intr(void *xvq)
{
	struct virtqueue *vq;
	int rc;

	vq = xvq;
	rc = virtqueue_intr(vq);

	return (rc ? FILTER_HANDLED : FILTER_STRAY);
}

static int
vtpci_config_intr(void *xsc)
{
	struct vtpci_softc *sc;
	device_t child;
	int rc;

	rc = 0;
	sc = xsc;
	child = sc->vtpci_child_dev;

	if (child != NULL)
		rc = VIRTIO_CONFIG_CHANGE(child);

	return (rc ? FILTER_HANDLED : FILTER_STRAY);
}
