/*-
 * Copyright (c) 2012 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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
 *
 * $FreeBSD: release/10.0.0/usr.bin/iscsictl/iscsictl.c 258131 2013-11-14 13:33:22Z trasz $
 */

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/linker.h>
#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iscsi_ioctl.h>
#include "iscsictl.h"

struct conf *
conf_new(void)
{
	struct conf *conf;

	conf = calloc(1, sizeof(*conf));
	if (conf == NULL)
		err(1, "calloc");

	TAILQ_INIT(&conf->conf_targets);

	return (conf);
}

struct target *
target_find(struct conf *conf, const char *nickname)
{
	struct target *targ;

	TAILQ_FOREACH(targ, &conf->conf_targets, t_next) {
		if (targ->t_nickname != NULL &&
		    strcasecmp(targ->t_nickname, nickname) == 0)
			return (targ);
	}

	return (NULL);
}

struct target *
target_new(struct conf *conf)
{
	struct target *targ;

	targ = calloc(1, sizeof(*targ));
	if (targ == NULL)
		err(1, "calloc");
	targ->t_conf = conf;
	TAILQ_INSERT_TAIL(&conf->conf_targets, targ, t_next);

	return (targ);
}

void
target_delete(struct target *targ)
{

	TAILQ_REMOVE(&targ->t_conf->conf_targets, targ, t_next);
	free(targ);
}


static char *
default_initiator_name(void)
{
	char *name;
	size_t namelen;
	int error;

	namelen = _POSIX_HOST_NAME_MAX + strlen(DEFAULT_IQN);

	name = calloc(1, namelen + 1);
	if (name == NULL)
		err(1, "calloc");
	strcpy(name, DEFAULT_IQN);
	error = gethostname(name + strlen(DEFAULT_IQN),
	    namelen - strlen(DEFAULT_IQN));
	if (error != 0)
		err(1, "gethostname");

	return (name);
}

static bool
valid_hex(const char ch)
{
	switch (ch) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case 'a':
	case 'A':
	case 'b':
	case 'B':
	case 'c':
	case 'C':
	case 'd':
	case 'D':
	case 'e':
	case 'E':
	case 'f':
	case 'F':
		return (true);
	default:
		return (false);
	}
}

bool
valid_iscsi_name(const char *name)
{
	int i;

	if (strlen(name) >= MAX_NAME_LEN) {
		warnx("overlong name for \"%s\"; max length allowed "
		    "by iSCSI specification is %d characters",
		    name, MAX_NAME_LEN);
		return (false);
	}

	/*
	 * In the cases below, we don't return an error, just in case the admin
	 * was right, and we're wrong.
	 */
	if (strncasecmp(name, "iqn.", strlen("iqn.")) == 0) {
		for (i = strlen("iqn."); name[i] != '\0'; i++) {
			/*
			 * XXX: We should verify UTF-8 normalisation, as defined
			 * 	by 3.2.6.2: iSCSI Name Encoding.
			 */
			if (isalnum(name[i]))
				continue;
			if (name[i] == '-' || name[i] == '.' || name[i] == ':')
				continue;
			warnx("invalid character \"%c\" in iSCSI name "
			    "\"%s\"; allowed characters are letters, digits, "
			    "'-', '.', and ':'", name[i], name);
			break;
		}
		/*
		 * XXX: Check more stuff: valid date and a valid reversed domain.
		 */
	} else if (strncasecmp(name, "eui.", strlen("eui.")) == 0) {
		if (strlen(name) != strlen("eui.") + 16)
			warnx("invalid iSCSI name \"%s\"; the \"eui.\" "
			    "should be followed by exactly 16 hexadecimal "
			    "digits", name);
		for (i = strlen("eui."); name[i] != '\0'; i++) {
			if (!valid_hex(name[i])) {
				warnx("invalid character \"%c\" in iSCSI "
				    "name \"%s\"; allowed characters are 1-9 "
				    "and A-F", name[i], name);
				break;
			}
		}
	} else if (strncasecmp(name, "naa.", strlen("naa.")) == 0) {
		if (strlen(name) > strlen("naa.") + 32)
			warnx("invalid iSCSI name \"%s\"; the \"naa.\" "
			    "should be followed by at most 32 hexadecimal "
			    "digits", name);
		for (i = strlen("naa."); name[i] != '\0'; i++) {
			if (!valid_hex(name[i])) {
				warnx("invalid character \"%c\" in ISCSI "
				    "name \"%s\"; allowed characters are 1-9 "
				    "and A-F", name[i], name);
				break;
			}
		}
	} else {
		warnx("invalid iSCSI name \"%s\"; should start with "
		    "either \".iqn\", \"eui.\", or \"naa.\"",
		    name);
	}
	return (true);
}

