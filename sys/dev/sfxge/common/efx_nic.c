/*-
 * Copyright 2007-2009 Solarflare Communications Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "efsys.h"
#include "efx.h"
#include "efx_types.h"
#include "efx_regs.h"
#include "efx_impl.h"

	__checkReturn	int
efx_family(
	__in		uint16_t venid,
	__in		uint16_t devid,
	__out		efx_family_t *efp)
{
#if EFSYS_OPT_FALCON
	if (venid == EFX_PCI_VENID_SFC && devid == EFX_PCI_DEVID_FALCON) {
		*efp = EFX_FAMILY_FALCON;
		return (0);
	}
#endif
#if EFSYS_OPT_SIENA
	if (venid == EFX_PCI_VENID_SFC && devid == EFX_PCI_DEVID_BETHPAGE) {
		*efp = EFX_FAMILY_SIENA;
		return (0);
	}
	if (venid == EFX_PCI_VENID_SFC && devid == EFX_PCI_DEVID_SIENA) {
		*efp = EFX_FAMILY_SIENA;
		return (0);
	}
	if (venid == EFX_PCI_VENID_SFC &&
	    devid == EFX_PCI_DEVID_SIENA_F1_UNINIT) {
		*efp = EFX_FAMILY_SIENA;
		return (0);
	}
#endif
	return (ENOTSUP);
}

/*
 * To support clients which aren't provided with any PCI context infer
 * the hardware family by inspecting the hardware. Obviously the caller
 * must be damn sure they're really talking to a supported device.
 */
	__checkReturn	int
