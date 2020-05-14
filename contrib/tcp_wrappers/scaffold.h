 /*
  * @(#) scaffold.h 1.3 94/12/31 18:19:19
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  *
  * $FreeBSD: stable/11/contrib/tcp_wrappers/scaffold.h 350345 2019-07-26 02:49:34Z brooks $
  */

#ifdef INET6
extern struct addrinfo *find_inet_addr(char *host);
#else
extern struct hostent *find_inet_addr(char *host);
#endif
extern int check_dns(char *host);
extern int check_path(char *path, struct stat *st);