void
conf_verify(struct conf *conf)
{
	struct target *targ;

	TAILQ_FOREACH(targ, &conf->conf_targets, t_next) {
		assert(targ->t_nickname != NULL);
		if (targ->t_session_type == SESSION_TYPE_UNSPECIFIED)
			targ->t_session_type = SESSION_TYPE_NORMAL;
		if (targ->t_session_type == SESSION_TYPE_NORMAL &&
		    targ->t_name == NULL)
			errx(1, "missing TargetName for target \"%s\"",
			    targ->t_nickname);
		if (targ->t_session_type == SESSION_TYPE_DISCOVERY &&
		    targ->t_name != NULL)
			errx(1, "cannot specify TargetName for discovery "
			    "sessions for target \"%s\"", targ->t_nickname);
		if (targ->t_name != NULL) {
			if (valid_iscsi_name(targ->t_name) == false)
				errx(1, "invalid target name \"%s\"",
				    targ->t_name);
		}
		if (targ->t_protocol == PROTOCOL_UNSPECIFIED)
			targ->t_protocol = PROTOCOL_ISCSI;
#ifndef ICL_KERNEL_PROXY
		if (targ->t_protocol == PROTOCOL_ISER)
			errx(1, "iSER support requires ICL_KERNEL_PROXY; "
			    "see iscsi(4) for details");
#endif
		if (targ->t_address == NULL)
			errx(1, "missing TargetAddress for target \"%s\"",
			    targ->t_nickname);
		if (targ->t_initiator_name == NULL)
			targ->t_initiator_name = default_initiator_name();
		if (valid_iscsi_name(targ->t_initiator_name) == false)
			errx(1, "invalid initiator name \"%s\"",
			    targ->t_initiator_name);
		if (targ->t_header_digest == DIGEST_UNSPECIFIED)
			targ->t_header_digest = DIGEST_NONE;
		if (targ->t_data_digest == DIGEST_UNSPECIFIED)
			targ->t_data_digest = DIGEST_NONE;
		if (targ->t_auth_method == AUTH_METHOD_UNSPECIFIED) {
			if (targ->t_user != NULL || targ->t_secret != NULL ||
			    targ->t_mutual_user != NULL ||
			    targ->t_mutual_secret != NULL)
				targ->t_auth_method =
				    AUTH_METHOD_CHAP;
			else
				targ->t_auth_method =
				    AUTH_METHOD_NONE;
		}
		if (targ->t_auth_method == AUTH_METHOD_CHAP) {
			if (targ->t_user == NULL) {
				errx(1, "missing chapIName for target \"%s\"",
				    targ->t_nickname);
			}
			if (targ->t_secret == NULL)
				errx(1, "missing chapSecret for target \"%s\"",
				    targ->t_nickname);
			if (targ->t_mutual_user != NULL ||
			    targ->t_mutual_secret != NULL) {
				if (targ->t_mutual_user == NULL)
					errx(1, "missing tgtChapName for "
					    "target \"%s\"", targ->t_nickname);
				if (targ->t_mutual_secret == NULL)
					errx(1, "missing tgtChapSecret for "
					    "target \"%s\"", targ->t_nickname);
			}
		}
	}
}

static void
conf_from_target(struct iscsi_session_conf *conf,
    const struct target *targ)
{
	memset(conf, 0, sizeof(*conf));

	/*
	 * XXX: Check bounds and return error instead of silently truncating.
	 */
	if (targ->t_initiator_name != NULL)
		strlcpy(conf->isc_initiator, targ->t_initiator_name,
		    sizeof(conf->isc_initiator));
	if (targ->t_initiator_address != NULL)
		strlcpy(conf->isc_initiator_addr, targ->t_initiator_address,
		    sizeof(conf->isc_initiator_addr));
	if (targ->t_initiator_alias != NULL)
		strlcpy(conf->isc_initiator_alias, targ->t_initiator_alias,
		    sizeof(conf->isc_initiator_alias));
	if (targ->t_name != NULL)
		strlcpy(conf->isc_target, targ->t_name,
		    sizeof(conf->isc_target));
	if (targ->t_address != NULL)
		strlcpy(conf->isc_target_addr, targ->t_address,
		    sizeof(conf->isc_target_addr));
	if (targ->t_user != NULL)
		strlcpy(conf->isc_user, targ->t_user,
		    sizeof(conf->isc_user));
	if (targ->t_secret != NULL)
		strlcpy(conf->isc_secret, targ->t_secret,
		    sizeof(conf->isc_secret));
	if (targ->t_mutual_user != NULL)
		strlcpy(conf->isc_mutual_user, targ->t_mutual_user,
		    sizeof(conf->isc_mutual_user));
	if (targ->t_mutual_secret != NULL)
		strlcpy(conf->isc_mutual_secret, targ->t_mutual_secret,
		    sizeof(conf->isc_mutual_secret));
	if (targ->t_session_type == SESSION_TYPE_DISCOVERY)
		conf->isc_discovery = 1;
	if (targ->t_protocol == PROTOCOL_ISER)
		conf->isc_iser = 1;
	if (targ->t_header_digest == DIGEST_CRC32C)
		conf->isc_header_digest = ISCSI_DIGEST_CRC32C;
	else
		conf->isc_header_digest = ISCSI_DIGEST_NONE;
	if (targ->t_data_digest == DIGEST_CRC32C)
		conf->isc_data_digest = ISCSI_DIGEST_CRC32C;
	else
		conf->isc_data_digest = ISCSI_DIGEST_NONE;
}

