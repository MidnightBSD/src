/*
 * $FreeBSD: release/10.0.0/sys/dev/stg/tmc18c30.h 194023 2009-06-11 17:14:28Z avg $
 */

extern devclass_t stg_devclass;

int	stg_alloc_resource	(device_t);
void	stg_release_resource	(device_t);
int	stg_probe		(device_t);
int	stg_attach		(device_t);
int	stg_detach		(device_t);
void	stg_intr		(void *);
