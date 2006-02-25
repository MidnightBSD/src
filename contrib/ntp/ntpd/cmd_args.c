/*
 * cmd_args.c = command-line argument processing
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ntpd.h"
#include "ntp_stdlib.h"
#include "ntp_cmdargs.h"

#ifdef SIM
#include "ntpsim.h"
#endif /* SIM */

/*
 * Definitions of things either imported from or exported to outside
 */
extern char const *progname;
int	listen_to_virtual_ips = 1;

#ifdef SYS_WINNT
extern BOOL NoWinService;
#endif

static const char *ntp_options = "aAbB:c:C:dD:f:gi:k:l:LmnNO:p:P:qr:s:S:t:T:W:u:v:V:xY:Z:-:";

#ifdef HAVE_NETINFO
extern int	check_netinfo;
#endif


/*
 * getstartup - search through the options looking for a debugging flag
 */
void
getstartup(
	int argc,
	char *argv[]
	)
{
	int errflg;
	extern int priority_done;
	int c;

#ifdef DEBUG
	debug = 0;		/* no debugging by default */
#endif

	/*
	 * This is a big hack.	We don't really want to read command line
	 * configuration until everything else is initialized, since
	 * the ability to configure the system may depend on storage
	 * and the like having been initialized.  Except that we also
	 * don't want to initialize anything until after detaching from
	 * the terminal, but we won't know to do that until we've
	 * parsed the command line.  Do that now, crudely, and do it
	 * again later.  Our ntp_getopt() is explicitly reusable, by the
	 * way.  Your own mileage may vary.
	 *
	 * This hack is even called twice (to allow complete logging to file)
	 */
	errflg = 0;
	progname = argv[0];

	/*
	 * Decode argument list
	 */
	while ((c = ntp_getopt(argc, argv, ntp_options)) != EOF)
	    switch (c) {
#ifdef DEBUG
		case 'd':
		    ++debug;
		    break;
		case 'D':
		    debug = (int)atol(ntp_optarg);
		    printf("Debug1: %s -> %x = %d\n", ntp_optarg, debug, debug);
		    break;
#else
		case 'd':
		case 'D':
		    msyslog(LOG_ERR, "ntpd not compiled with -DDEBUG option - no DEBUG support");
		    fprintf(stderr, "ntpd not compiled with -DDEBUG option - no DEBUG support\n");
		    ++errflg;
		    break;
#endif
		case 'L':
		    listen_to_virtual_ips = 0;
		    break;
		case 'l':
			{
				FILE *new_file;

				if(strcmp(ntp_optarg, "stderr") == 0)
					new_file = stderr;
				else if(strcmp(ntp_optarg, "stdout") == 0)
					new_file = stdout;
				else
					new_file = fopen(ntp_optarg, "a");
				if (new_file != NULL) {
					NLOG(NLOG_SYSINFO)
						msyslog(LOG_NOTICE, "logging to file %s", ntp_optarg);
					if (syslog_file != NULL &&
						fileno(syslog_file) != fileno(new_file))
						(void)fclose(syslog_file);

					syslog_file = new_file;
					syslogit = 0;
				}
				else
					msyslog(LOG_ERR,
						"Cannot open log file %s",
						ntp_optarg);
			}
			break;

		case 'n':
		case 'q':
		    ++nofork;
#ifdef SYS_WINNT
		    NoWinService = TRUE;	 
#endif
		    break;

		case 'N':
		    priority_done = 0;
		    break;
			
		case '?':
		    ++errflg;
		    break;

	    case '-':
	      if ( ! strcmp(ntp_optarg, "version") ) {
		printf("%.80s: %.80s\n", progname, Version);
		exit(0);
	      } else if ( ! strcmp(ntp_optarg, "help") ) {
		/* usage(); */
		/* exit(0); */
		++errflg;
	      } else if ( ! strcmp(ntp_optarg, "copyright") ) {
		printf("unknown\n");
		exit(0);
	      } else {
		fprintf(stderr, "%.80s: Error unknown argument '--%.80s'\n",
			progname,
			ntp_optarg);
		exit(12);
	      }
	      break;

		default:
			break;
		}

	if (errflg || ntp_optind != argc) {
		(void) fprintf(stderr, "usage: %s [ -abdgmnqx ] [ -c config_file ] [ -e e_delay ]\n", progname);
		(void) fprintf(stderr, "\t\t[ -f freq_file ] [ -k key_file ] [ -l log_file ]\n");
		(void) fprintf(stderr, "\t\t[ -p pid_file ] [ -r broad_delay ] [ -s statdir ]\n");
		(void) fprintf(stderr, "\t\t[ -t trust_key ] [ -v sys_var ] [ -V default_sysvar ]\n");
#if defined(HAVE_SCHED_SETSCHEDULER)
		(void) fprintf(stderr, "\t\t[ -P fixed_process_priority ]\n");
#endif
#ifdef HAVE_CLOCKCTL
		(void) fprintf(stderr, "\t\t[ -u user[:group] ] [ -i chrootdir ]\n");
#endif
		exit(2);
	}
	ntp_optind = 0;	/* reset ntp_optind to restart ntp_getopt */

#ifdef DEBUG
	if (debug) {
#ifdef HAVE_SETVBUF
		static char buf[BUFSIZ];
		setvbuf(stdout, buf, _IOLBF, BUFSIZ);
#else
		setlinebuf(stdout);
#endif
	}
#endif
}