static int
kernel_add(int iscsi_fd, const struct target *targ)
{
	struct iscsi_session_add isa;
	int error;

	memset(&isa, 0, sizeof(isa));
	conf_from_target(&isa.isa_conf, targ);
	error = ioctl(iscsi_fd, ISCSISADD, &isa);
	if (error != 0)
		warn("ISCSISADD");
	return (error);
}

static int
kernel_remove(int iscsi_fd, const struct target *targ)
{
	struct iscsi_session_remove isr;
	int error;

	memset(&isr, 0, sizeof(isr));
	conf_from_target(&isr.isr_conf, targ);
	error = ioctl(iscsi_fd, ISCSISREMOVE, &isr);
	if (error != 0)
		warn("ISCSISREMOVE");
	return (error);
}

/*
 * XXX: Add filtering.
 */
static int
kernel_list(int iscsi_fd, const struct target *targ __unused,
    int verbose)
{
	struct iscsi_session_state *states = NULL;
	const struct iscsi_session_state *state;
	const struct iscsi_session_conf *conf;
	struct iscsi_session_list isl;
	unsigned int i, nentries = 1;
	int error;
	bool show_periphs;

	for (;;) {
		states = realloc(states,
		    nentries * sizeof(struct iscsi_session_state));
		if (states == NULL)
			err(1, "realloc");

		memset(&isl, 0, sizeof(isl));
		isl.isl_nentries = nentries;
		isl.isl_pstates = states;

		error = ioctl(iscsi_fd, ISCSISLIST, &isl);
		if (error != 0 && errno == EMSGSIZE) {
			nentries *= 4;
			continue;
		}
		break;
	}
	if (error != 0) {
		warn("ISCSISLIST");
		return (error);
	}

	if (verbose != 0) {
		for (i = 0; i < isl.isl_nentries; i++) {
			state = &states[i];
			conf = &state->iss_conf;

			printf("Session ID:       %d\n", state->iss_id);
			printf("Initiator name:   %s\n", conf->isc_initiator);
			printf("Initiator portal: %s\n",
			    conf->isc_initiator_addr);
			printf("Initiator alias:  %s\n",
			    conf->isc_initiator_alias);
			printf("Target name:      %s\n", conf->isc_target);
			printf("Target portal:    %s\n",
			    conf->isc_target_addr);
			printf("Target alias:     %s\n",
			    state->iss_target_alias);
			printf("User:             %s\n", conf->isc_user);
			printf("Secret:           %s\n", conf->isc_secret);
			printf("Mutual user:      %s\n",
			    conf->isc_mutual_user);
			printf("Mutual secret:    %s\n",
			    conf->isc_mutual_secret);
			printf("Session type:     %s\n",
			    conf->isc_discovery ? "Discovery" : "Normal");
			printf("Session state:    %s\n",
			    state->iss_connected ?
			    "Connected" : "Disconnected");
			printf("Failure reason:   %s\n", state->iss_reason);
			printf("Header digest:    %s\n",
			    state->iss_header_digest == ISCSI_DIGEST_CRC32C ?
			    "CRC32C" : "None");
			printf("Data digest:      %s\n",
			    state->iss_data_digest == ISCSI_DIGEST_CRC32C ?
			    "CRC32C" : "None");
			printf("DataSegmentLen:   %d\n",
			    state->iss_max_data_segment_length);
			printf("ImmediateData:    %s\n",
			    state->iss_immediate_data ? "Yes" : "No");
			printf("iSER (RDMA):      %s\n",
			    conf->isc_iser ? "Yes" : "No");
			printf("Device nodes:     ");
			print_periphs(state->iss_id);
			printf("\n\n");
		}
	} else {
		printf("%-36s %-16s %s\n",
		    "Target name", "Target portal", "State");
		for (i = 0; i < isl.isl_nentries; i++) {
			state = &states[i];
			conf = &state->iss_conf;
			show_periphs = false;

			printf("%-36s %-16s ",
			    conf->isc_target, conf->isc_target_addr);

			if (state->iss_reason[0] != '\0') {
				printf("%s\n", state->iss_reason);
			} else {
				if (conf->isc_discovery) {
					printf("Discovery\n");
				} else if (state->iss_connected) {
					printf("Connected: ");
					print_periphs(state->iss_id);
					printf("\n");
				} else {
					printf("Disconnected\n");
				}
			}
		}
	}

	return (0);
}

