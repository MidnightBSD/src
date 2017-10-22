/* report.h */
/* $FreeBSD: release/10.0.0/libexec/bootpd/report.h 97417 2002-05-28 18:36:43Z alfred $ */

extern void report_init(int nolog);
extern void report(int, const char *, ...) __printflike(2, 3);
extern const char *get_errmsg(void);