/*
 * getCmdOpts - get command line options
 */
void
getCmdOpts(
	int argc,
	char *argv[]
	)
{
	extern char *config_file;
	struct sockaddr_in inaddrntp;
	int errflg;
	int c;

	/*
	 * Initialize, initialize
	 */
	errflg = 0;
#ifdef DEBUG
	debug = 0;
#endif	/* DEBUG */

	progname = argv[0];

	/*
	 * Decode argument list
	 */
	while ((c = ntp_getopt(argc, argv, ntp_options)) != EOF) {
		switch (c) {
		    case 'a':
			proto_config(PROTO_AUTHENTICATE, 1, 0., NULL);
			break;

		    case 'A':
			proto_config(PROTO_AUTHENTICATE, 0, 0., NULL);
			break;

		    case 'b':
			proto_config(PROTO_BROADCLIENT, 1, 0., NULL);
			break;

		    case 'c':
			config_file = ntp_optarg;
#ifdef HAVE_NETINFO
			check_netinfo = 0;
#endif
			break;

		    case 'd':
#ifdef DEBUG
			debug++;
#else
			errflg++;
#endif	/* DEBUG */
			break;

		    case 'D':
#ifdef DEBUG
			debug = (int)atol(ntp_optarg);
			printf("Debug2: %s -> %x = %d\n", ntp_optarg, debug, debug);
#else
			errflg++;
#endif	/* DEBUG */
			break;

		    case 'f':
			stats_config(STATS_FREQ_FILE, ntp_optarg);
			break;

		    case 'g':
			allow_panic = TRUE;
			break;

		    case 'i':
#ifdef HAVE_CLOCKCTL
			if (!ntp_optarg)
				errflg++;
			else
				chrootdir = ntp_optarg;
			break;
#else
			errflg++;
#endif
		    case 'k':
			getauthkeys(ntp_optarg);
			break;

		    case 'L':   /* already done at pre-scan */
		    case 'l':   /* already done at pre-scan */
			break;

		    case 'm':
			inaddrntp.sin_family = AF_INET;
			inaddrntp.sin_port = htons(NTP_PORT);
			inaddrntp.sin_addr.s_addr = htonl(INADDR_NTP);
			proto_config(PROTO_MULTICAST_ADD, 0, 0., (struct sockaddr_storage*)&inaddrntp);
			sys_bclient = 1;
			break;

		    case 'n':	/* already done at pre-scan */
			break;

		    case 'N':	/* already done at pre-scan */
			break;

		    case 'p':
			stats_config(STATS_PID_FILE, ntp_optarg);
			break;

		    case 'P':
#if defined(HAVE_SCHED_SETSCHEDULER)
			config_priority = (int)atol(ntp_optarg);
			config_priority_override = 1;
#else
			errflg++;
#endif
			break;

		    case 'q':
			mode_ntpdate = TRUE;
			break;

		    case 'r':
			do {
				double tmp;

				if (sscanf(ntp_optarg, "%lf", &tmp) != 1) {
					msyslog(LOG_ERR,
						"command line broadcast delay value %s undecodable",
						ntp_optarg);
				} else {
					proto_config(PROTO_BROADDELAY, 0, tmp, NULL);
				}
			} while (0);
			break;
			
		    case 'u':
#ifdef HAVE_CLOCKCTL
			user = malloc(strlen(ntp_optarg) + 1);
			if ((user == NULL) || (ntp_optarg == NULL))
				errflg++;
			(void)strncpy(user, ntp_optarg, strlen(ntp_optarg) + 1);
			group = rindex(user, ':');
			if (group)
				*group++ = '\0'; /* get rid of the ':' */
#else
			errflg++;
#endif
			break;
		    case 's':
			stats_config(STATS_STATSDIR, ntp_optarg);
			break;
			
		    case 't':
			do {
				u_long tkey;
				
				tkey = (int)atol(ntp_optarg);
				if (tkey <= 0 || tkey > NTP_MAXKEY) {
					msyslog(LOG_ERR,
					    "command line trusted key %s is invalid",
					    ntp_optarg);
				} else {
					authtrust(tkey, 1);
				}
			} while (0);
			break;

		    case 'v':
		    case 'V':
			set_sys_var(ntp_optarg, strlen(ntp_optarg)+1,
			    (u_short) (RW | ((c == 'V') ? DEF : 0)));
			break;

		    case 'x':
			clock_max = 600;
			break;
#ifdef SIM
		case 'B':
			sscanf(ntp_optarg, "%lf", &ntp_node.bdly);
                        break;

		case 'C':
			sscanf(ntp_optarg, "%lf", &ntp_node.snse);
                        break;

		case 'H':
			sscanf(ntp_optarg, "%lf", &ntp_node.slew);
                        break;

		case 'O':
			sscanf(ntp_optarg, "%lf", &ntp_node.clk_time);
                        break;

		case 'S':
			sscanf(ntp_optarg, "%lf", &ntp_node.sim_time);
                        break;

		case 'T':
			sscanf(ntp_optarg, "%lf", &ntp_node.ferr);
                        break;

		case 'W':
			sscanf(ntp_optarg, "%lf", &ntp_node.fnse);
                        break;

		case 'Y':
			sscanf(ntp_optarg, "%lf", &ntp_node.ndly);
                        break;

		case 'Z': 
			sscanf(ntp_optarg, "%lf", &ntp_node.pdly);
                        break;

#endif /* SIM */
		    default:
			errflg++;
			break;
		}
	}

	if (errflg || ntp_optind != argc) {
		(void) fprintf(stderr, "usage: %s [ -abdgmnx ] [ -c config_file ] [ -e e_delay ]\n", progname);
		(void) fprintf(stderr, "\t\t[ -f freq_file ] [ -k key_file ] [ -l log_file ]\n");
		(void) fprintf(stderr, "\t\t[ -p pid_file ] [ -r broad_delay ] [ -s statdir ]\n");
		(void) fprintf(stderr, "\t\t[ -t trust_key ] [ -v sys_var ] [ -V default_sysvar ]\n");
#if defined(HAVE_SCHED_SETSCHEDULER)
		(void) fprintf(stderr, "\t\t[ -P fixed_process_priority ]\n");
#endif
#ifdef HAVE_CLOCKCTL
		(void) fprintf(stderr, "\t\t[ -u user[:group] ] [ -i chrootdir ]\n");
#endif
		exit(2);
	}
	return;
}
