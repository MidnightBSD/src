/* $FreeBSD: release/10.0.0/usr.sbin/ctm/ctm_rmail/error.h 76300 2001-05-06 03:03:45Z kris $ */

extern	void	err_set_log(char *log_file);
extern	void	err_prog_name(char *name);
extern	void	err(const char *fmt, ...) __printflike(1, 2);