efx_infer_family(
	__in		efsys_bar_t *esbp,
	__out		efx_family_t *efp)
{
	efx_family_t family;
	efx_oword_t oword;
	unsigned int portnum;
	int rc;

	EFSYS_BAR_READO(esbp, FR_AZ_CS_DEBUG_REG_OFST, &oword, B_TRUE);
	portnum = EFX_OWORD_FIELD(oword, FRF_CZ_CS_PORT_NUM);
	switch (portnum) {
#if EFSYS_OPT_FALCON
	case 0:
		family = EFX_FAMILY_FALCON;
		break;
#endif
#if EFSYS_OPT_SIENA
	case 1:
	case 2:
		family = EFX_FAMILY_SIENA;
		break;
#endif
	default:
		rc = ENOTSUP;
		goto fail1;
	}

	if (efp != NULL)
		*efp = family;
	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

/*
 * The built-in default value device id for port 1 of Siena is 0x0810.
 * manftest needs to be able to cope with that.
 */

#define	EFX_BIU_MAGIC0	0x01234567
#define	EFX_BIU_MAGIC1	0xfedcba98

static	__checkReturn	int
efx_nic_biu_test(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;
	int rc;

	/*
	 * Write magic values to scratch registers 0 and 1, then
	 * verify that the values were written correctly.  Interleave
	 * the accesses to ensure that the BIU is not just reading
	 * back the cached value that was last written.
	 */
	EFX_POPULATE_OWORD_1(oword, FRF_AZ_DRIVER_DW0, EFX_BIU_MAGIC0);
	EFX_BAR_TBL_WRITEO(enp, FR_AZ_DRIVER_REG, 0, &oword);

	EFX_POPULATE_OWORD_1(oword, FRF_AZ_DRIVER_DW0, EFX_BIU_MAGIC1);
	EFX_BAR_TBL_WRITEO(enp, FR_AZ_DRIVER_REG, 1, &oword);

	EFX_BAR_TBL_READO(enp, FR_AZ_DRIVER_REG, 0, &oword);
	if (EFX_OWORD_FIELD(oword, FRF_AZ_DRIVER_DW0) != EFX_BIU_MAGIC0) {
		rc = EIO;
		goto fail1;
	}

	EFX_BAR_TBL_READO(enp, FR_AZ_DRIVER_REG, 1, &oword);
	if (EFX_OWORD_FIELD(oword, FRF_AZ_DRIVER_DW0) != EFX_BIU_MAGIC1) {
		rc = EIO;
		goto fail2;
	}

	/*
	 * Perform the same test, with the values swapped.  This
	 * ensures that subsequent tests don't start with the correct
	 * values already written into the scratch registers.
	 */
	EFX_POPULATE_OWORD_1(oword, FRF_AZ_DRIVER_DW0, EFX_BIU_MAGIC1);
	EFX_BAR_TBL_WRITEO(enp, FR_AZ_DRIVER_REG, 0, &oword);

	EFX_POPULATE_OWORD_1(oword, FRF_AZ_DRIVER_DW0, EFX_BIU_MAGIC0);
	EFX_BAR_TBL_WRITEO(enp, FR_AZ_DRIVER_REG, 1, &oword);

	EFX_BAR_TBL_READO(enp, FR_AZ_DRIVER_REG, 0, &oword);
	if (EFX_OWORD_FIELD(oword, FRF_AZ_DRIVER_DW0) != EFX_BIU_MAGIC1) {
		rc = EIO;
		goto fail3;
	}

	EFX_BAR_TBL_READO(enp, FR_AZ_DRIVER_REG, 1, &oword);
	if (EFX_OWORD_FIELD(oword, FRF_AZ_DRIVER_DW0) != EFX_BIU_MAGIC0) {
		rc = EIO;
		goto fail4;
	}

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_FALCON

static efx_nic_ops_t	__cs __efx_nic_falcon_ops = {
	falcon_nic_probe,		/* eno_probe */
	falcon_nic_reset,		/* eno_reset */
	falcon_nic_init,		/* eno_init */
#if EFSYS_OPT_DIAG
	falcon_sram_test,		/* eno_sram_test */
	falcon_nic_register_test,	/* eno_register_test */
#endif	/* EFSYS_OPT_DIAG */
	falcon_nic_fini,		/* eno_fini */
	falcon_nic_unprobe,		/* eno_unprobe */
};

#endif	/* EFSYS_OPT_FALCON */

#if EFSYS_OPT_SIENA

static efx_nic_ops_t	__cs __efx_nic_siena_ops = {
	siena_nic_probe,		/* eno_probe */
	siena_nic_reset,		/* eno_reset */
	siena_nic_init,			/* eno_init */
#if EFSYS_OPT_DIAG
	siena_sram_test,		/* eno_sram_test */
	siena_nic_register_test,	/* eno_register_test */
#endif	/* EFSYS_OPT_DIAG */
	siena_nic_fini,			/* eno_fini */
	siena_nic_unprobe,		/* eno_unprobe */
};

#endif	/* EFSYS_OPT_SIENA */

	__checkReturn	int
efx_nic_create(
	__in		efx_family_t family,
	__in		efsys_identifier_t *esip,
	__in		efsys_bar_t *esbp,
	__in		efsys_lock_t *eslp,
	__deref_out	efx_nic_t **enpp)
{
	efx_nic_t *enp;
	int rc;

	EFSYS_ASSERT3U(family, >, EFX_FAMILY_INVALID);
	EFSYS_ASSERT3U(family, <, EFX_FAMILY_NTYPES);

	/* Allocate a NIC object */
	EFSYS_KMEM_ALLOC(esip, sizeof (efx_nic_t), enp);

	if (enp == NULL) {
		rc = ENOMEM;
		goto fail1;
	}

	enp->en_magic = EFX_NIC_MAGIC;

	switch (family) {
#if EFSYS_OPT_FALCON
	case EFX_FAMILY_FALCON:
		enp->en_enop = (efx_nic_ops_t *)&__efx_nic_falcon_ops;
		enp->en_features = 0;
		break;
#endif	/* EFSYS_OPT_FALCON */

#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		enp->en_enop = (efx_nic_ops_t *)&__efx_nic_siena_ops;
		enp->en_features = EFX_FEATURE_IPV6 |
		    EFX_FEATURE_LFSR_HASH_INSERT |
		    EFX_FEATURE_LINK_EVENTS | EFX_FEATURE_PERIODIC_MAC_STATS |
		    EFX_FEATURE_WOL | EFX_FEATURE_MCDI |
		    EFX_FEATURE_LOOKAHEAD_SPLIT | EFX_FEATURE_MAC_HEADER_FILTERS;
		break;
#endif	/* EFSYS_OPT_SIENA */

	default:
		rc = ENOTSUP;
		goto fail2;
	}

	enp->en_family = family;
	enp->en_esip = esip;
	enp->en_esbp = esbp;
	enp->en_eslp = eslp;

	*enpp = enp;

	return (0);

fail2:
	EFSYS_PROBE(fail3);

	enp->en_magic = 0;

	/* Free the NIC object */
	EFSYS_KMEM_FREE(esip, sizeof (efx_nic_t), enp);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
efx_nic_probe(
	__in		efx_nic_t *enp)
{
	efx_nic_ops_t *enop;
	efx_oword_t oword;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
#if EFSYS_OPT_MCDI
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
#endif	/* EFSYS_OPT_MCDI */
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_PROBE));

	/* Test BIU */
	if ((rc = efx_nic_biu_test(enp)) != 0)
		goto fail1;

	/* Clear the region register */
	EFX_POPULATE_OWORD_4(oword,
	    FRF_AZ_ADR_REGION0, 0,
	    FRF_AZ_ADR_REGION1, (1 << 16),
	    FRF_AZ_ADR_REGION2, (2 << 16),
	    FRF_AZ_ADR_REGION3, (3 << 16));
	EFX_BAR_WRITEO(enp, FR_AZ_ADR_REGION_REG, &oword);

	enop = enp->en_enop;
	if ((rc = enop->eno_probe(enp)) != 0)
		goto fail2;

	if ((rc = efx_phy_probe(enp)) != 0)
		goto fail3;

	enp->en_mod_flags |= EFX_MOD_PROBE;

	return (0);

fail3:
	EFSYS_PROBE(fail3);

	enop->eno_unprobe(enp);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_PCIE_TUNE

	__checkReturn	int
efx_nic_pcie_tune(
	__in		efx_nic_t *enp,
	unsigned int	nlanes)
{
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_NIC));

#if EFSYS_OPT_FALCON
	if (enp->en_family == EFX_FAMILY_FALCON)
		return (falcon_nic_pcie_tune(enp, nlanes));
#endif
	return (ENOTSUP);
}

	__checkReturn	int
