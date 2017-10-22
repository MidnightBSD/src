/*
 * $FreeBSD: release/7.0.0/sys/dev/stg/tmc18c30.h 113205 2003-04-07 10:13:25Z mdodd $
 */

extern devclass_t stg_devclass;

int	stg_alloc_resource	(device_t);
void	stg_release_resource	(device_t);
int	stg_probe		(device_t);
int	stg_attach		(device_t);
void	stg_detach		(device_t);
void	stg_intr		(void *);
