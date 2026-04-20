/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The MidnightBSD Project
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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

#ifndef PROWLD_H
#define PROWLD_H

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/*
 * Forward typedefs so prowl_socket_t can reference job_t and vice versa.
 * The full struct bodies appear later in this file.
 */
typedef struct job job_t;
typedef struct prowl_socket prowl_socket_t;

/*
 * Timer ident encoding (low 2 bits of job pointer).
 * Assumes job_t pointers are at least 4-byte aligned.
 */
#define TIMER_MASK	3UL
#define TIMER_THROTTLE	0UL
#define TIMER_STOP	1UL
#define TIMER_WATCHDOG	2UL
#define NOTIFY_TMO	3UL

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#include <assert.h>
/* Ensure the tagging trick is safe for this architecture */
static_assert((__alignof__(struct job) & TIMER_MASK) == 0,
    "struct job alignment insufficient for timer tagging");
#endif

/* String size limits */
#define PROWL_LABEL_MAX		256
#define PROWL_PATH_MAX		1024
#define PROWL_DESC_MAX		512
#define PROWL_PROVIDES_MAX	16
#define PROWL_DEPS_MAX		32
#define PROWL_ARGS_MAX		64
#define PROWL_ENV_MAX		64
#define PROWL_GROUPS_MAX	32

/* Runtime paths */
#define PROWLD_SOCK_PATH	"/var/run/prowld/prowld.sock"
#define PROWLD_PID_PATH		"/var/run/prowld/prowld.pid"
#define PROWLD_RUN_DIR		"/var/run/prowld"
#define PROWLD_NOTIFY_DIR	"/var/run/prowld/notify"
#define PROWLD_CONF_PATH	"/etc/prowld/prowld.conf"
#define PROWLD_MASK_DIR		"/var/db/prowld/masked.d"
#define PROWLD_DB_DIR		"/var/db/prowld"
#define PROWLD_LOG_DIR		"/var/log/prowld"
#define PROWLD_JOB_LOG_DIR	"/var/log/prowld/jobs"

/* Unit file directories (searched in order; later overrides earlier) */
#define UNIT_DIR_BASE		"/etc/prowld/units.d"
#define UNIT_DIR_LOCAL		"/usr/local/etc/prowld/units.d"
#define UNIT_DIR_OVERRIDE	"/etc/prowld/overrides.d"
#define UNIT_DIR_GENERATED	"/var/run/prowld/generated.d"

/*
 * Any persistent state files (e.g., timer state) MUST be written atomically:
 *   write to <path>.tmp -> fsync -> rename(<path>.tmp, <path>)
 */

/* rc.d script directories */
#define RCD_DIR_BASE		"/etc/rc.d"
#define RCD_DIR_LOCAL		"/usr/local/etc/rc.d"

/* rc.conf paths */
#define RCCONF_PATH		"/etc/rc.conf"
#define RCCONF_LOCAL_PATH	"/etc/rc.conf.local"

/* Default concurrency: overridden by prowld.conf */
#define DEFAULT_MAX_STARTS	0	/* 0 = auto (ncpu*2) */
#define DEFAULT_SHUTDOWN_TMO	300	/* seconds */
#define DEFAULT_THROTTLE_INT	10	/* seconds */
#define DEFAULT_EXIT_TIMEOUT	20	/* seconds */

/* IPC */
#define IPC_MAX_CLIENTS		32
#define IPC_MSG_MAX		65536	/* max JSON message size */

/* Job types */
typedef enum {
	JOB_TYPE_DAEMON  = 0,
	JOB_TYPE_ONESHOT = 1,
	JOB_TYPE_TIMER   = 2,
	JOB_TYPE_SOCKET  = 3,
	JOB_TYPE_RCSHIM  = 4,
} job_type_t;

/* Job states */
typedef enum {
	JOB_STATE_UNKNOWN   = 0,
	JOB_STATE_LOADED    = 1,
	JOB_STATE_DISABLED  = 2,
	JOB_STATE_MASKED    = 3,
	JOB_STATE_STARTING  = 4,
	JOB_STATE_RUNNING   = 5,
	JOB_STATE_STOPPING  = 6,
	JOB_STATE_STOPPED   = 7,
	JOB_STATE_FAILED    = 8,
	JOB_STATE_COMPLETED = 9,	/* oneshots only */
} job_state_t;

