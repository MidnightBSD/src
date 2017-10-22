/* $FreeBSD: release/10.0.0/sys/dev/spibus/spibusvar.h 228471 2011-12-13 14:06:01Z ed $ */

#define SPIBUS_IVAR(d) (struct spibus_ivar *) device_get_ivars(d)
#define SPIBUS_SOFTC(d) (struct spibus_softc *) device_get_softc(d)

struct spibus_softc
{
	device_t	dev;
};

struct spibus_ivar
{
	uint32_t	cs;
};

enum {
	SPIBUS_IVAR_CS		/* chip select that we're on */
};

#define SPIBUS_ACCESSOR(A, B, T)					\
static inline int							\
spibus_get_ ## A(device_t dev, T *t)					\
{									\
	return BUS_READ_IVAR(device_get_parent(dev), dev,		\
	    SPIBUS_IVAR_ ## B, (uintptr_t *) t);			\
}
	
SPIBUS_ACCESSOR(cs,		CS,		uint32_t)
