/* $FreeBSD: release/10.0.0/tools/tools/ath/ath_ee_v4k_print/v4k.h 217770 2011-01-24 06:46:03Z adrian $ */

#ifndef	__V4K_H__
#define	__V4K_H__

extern void eeprom_v4k_base_print(uint16_t *buf);
extern void eeprom_v4k_custdata_print(uint16_t *buf);
extern void eeprom_v4k_modal_print(uint16_t *buf);
extern void eeprom_v4k_calfreqpiers_print(uint16_t *buf);
extern void eeprom_v4k_ctl_print(uint16_t *buf);
extern void eeprom_v4k_print_targets(uint16_t *buf);
extern void eeprom_v4k_print_edges(uint16_t *buf);
extern void eeprom_v4k_print_other(uint16_t *buf);

#endif
