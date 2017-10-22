#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/xen/features.c 255040 2013-08-29 19:52:18Z gibbs $");

#include <sys/param.h>
#include <sys/systm.h>

#include <xen/xen-os.h>
#include <xen/hypervisor.h>
#include <xen/features.h>

uint8_t xen_features[XENFEAT_NR_SUBMAPS * 32] /* __read_mostly */;

void
setup_xen_features(void)
{
        xen_feature_info_t fi;
        int i, j;

        for (i = 0; i < XENFEAT_NR_SUBMAPS; i++) {
                fi.submap_idx = i;
                if (HYPERVISOR_xen_version(XENVER_get_features, &fi) < 0)
                        break;
                for (j = 0; j < 32; j++)
                        xen_features[i*32 + j] = !!(fi.submap & 1<<j);
        }
}