/* Notify types */
typedef enum {
	NOTIFY_NONE   = 0,
	NOTIFY_NOTIFY = 1,
} notify_type_t;

/* Socket activation */
#define PROWL_SOCK_NAME_MAX	64
#define PROWL_SOCKETS_MAX	8

struct prowl_socket {
	char	name[PROWL_SOCK_NAME_MAX]; /* key from unit file sockets block */
	char	path[PROWL_PATH_MAX];      /* AF_UNIX socket path */
	char	host[256];                 /* bind host for inet sockets */
	int	port;                      /* TCP/UDP port; 0 = AF_UNIX */
	int	socktype;                  /* SOCK_STREAM or SOCK_DGRAM */
	int	family;                    /* AF_UNIX, AF_INET, AF_INET6 */
	int	backlog;                   /* listen backlog (0 = SOMAXCONN) */
	int	fd;                        /* bound fd; -1 = not yet bound */
	job_t  *job;                       /* owning job (back-pointer) */
};

/* Keep-alive policy */
typedef struct keep_alive {
	bool	always;			/* always restart */
	bool	crashed;		/* restart on non-zero exit */
	bool	successful_exit;	/* restart even on clean exit */
} keep_alive_t;

/* Resource limits */
typedef struct rlimits {
	bool		set_nofile;
	rlim_t		nofile;
	bool		set_nproc;
	rlim_t		nproc;
} rlimits_t;

/* Dependency entry */
typedef struct dep_entry {
	char	name[PROWL_LABEL_MAX];
	bool	hard;		/* true = requires, false = wants */
} dep_entry_t;

/* Job structure (forward-declared as job_t above) */
struct job {
	/* Identity */
	char		label[PROWL_LABEL_MAX];
	char		description[PROWL_DESC_MAX];
	job_type_t	type;
	char		rc_name[PROWL_LABEL_MAX];

	/* Execution */
	char		program[PROWL_PATH_MAX];
	char	       *arguments[PROWL_ARGS_MAX];
	int		argc;
	char	       *envp[PROWL_ENV_MAX];
	int		envc;
	char		working_directory[PROWL_PATH_MAX];
	char		root_directory[PROWL_PATH_MAX];

	/* Privilege */
	char		user[64];
	char		group[64];
	mode_t		umask_val;
	bool		umask_set;

	/* Pre-resolved privilege info (set in parent before fork) */
	uid_t		run_uid;
	gid_t		run_gid;
	gid_t		run_groups[PROWL_GROUPS_MAX];
	int		run_ngroups;
	bool		run_priv_set;

	/* Lifecycle */
	bool		run_at_load;
	keep_alive_t	keep_alive;
	int		throttle_interval;
	int		exit_timeout;

	/* Dependencies */
	dep_entry_t	deps[PROWL_DEPS_MAX];	/* requires + wants merged */
	int		deps_count;
	char		before[PROWL_DEPS_MAX][PROWL_LABEL_MAX];
	int		before_count;
	char		after[PROWL_DEPS_MAX][PROWL_LABEL_MAX];
	int		after_count;
	char		conflicts[PROWL_DEPS_MAX][PROWL_LABEL_MAX];
	int		conflicts_count;
	char		provides[PROWL_PROVIDES_MAX][PROWL_LABEL_MAX];
	int		provides_count;

	/* I/O */
	char		stdin_path[PROWL_PATH_MAX];
	char		stdout_path[PROWL_PATH_MAX];
	char		stderr_path[PROWL_PATH_MAX];

	/* Resources */
	int		nice_val;
	rlimits_t	rlimits;

	/* Notification */
	notify_type_t	notify_type;
	int		watchdog_sec;
	int		notify_fd;

	/* Shutdown */
	bool		shutdown_wait;

	/* rc-shim specific */
	char		rcshim_path[PROWL_PATH_MAX];

	/* Runtime state */
	job_state_t	state;
	pid_t		pid;
	int		last_exit_status;
	int		restart_count;
	time_t		started_at;
	time_t		exited_at;
	char		status_msg[256];

	/* Enable state (from rc.conf) */
	bool		enabled;

	/* DAG bookkeeping */
	int		dag_indegree;
	bool		dag_visited;
	bool		dag_in_stack;