static void
usage(void)
{

	fprintf(stderr, "usage: iscsictl -A -p portal -t target "
	    "[-u user -s secret]\n");
	fprintf(stderr, "       iscsictl -A -d discovery-host "
	    "[-u user -s secret]\n");
	fprintf(stderr, "       iscsictl -A -a [-c path]\n");
	fprintf(stderr, "       iscsictl -A -n nickname [-c path]\n");
	fprintf(stderr, "       iscsictl -R [-p portal] [-t target]\n");
	fprintf(stderr, "       iscsictl -R -a\n");
	fprintf(stderr, "       iscsictl -R -n nickname [-c path]\n");
	fprintf(stderr, "       iscsictl -L [-v]\n");
	exit(1);
}

char *
checked_strdup(const char *s)
{
	char *c;

	c = strdup(s);
	if (c == NULL)
		err(1, "strdup");
	return (c);
}

int
main(int argc, char **argv)
{
	int Aflag = 0, Rflag = 0, Lflag = 0, aflag = 0, vflag = 0;
	const char *conf_path = DEFAULT_CONFIG_PATH;
	char *nickname = NULL, *discovery_host = NULL, *host = NULL,
	     *target = NULL, *user = NULL, *secret = NULL;
	int ch, error, iscsi_fd, retval, saved_errno;
	int failed = 0;
	struct conf *conf;
	struct target *targ;

	while ((ch = getopt(argc, argv, "ARLac:d:n:p:t:u:s:v")) != -1) {
		switch (ch) {
		case 'A':
			Aflag = 1;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'L':
			Lflag = 1;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'c':
			conf_path = optarg;
			break;
		case 'd':
			discovery_host = optarg;
			break;
		case 'n':
			nickname = optarg;
			break;
		case 'p':
			host = optarg;
			break;
		case 't':
			target = optarg;
			break;
		case 'u':
			user = optarg;
			break;
		case 's':
			secret = optarg;
			break;
		case 'v':
			vflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	if (argc != 0)
		usage();

	if (Aflag + Rflag + Lflag == 0)
		Lflag = 1;
	if (Aflag + Rflag + Lflag > 1)
		errx(1, "at most one of -A, -R, or -L may be specified");

	/*
	 * Note that we ignore unneccessary/inapplicable "-c" flag; so that
	 * people can do something like "alias ISCSICTL="iscsictl -c path"
	 * in shell scripts.
	 */
	if (Aflag != 0) {
		if (aflag != 0) {
			if (host != NULL)
				errx(1, "-a and -p and mutually exclusive");
			if (target != NULL)
				errx(1, "-a and -t and mutually exclusive");
			if (user != NULL)
				errx(1, "-a and -u and mutually exclusive");
			if (secret != NULL)
				errx(1, "-a and -s and mutually exclusive");
			if (nickname != NULL)
				errx(1, "-a and -n and mutually exclusive");
			if (discovery_host != NULL)
				errx(1, "-a and -d and mutually exclusive");
		} else if (nickname != NULL) {
			if (host != NULL)
				errx(1, "-n and -p and mutually exclusive");
			if (target != NULL)
				errx(1, "-n and -t and mutually exclusive");
			if (user != NULL)
				errx(1, "-n and -u and mutually exclusive");
			if (secret != NULL)
				errx(1, "-n and -s and mutually exclusive");
			if (discovery_host != NULL)
				errx(1, "-n and -d and mutually exclusive");
		} else if (discovery_host != NULL) {
			if (host != NULL)
				errx(1, "-d and -p and mutually exclusive");
			if (target != NULL)
				errx(1, "-d and -t and mutually exclusive");
		} else {
			if (target == NULL && host == NULL)
				errx(1, "must specify -a, -n or -t/-p");

			if (target != NULL && host == NULL)
				errx(1, "-t must always be used with -p");
			if (host != NULL && target == NULL)
				errx(1, "-p must always be used with -t");
		}

		if (user != NULL && secret == NULL)
			errx(1, "-u must always be used with -s");
		if (secret != NULL && user == NULL)
			errx(1, "-s must always be used with -u");

		if (vflag != 0)
			errx(1, "-v cannot be used with -A");

	} else if (Rflag != 0) {
		if (user != NULL)
			errx(1, "-R and -u are mutually exclusive");
		if (secret != NULL)
			errx(1, "-R and -s are mutually exclusive");
		if (discovery_host != NULL)
			errx(1, "-R and -d are mutually exclusive");

		if (aflag != 0) {
			if (host != NULL)
				errx(1, "-a and -p and mutually exclusive");
			if (target != NULL)
				errx(1, "-a and -t and mutually exclusive");
			if (nickname != NULL)
				errx(1, "-a and -n and mutually exclusive");
		} else if (nickname != NULL) {
			if (host != NULL)
				errx(1, "-n and -p and mutually exclusive");
			if (target != NULL)
				errx(1, "-n and -t and mutually exclusive");
		} else if (host != NULL) {
			if (target != NULL)
				errx(1, "-p and -t and mutually exclusive");
		} else if (target != NULL) {
			if (host != NULL)
				errx(1, "-t and -p and mutually exclusive");
		} else
			errx(1, "must specify either -a, -n, -t, or -p");

		if (vflag != 0)
			errx(1, "-v cannot be used with -R");

	} else {
		assert(Lflag != 0);

		if (host != NULL)
			errx(1, "-L and -p and mutually exclusive");
		if (target != NULL)
			errx(1, "-L and -t and mutually exclusive");
		if (user != NULL)
			errx(1, "-L and -u and mutually exclusive");
		if (secret != NULL)
			errx(1, "-L and -s and mutually exclusive");
		if (nickname != NULL)
			errx(1, "-L and -n and mutually exclusive");
		if (discovery_host != NULL)
			errx(1, "-L and -d and mutually exclusive");
	}

	iscsi_fd = open(ISCSI_PATH, O_RDWR);
	if (iscsi_fd < 0 && errno == ENOENT) {
		saved_errno = errno;
		retval = kldload("iscsi");
		if (retval != -1)
			iscsi_fd = open(ISCSI_PATH, O_RDWR);
		else
			errno = saved_errno;
	}
	if (iscsi_fd < 0)
		err(1, "failed to open %s", ISCSI_PATH);

	if (Aflag != 0 && aflag != 0) {
		conf = conf_new_from_file(conf_path);

		TAILQ_FOREACH(targ, &conf->conf_targets, t_next)
			failed += kernel_add(iscsi_fd, targ);
	} else if (nickname != NULL) {
		conf = conf_new_from_file(conf_path);
		targ = target_find(conf, nickname);
		if (targ == NULL)
			errx(1, "target %s not found in the configuration file",
			    nickname);

		if (Aflag != 0)
			failed += kernel_add(iscsi_fd, targ);
		else if (Rflag != 0)
			failed += kernel_remove(iscsi_fd, targ);
		else
			failed += kernel_list(iscsi_fd, targ, vflag);
	} else {
		if (Aflag != 0 && target != NULL) {
			if (valid_iscsi_name(target) == false)
				errx(1, "invalid target name \"%s\"", target);
		}
		conf = conf_new();
		targ = target_new(conf);
		targ->t_initiator_name = default_initiator_name();
		targ->t_header_digest = DIGEST_NONE;
		targ->t_data_digest = DIGEST_NONE;
		targ->t_name = target;
		if (discovery_host != NULL) {
			targ->t_session_type = SESSION_TYPE_DISCOVERY;
			targ->t_address = discovery_host;
		} else {
			targ->t_session_type = SESSION_TYPE_NORMAL;
			targ->t_address = host;
		}
		targ->t_user = user;
		targ->t_secret = secret;

		if (Aflag != 0)
			failed += kernel_add(iscsi_fd, targ);
		else if (Rflag != 0)
			failed += kernel_remove(iscsi_fd, targ);
		else
			failed += kernel_list(iscsi_fd, targ, vflag);
	}

	error = close(iscsi_fd);
	if (error != 0)
		err(1, "close");

	if (failed > 0)
		return (1);
	return (0);
}
