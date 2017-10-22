/* $FreeBSD: release/10.0.0/tools/tools/ath/ath_ee_9287_print/9287.h 222322 2011-05-26 19:49:32Z adrian $ */

#ifndef	__9287_H__
#define	__9287_H__

extern void eeprom_9287_base_print(uint16_t *buf);
extern void eeprom_9287_custdata_print(uint16_t *buf);
extern void eeprom_9287_modal_print(uint16_t *buf);
extern void eeprom_9287_calfreqpiers_print(uint16_t *buf);
extern void eeprom_9287_ctl_print(uint16_t *buf);
extern void eeprom_9287_print_targets(uint16_t *buf);
extern void eeprom_9287_print_edges(uint16_t *buf);
extern void eeprom_9287_print_other(uint16_t *buf);

#endif
