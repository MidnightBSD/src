/* $Id: fclone.c,v 1.1 2009-12-13 01:09:42 laffer1 Exp $ */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>

static d_open_t		fclone_open;
static d_close_t	fclone_close;
static d_read_t		fclone_read;
static struct cdevsw fclone_cdevsw = {
	.d_open =	fclone_open,
	.d_close =	fclone_close,
	.d_read =	fclone_read,	
	.d_name =	"fclone",
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
};

MALLOC_DEFINE(M_FCLONESC, "fclone memory", "fclone memory");

struct fclone_sc
{
	int pos;
};

static int
fclone_open(struct cdev *dev, int oflags, int devtype, d_thread_t *td)
{
	dev->si_drv2 = malloc(sizeof(struct fclone_sc), M_FCLONESC,
	    M_WAITOK | M_ZERO);

	return (0);
}

static int
fclone_close(struct cdev *dev, int fflag, int devtype, d_thread_t *td)
{
	void *x;

	x = dev->si_drv2;
	free(x, M_FCLONESC);
	return (0);
}

static char rdata[] = "fclone sample data string\n";

static int
fclone_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct fclone_sc *sc;
	int rv, amnt;
	
	sc = dev->si_drv2;
	rv = 0;
	while (uio->uio_resid > 0) {
		amnt = MIN(uio->uio_resid, sizeof(rdata) - sc->pos);
		rv = uiomove(rdata + sc->pos, amnt, uio);
		if (rv != 0)
			break;
		sc->pos += amnt;
		sc->pos %= sizeof(rdata);
	}
	return (rv);
}

static d_fdopen_t	fmaster_fdopen;
static d_close_t	fmaster_close;
static struct cdevsw fmaster_cdevsw = {
	.d_fdopen =	fmaster_fdopen,
	.d_close =	fmaster_close,
	.d_name =	"fmaster",
	.d_version =	D_VERSION,
};

static int
fmaster_fdopen(struct cdev *dev, int oflags, struct thread *td, struct file *fp)
{
	struct cdev *clone;
	int res;

	res = fdclone(&fclone_cdevsw, fp, oflags, &clone, dev, td);
	return (res);
}

static int
fmaster_close(struct cdev *dev, int fflags, int devtype, d_thread_t *td)
{
	return (0);
}

static int
fclone_modevent(module_t mod, int what, void *arg)
{
	static struct cdev *fmaster_dev;

	switch (what) {
        case MOD_LOAD:
		fmaster_dev = make_dev(&fmaster_cdevsw, 0, UID_ROOT, GID_WHEEL,
		    0666, "fmaster");
		if (fmaster_dev == NULL)
			return (ENOMEM);
		return(0);
        case MOD_UNLOAD:
		if (fmaster_dev)
			destroy_dev(fmaster_dev);
		destroy_dev_drain(&fclone_cdevsw);
		return (0);
        default:
		break;
	}

	return (0);
}

moduledata_t fclone_mdata = {
	"fclone",
	fclone_modevent,
	NULL
};

DECLARE_MODULE(fclone, fclone_mdata, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(fclone, 1);