	/* Timer tracking */
	bool		throttle_timer_active;
	bool		stop_timer_active;
	bool		watchdog_timer_active;
	time_t		watchdog_last_ping;

	/* Socket activation */
	prowl_socket_t	sockets[PROWL_SOCKETS_MAX];
	int		sockets_count;
	bool		socket_activated;	/* daemon has been fork+exec'd */

	TAILQ_ENTRY(job) entries;
};

TAILQ_HEAD(job_list, job);

/* IPC client state */
typedef struct ipc_client {
	int		fd;
	char		buf[IPC_MSG_MAX];
	size_t		buf_used;
	bool		active;
	uid_t		peer_uid;	/* from getpeereid(2) at accept time */
	gid_t		peer_gid;
} ipc_client_t;

/* Prowld daemon configuration */
typedef struct prowld_config {
	int	max_concurrent_starts;
	int	shutdown_timeout;
	bool	debug;
} prowld_config_t;

/* ---- Global state (defined in prowld.c) ---- */
extern struct job_list	 g_jobs;
extern prowld_config_t	 g_config;
extern int		 g_kqueue_fd;
extern volatile bool	 g_shutdown;
extern volatile bool	 g_reload;
extern int		 g_current_starts;

/*
 * Runtime path globals.  Defaults match the compile-time macros but may be
 * overridden by the -s flag or auto-detected fallback on read-only media.
 * All code must use these globals rather than the macro constants directly.
 */
extern char		 g_run_dir[PROWL_PATH_MAX];
extern char		 g_notify_dir[PROWL_PATH_MAX];
extern char		 g_sock_path[PROWL_PATH_MAX];
extern char		 g_pid_path[PROWL_PATH_MAX];
extern char		 g_db_dir[PROWL_PATH_MAX];
extern char		 g_mask_dir[PROWL_PATH_MAX];
extern char		 g_log_dir[PROWL_PATH_MAX];
extern char		 g_job_log_dir[PROWL_PATH_MAX];
extern char		 g_generated_dir[PROWL_PATH_MAX];

/* ---- IPC state (defined in ipc.c) ---- */
extern int		 ipc_listen_fd;

/* ---- job.c ---- */
job_t		*job_alloc(void);
void		 job_free(job_t *);
job_t		*job_find_by_label(const char *);
job_t		*job_find_by_rcname(const char *);
job_t		*job_find_by_provides(const char *);
bool		 job_provides(const job_t *, const char *);
void		 job_set_state(job_t *, job_state_t);
const char	*job_state_str(job_state_t);
const char	*job_type_str(job_type_t);
bool		 job_is_masked(const job_t *);
bool		 job_dep_satisfied(const job_t *, const dep_entry_t *);
bool		 job_all_deps_satisfied(const job_t *);
bool		 label_valid(const char *);

/* ---- unit.c ---- */
int	unit_load_dir(const char *);
int	unit_load_file(const char *);

/* ---- rcshim.c ---- */
int	rcshim_scan_dir(const char *);
bool	rcconf_is_enabled(const char *, bool);

/* ---- dag.c ---- */
int	dag_build(void);
void	dag_schedule_ready(void);

/* ---- supervisor.c ---- */
int	supervisor_start(job_t *);
int	supervisor_stop(job_t *, bool);
int	supervisor_signal(job_t *, int);
void	supervisor_reap(pid_t, int);
void	supervisor_handle_throttle(job_t *);
void	supervisor_handle_stop_timeout(job_t *);
void	supervisor_handle_notify(job_t *);
void	supervisor_handle_watchdog(job_t *);
void	supervisor_handle_notify_timeout(job_t *);
void	supervisor_socket_activate(job_t *);
void	supervisor_shutdown_all(void);

/* ---- socket_activation.c ---- */
int	socket_bind_all(job_t *);
void	socket_handle_activation(prowl_socket_t *);
void	socket_rearm(job_t *);
void	socket_close_all(job_t *);

/* ---- ipc.c ---- */
int	ipc_init(void);
void	ipc_shutdown(void);
void	ipc_accept(void);
void	ipc_read_client(int);
void	ipc_close_client(int);

/* ---- logger ---- */
void	prowl_log(int, const char *, ...)
    __attribute__((format(printf, 2, 3)));

#endif /* PROWLD_H */