efx_nic_pcie_extended_sync(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_NIC));

#if EFSYS_OPT_SIENA
	if (enp->en_family == EFX_FAMILY_SIENA)
		return (siena_nic_pcie_extended_sync(enp));
#endif

	return (ENOTSUP);
}

#endif	/* EFSYS_OPT_PCIE_TUNE */

	__checkReturn	int
efx_nic_init(
	__in		efx_nic_t *enp)
{
	efx_nic_ops_t *enop = enp->en_enop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);

	if (enp->en_mod_flags & EFX_MOD_NIC) {
		rc = EINVAL;
		goto fail1;
	}

	if ((rc = enop->eno_init(enp)) != 0)
		goto fail2;

	enp->en_mod_flags |= EFX_MOD_NIC;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

			void
efx_nic_fini(
	__in		efx_nic_t *enp)
{
	efx_nic_ops_t *enop = enp->en_enop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROBE);
	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_NIC);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_INTR));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_EV));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_RX));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_TX));

	enop->eno_fini(enp);

	enp->en_mod_flags &= ~EFX_MOD_NIC;
}

			void
efx_nic_unprobe(
	__in		efx_nic_t *enp)
{
	efx_nic_ops_t *enop = enp->en_enop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
#if EFSYS_OPT_MCDI
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
#endif	/* EFSYS_OPT_MCDI */
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_NIC));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_INTR));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_EV));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_RX));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_TX));

	efx_phy_unprobe(enp);

	enop->eno_unprobe(enp);

	enp->en_mod_flags &= ~EFX_MOD_PROBE;
}

			void
efx_nic_destroy(
	__in	efx_nic_t *enp)
{
	efsys_identifier_t *esip = enp->en_esip;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, ==, 0);

	enp->en_family = 0;
	enp->en_esip = NULL;
	enp->en_esbp = NULL;
	enp->en_eslp = NULL;

	enp->en_enop = NULL;

	enp->en_magic = 0;

	/* Free the NIC object */
	EFSYS_KMEM_FREE(esip, sizeof (efx_nic_t), enp);
}

	__checkReturn	int
efx_nic_reset(
	__in		efx_nic_t *enp)
{
	efx_nic_ops_t *enop = enp->en_enop;
	unsigned int mod_flags;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROBE);
	/*
	 * All modules except the MCDI, PROBE, NVRAM, VPD, MON (which we
	 * do not reset here) must have been shut down or never initialized.
	 *
	 * A rule of thumb here is: If the controller or MC reboots, is *any*
	 * state lost. If it's lost and needs reapplying, then the module
	 * *must* not be initialised during the reset.
	 */
	mod_flags = enp->en_mod_flags;
	mod_flags &= ~(EFX_MOD_MCDI | EFX_MOD_PROBE | EFX_MOD_NVRAM |
		    EFX_MOD_VPD | EFX_MOD_MON);
	EFSYS_ASSERT3U(mod_flags, ==, 0);
	if (mod_flags != 0) {
		rc = EINVAL;
		goto fail1;
	}

	if ((rc = enop->eno_reset(enp)) != 0)
		goto fail2;

	enp->en_reset_flags |= EFX_RESET_MAC;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

			const efx_nic_cfg_t *
