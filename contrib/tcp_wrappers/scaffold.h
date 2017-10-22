 /*
  * @(#) scaffold.h 1.3 94/12/31 18:19:19
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  *
  * $FreeBSD: release/7.0.0/contrib/tcp_wrappers/scaffold.h 172506 2007-10-10 16:59:15Z cvs2svn $
  */

#ifdef INET6
extern struct addrinfo *find_inet_addr();
#else
extern struct hostent *find_inet_addr();
#endif
extern int check_dns();
extern int check_path();
