/*	$FreeBSD: release/7.0.0/sys/contrib/ipfilter/netinet/ip_rules.h 172506 2007-10-10 16:59:15Z cvs2svn $	*/

extern int ipfrule_add __P((void));
extern int ipfrule_remove __P((void));

extern frentry_t *ipfrule_match_out_ __P((fr_info_t *, u_32_t *));
extern frentry_t *ipf_rules_out_[1];

extern int ipfrule_add_out_ __P((void));
extern int ipfrule_remove_out_ __P((void));

extern frentry_t *ipfrule_match_in_ __P((fr_info_t *, u_32_t *));
extern frentry_t *ipf_rules_in_[1];

extern int ipfrule_add_in_ __P((void));
extern int ipfrule_remove_in_ __P((void));