efx_nic_cfg_get(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	return (&(enp->en_nic_cfg));
}

#if EFSYS_OPT_DIAG

	__checkReturn	int
efx_nic_register_test(
	__in		efx_nic_t *enp)
{
	efx_nic_ops_t *enop = enp->en_enop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_NIC));

	if ((rc = enop->eno_register_test(enp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
efx_nic_test_registers(
	__in		efx_nic_t *enp,
	__in		efx_register_set_t *rsp,
	__in		size_t count)
{
	unsigned int bit;
	efx_oword_t original;
	efx_oword_t reg;
	efx_oword_t buf;
	int rc;

	while (count > 0) {
		/* This function is only suitable for registers */
		EFSYS_ASSERT(rsp->rows == 1);

		/* bit sweep on and off */
		EFSYS_BAR_READO(enp->en_esbp, rsp->address, &original,
			    B_TRUE);
		for (bit = 0; bit < 128; bit++) {
			/* Is this bit in the mask? */
			if (~(rsp->mask.eo_u32[bit >> 5]) & (1 << bit))
				continue;

			/* Test this bit can be set in isolation */
			reg = original;
			EFX_AND_OWORD(reg, rsp->mask);
			EFX_SET_OWORD_BIT(reg, bit);

			EFSYS_BAR_WRITEO(enp->en_esbp, rsp->address, &reg,
				    B_TRUE);
			EFSYS_BAR_READO(enp->en_esbp, rsp->address, &buf,
				    B_TRUE);

			EFX_AND_OWORD(buf, rsp->mask);
			if (memcmp(&reg, &buf, sizeof (reg))) {
				rc = EIO;
				goto fail1;
			}

			/* Test this bit can be cleared in isolation */
			EFX_OR_OWORD(reg, rsp->mask);
			EFX_CLEAR_OWORD_BIT(reg, bit);

			EFSYS_BAR_WRITEO(enp->en_esbp, rsp->address, &reg,
				    B_TRUE);
			EFSYS_BAR_READO(enp->en_esbp, rsp->address, &buf,
				    B_TRUE);

			EFX_AND_OWORD(buf, rsp->mask);
			if (memcmp(&reg, &buf, sizeof (reg))) {
				rc = EIO;
				goto fail2;
			}
		}

		/* Restore the old value */
		EFSYS_BAR_WRITEO(enp->en_esbp, rsp->address, &original,
			    B_TRUE);

		--count;
		++rsp;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	/* Restore the old value */
	EFSYS_BAR_WRITEO(enp->en_esbp, rsp->address, &original, B_TRUE);

	return (rc);
}

	__checkReturn	int
efx_nic_test_tables(
	__in		efx_nic_t *enp,
	__in		efx_register_set_t *rsp,
	__in		efx_pattern_type_t pattern,
	__in		size_t count)
{
	efx_sram_pattern_fn_t func;
	unsigned int index;
	unsigned int address;
	efx_oword_t reg;
	efx_oword_t buf;
	int rc;

	EFSYS_ASSERT(pattern < EFX_PATTERN_NTYPES);
	func = __efx_sram_pattern_fns[pattern];

	while (count > 0) {
		/* Write */
		address = rsp->address;
		for (index = 0; index < rsp->rows; ++index) {
			func(2 * index + 0, B_FALSE, &reg.eo_qword[0]);
			func(2 * index + 1, B_FALSE, &reg.eo_qword[1]);
			EFX_AND_OWORD(reg, rsp->mask);
			EFSYS_BAR_WRITEO(enp->en_esbp, address, &reg, B_TRUE);

			address += rsp->step;
		}

		/* Read */
		address = rsp->address;
		for (index = 0; index < rsp->rows; ++index) {
			func(2 * index + 0, B_FALSE, &reg.eo_qword[0]);
			func(2 * index + 1, B_FALSE, &reg.eo_qword[1]);
			EFX_AND_OWORD(reg, rsp->mask);
			EFSYS_BAR_READO(enp->en_esbp, address, &buf, B_TRUE);
			if (memcmp(&reg, &buf, sizeof (reg))) {
				rc = EIO;
				goto fail1;
			}

			address += rsp->step;
		}

		++rsp;
		--count;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_DIAG */
