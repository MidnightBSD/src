/*-
 * Copyright (c) 2012-2013 Baptiste Daroussin <bapt@FreeBSD.org>
 * Copyright (c) 2013 Bryan Drewery <bdrewery@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/usr.sbin/pkg/pkg.c 258773 2013-11-30 17:33:49Z gjb $");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/sbuf.h>
#include <sys/wait.h>

#define _WITH_GETLINE
#include <archive.h>
#include <archive_entry.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fetch.h>
#include <paths.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <yaml.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "dns_utils.h"
#include "config.h"

struct sig_cert {
	char *name;
	unsigned char *sig;
	int siglen;
	unsigned char *cert;
	int certlen;
	bool trusted;
};

typedef enum {
       HASH_UNKNOWN,
       HASH_SHA256,
} hash_t;

struct fingerprint {
       hash_t type;
       char *name;
       char hash[BUFSIZ];
       STAILQ_ENTRY(fingerprint) next;
};

STAILQ_HEAD(fingerprint_list, fingerprint);

static int
extract_pkg_static(int fd, char *p, int sz)
{
	struct archive *a;
	struct archive_entry *ae;
	char *end;
	int ret, r;

	ret = -1;
	a = archive_read_new();
	if (a == NULL) {
		warn("archive_read_new");
		return (ret);
	}
	archive_read_support_filter_all(a);
	archive_read_support_format_tar(a);

	if (lseek(fd, 0, 0) == -1) {
		warn("lseek");
		goto cleanup;
	}

	if (archive_read_open_fd(a, fd, 4096) != ARCHIVE_OK) {
		warnx("archive_read_open_fd: %s", archive_error_string(a));
		goto cleanup;
	}

	ae = NULL;
	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK) {
		end = strrchr(archive_entry_pathname(ae), '/');
		if (end == NULL)
			continue;

		if (strcmp(end, "/pkg-static") == 0) {
			r = archive_read_extract(a, ae,
			    ARCHIVE_EXTRACT_OWNER | ARCHIVE_EXTRACT_PERM |
			    ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_ACL |
			    ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_XATTR);
			strlcpy(p, archive_entry_pathname(ae), sz);
			break;
		}
	}

	if (r == ARCHIVE_OK)
		ret = 0;
	else
		warnx("fail to extract pkg-static");

cleanup:
	archive_read_free(a);
	return (ret);

}

static int
install_pkg_static(const char *path, const char *pkgpath, bool force)
{
	int pstat;
	pid_t pid;

	switch ((pid = fork())) {
	case -1:
		return (-1);
	case 0:
		if (force)
			execl(path, "pkg-static", "add", "-f", pkgpath,
			    (char *)NULL);
		else
			execl(path, "pkg-static", "add", pkgpath,
			    (char *)NULL);
		_exit(1);
	default:
		break;
	}

	while (waitpid(pid, &pstat, 0) == -1)
		if (errno != EINTR)
			return (-1);

	if (WEXITSTATUS(pstat))
		return (WEXITSTATUS(pstat));
	else if (WIFSIGNALED(pstat))
		return (128 & (WTERMSIG(pstat)));
	return (pstat);
}

static int
fetch_to_fd(const char *url, char *path)
{
	struct url *u;
	struct dns_srvinfo *mirrors, *current;
	struct url_stat st;
	FILE *remote;
	/* To store _https._tcp. + hostname + \0 */
	int fd;
	int retry, max_retry;
	off_t done, r;
	time_t now, last;
	char buf[10240];
	char zone[MAXHOSTNAMELEN + 13];
	static const char *mirror_type = NULL;

	done = 0;
	last = 0;
	max_retry = 3;
	current = mirrors = NULL;
	remote = NULL;

	if (mirror_type == NULL && config_string(MIRROR_TYPE, &mirror_type)
	    != 0) {
		warnx("No MIRROR_TYPE defined");
		return (-1);
	}

	if ((fd = mkstemp(path)) == -1) {
		warn("mkstemp()");
		return (-1);
	}

	retry = max_retry;

	u = fetchParseURL(url);
	while (remote == NULL) {
		if (retry == max_retry) {
			if (strcmp(u->scheme, "file") != 0 &&
			    strcasecmp(mirror_type, "srv") == 0) {
				snprintf(zone, sizeof(zone),
				    "_%s._tcp.%s", u->scheme, u->host);
				mirrors = dns_getsrvinfo(zone);
				current = mirrors;
			}
		}

		if (mirrors != NULL) {
			strlcpy(u->host, current->host, sizeof(u->host));
			u->port = current->port;
		}

		remote = fetchXGet(u, &st, "");
		if (remote == NULL) {
			--retry;
			if (retry <= 0)
				goto fetchfail;
			if (mirrors == NULL) {
				sleep(1);
			} else {
				current = current->next;
				if (current == NULL)
					current = mirrors;
			}
		}
	}

	if (remote == NULL)
		goto fetchfail;

	while (done < st.size) {
		if ((r = fread(buf, 1, sizeof(buf), remote)) < 1)
			break;

		if (write(fd, buf, r) != r) {
			warn("write()");
			goto fetchfail;
		}

		done += r;
		now = time(NULL);
		if (now > last || done == st.size)
			last = now;
	}

	if (ferror(remote))
		goto fetchfail;

	goto cleanup;

fetchfail:
	if (fd != -1) {
		close(fd);
		fd = -1;
		unlink(path);
	}

cleanup:
	if (remote != NULL)
		fclose(remote);

	return fd;
}

static struct fingerprint *
parse_fingerprint(yaml_document_t *doc, yaml_node_t *node)
{
	yaml_node_pair_t *pair;
	yaml_char_t *function, *fp;
	struct fingerprint *f;
	hash_t fct = HASH_UNKNOWN;

	function = fp = NULL;

	pair = node->data.mapping.pairs.start;
	while (pair < node->data.mapping.pairs.top) {
		yaml_node_t *key = yaml_document_get_node(doc, pair->key);
		yaml_node_t *val = yaml_document_get_node(doc, pair->value);

		if (key->data.scalar.length <= 0) {
			++pair;
			continue;
		}

		if (val->type != YAML_SCALAR_NODE) {
			++pair;
			continue;
		}

		if (strcasecmp(key->data.scalar.value, "function") == 0)
			function = val->data.scalar.value;
		else if (strcasecmp(key->data.scalar.value, "fingerprint")
		    == 0)
			fp = val->data.scalar.value;

		++pair;
		continue;
	}

	if (fp == NULL || function == NULL)
		return (NULL);

	if (strcasecmp(function, "sha256") == 0)
		fct = HASH_SHA256;

	if (fct == HASH_UNKNOWN) {
		fprintf(stderr, "Unsupported hashing function: %s\n", function);
		return (NULL);
	}

	f = calloc(1, sizeof(struct fingerprint));
	f->type = fct;
	strlcpy(f->hash, fp, sizeof(f->hash));

	return (f);
}

static void
free_fingerprint_list(struct fingerprint_list* list)
{
	struct fingerprint *fingerprint, *tmp;

	STAILQ_FOREACH_SAFE(fingerprint, list, next, tmp) {
		if (fingerprint->name)
			free(fingerprint->name);
		free(fingerprint);
	}
	free(list);
}

static struct fingerprint *
load_fingerprint(const char *dir, const char *filename)
{
	yaml_parser_t parser;
	yaml_document_t doc;
	yaml_node_t *node;
	FILE *fp;
	struct fingerprint *f;
	char path[MAXPATHLEN];

	f = NULL;

	snprintf(path, MAXPATHLEN, "%s/%s", dir, filename);

	if ((fp = fopen(path, "r")) == NULL)
		return (NULL);

	yaml_parser_initialize(&parser);
	yaml_parser_set_input_file(&parser, fp);
	yaml_parser_load(&parser, &doc);

	node = yaml_document_get_root_node(&doc);
	if (node == NULL || node->type != YAML_MAPPING_NODE)
		goto out;

	f = parse_fingerprint(&doc, node);
	f->name = strdup(filename);

out:
	yaml_document_delete(&doc);
	yaml_parser_delete(&parser);
	fclose(fp);

	return (f);
}

static struct fingerprint_list *
load_fingerprints(const char *path, int *count)
{
	DIR *d;
	struct dirent *ent;
	struct fingerprint *finger;
	struct fingerprint_list *fingerprints;

	*count = 0;

	fingerprints = calloc(1, sizeof(struct fingerprint_list));
	if (fingerprints == NULL)
		return (NULL);
	STAILQ_INIT(fingerprints);

	if ((d = opendir(path)) == NULL)
		return (NULL);

	while ((ent = readdir(d))) {
		if (strcmp(ent->d_name, ".") == 0 ||
		    strcmp(ent->d_name, "..") == 0)
			continue;
		finger = load_fingerprint(path, ent->d_name);
		if (finger != NULL) {
			STAILQ_INSERT_TAIL(fingerprints, finger, next);
			++(*count);
		}
	}

	closedir(d);

	return (fingerprints);
}

static void
sha256_hash(unsigned char hash[SHA256_DIGEST_LENGTH],
    char out[SHA256_DIGEST_LENGTH * 2 + 1])
{
	int i;

	for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
		sprintf(out + (i * 2), "%02x", hash[i]);

	out[SHA256_DIGEST_LENGTH * 2] = '\0';
}

static void
sha256_buf(char *buf, size_t len, char out[SHA256_DIGEST_LENGTH * 2 + 1])
{
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256_CTX sha256;

	out[0] = '\0';

	SHA256_Init(&sha256);
	SHA256_Update(&sha256, buf, len);
	SHA256_Final(hash, &sha256);
	sha256_hash(hash, out);
}

static int
sha256_fd(int fd, char out[SHA256_DIGEST_LENGTH * 2 + 1])
{
	int my_fd;
	FILE *fp;
	char buffer[BUFSIZ];
	unsigned char hash[SHA256_DIGEST_LENGTH];
	size_t r;
	int ret;
	SHA256_CTX sha256;

	my_fd = -1;
	fp = NULL;
	r = 0;
	ret = 1;

	out[0] = '\0';

	/* Duplicate the fd so that fclose(3) does not close it. */
	if ((my_fd = dup(fd)) == -1) {
		warnx("dup");
		goto cleanup;
	}

	if ((fp = fdopen(my_fd, "rb")) == NULL) {
		warnx("fdopen");
		goto cleanup;
	}

	SHA256_Init(&sha256);

	while ((r = fread(buffer, 1, BUFSIZ, fp)) > 0)
		SHA256_Update(&sha256, buffer, r);

	if (ferror(fp) != 0) {
		warnx("fread");
		goto cleanup;
	}

	SHA256_Final(hash, &sha256);
	sha256_hash(hash, out);
	ret = 0;

cleanup:
	if (fp != NULL)
		fclose(fp);
	else if (my_fd != -1)
		close(my_fd);
	(void)lseek(fd, 0, SEEK_SET);

	return (ret);
}

static EVP_PKEY *
load_public_key_buf(const unsigned char *cert, int certlen)
{
	EVP_PKEY *pkey;
	BIO *bp;
	char errbuf[1024];

	bp = BIO_new_mem_buf(__DECONST(void *, cert), certlen);

	if ((pkey = PEM_read_bio_PUBKEY(bp, NULL, NULL, NULL)) == NULL)
		warnx("%s", ERR_error_string(ERR_get_error(), errbuf));

	BIO_free(bp);

	return (pkey);
}

static bool
rsa_verify_cert(int fd, const unsigned char *key, int keylen,
    unsigned char *sig, int siglen)
{
	EVP_MD_CTX *mdctx;
	EVP_PKEY *pkey;
	char sha256[(SHA256_DIGEST_LENGTH * 2) + 2];
	char errbuf[1024];
	bool ret;

	pkey = NULL;
	mdctx = NULL;
	ret = false;

	/* Compute SHA256 of the package. */
	if (lseek(fd, 0, 0) == -1) {
		warn("lseek");
		goto cleanup;
	}
	if ((sha256_fd(fd, sha256)) == -1) {
		warnx("Error creating SHA256 hash for package");
		goto cleanup;
	}

	if ((pkey = load_public_key_buf(key, keylen)) == NULL) {
		warnx("Error reading public key");
		goto cleanup;
	}

	/* Verify signature of the SHA256(pkg) is valid. */
	if ((mdctx = EVP_MD_CTX_create()) == NULL) {
		warnx("%s", ERR_error_string(ERR_get_error(), errbuf));
		goto error;
	}

	if (EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, pkey) != 1) {
		warnx("%s", ERR_error_string(ERR_get_error(), errbuf));
		goto error;
	}
	if (EVP_DigestVerifyUpdate(mdctx, sha256, strlen(sha256)) != 1) {
		warnx("%s", ERR_error_string(ERR_get_error(), errbuf));
		goto error;
	}

	if (EVP_DigestVerifyFinal(mdctx, sig, siglen) != 1) {
		warnx("%s", ERR_error_string(ERR_get_error(), errbuf));
		goto error;
	}

	ret = true;
	printf("done\n");
	goto cleanup;

error:
	printf("failed\n");

cleanup:
	if (pkey)
		EVP_PKEY_free(pkey);
	if (mdctx)
		EVP_MD_CTX_destroy(mdctx);
	ERR_free_strings();

	return (ret);
}

static struct sig_cert *
parse_cert(int fd) {
	int my_fd;
	struct sig_cert *sc;
	FILE *fp;
	struct sbuf *buf, *sig, *cert;
	char *line;
	size_t linecap;
	ssize_t linelen;

	buf = NULL;
	my_fd = -1;
	sc = NULL;
	line = NULL;
	linecap = 0;

	if (lseek(fd, 0, 0) == -1) {
		warn("lseek");
		return (NULL);
	}

	/* Duplicate the fd so that fclose(3) does not close it. */
	if ((my_fd = dup(fd)) == -1) {
		warnx("dup");
		return (NULL);
	}

	if ((fp = fdopen(my_fd, "rb")) == NULL) {
		warn("fdopen");
		close(my_fd);
		return (NULL);
	}

	sig = sbuf_new_auto();
	cert = sbuf_new_auto();

	while ((linelen = getline(&line, &linecap, fp)) > 0) {
		if (strcmp(line, "SIGNATURE\n") == 0) {
			buf = sig;
			continue;
		} else if (strcmp(line, "CERT\n") == 0) {
			buf = cert;
			continue;
		} else if (strcmp(line, "END\n") == 0) {
			break;
		}
		if (buf != NULL)
			sbuf_bcat(buf, line, linelen);
	}

	fclose(fp);

	/* Trim out unrelated trailing newline */
	sbuf_setpos(sig, sbuf_len(sig) - 1);

	sbuf_finish(sig);
	sbuf_finish(cert);

	sc = calloc(1, sizeof(struct sig_cert));
	sc->siglen = sbuf_len(sig);
	sc->sig = calloc(1, sc->siglen);
	memcpy(sc->sig, sbuf_data(sig), sc->siglen);

	sc->certlen = sbuf_len(cert);
	sc->cert = strdup(sbuf_data(cert));

	sbuf_delete(sig);
	sbuf_delete(cert);

	return (sc);
}

static bool
verify_signature(int fd_pkg, int fd_sig)
{
	struct fingerprint_list *trusted, *revoked;
	struct fingerprint *fingerprint;
	struct sig_cert *sc;
	bool ret;
	int trusted_count, revoked_count;
	const char *fingerprints;
	char path[MAXPATHLEN];
	char hash[SHA256_DIGEST_LENGTH * 2 + 1];

	sc = NULL;
	trusted = revoked = NULL;
	ret = false;

	/* Read and parse fingerprints. */
	if (config_string(FINGERPRINTS, &fingerprints) != 0) {
		warnx("No CONFIG_FINGERPRINTS defined");
		goto cleanup;
	}

	snprintf(path, MAXPATHLEN, "%s/trusted", fingerprints);
	if ((trusted = load_fingerprints(path, &trusted_count)) == NULL) {
		warnx("Error loading trusted certificates");
		goto cleanup;
	}

	if (trusted_count == 0 || trusted == NULL) {
		fprintf(stderr, "No trusted certificates found.\n");
		goto cleanup;
	}

	snprintf(path, MAXPATHLEN, "%s/revoked", fingerprints);
	if ((revoked = load_fingerprints(path, &revoked_count)) == NULL) {
		warnx("Error loading revoked certificates");
		goto cleanup;
	}

	/* Read certificate and signature in. */
	if ((sc = parse_cert(fd_sig)) == NULL) {
		warnx("Error parsing certificate");
		goto cleanup;
	}
	/* Explicitly mark as non-trusted until proven otherwise. */
	sc->trusted = false;

	/* Parse signature and pubkey out of the certificate */
	sha256_buf(sc->cert, sc->certlen, hash);

	/* Check if this hash is revoked */
	if (revoked != NULL) {
		STAILQ_FOREACH(fingerprint, revoked, next) {
			if (strcasecmp(fingerprint->hash, hash) == 0) {
				fprintf(stderr, "The package was signed with "
				    "revoked certificate %s\n",
				    fingerprint->name);
				goto cleanup;
			}
		}
	}

	STAILQ_FOREACH(fingerprint, trusted, next) {
		if (strcasecmp(fingerprint->hash, hash) == 0) {
			sc->trusted = true;
			sc->name = strdup(fingerprint->name);
			break;
		}
	}

	if (sc->trusted == false) {
		fprintf(stderr, "No trusted fingerprint found matching "
		    "package's certificate\n");
		goto cleanup;
	}

	/* Verify the signature. */
	printf("Verifying signature with trusted certificate %s... ", sc->name);
	if (rsa_verify_cert(fd_pkg, sc->cert, sc->certlen, sc->sig,
	    sc->siglen) == false) {
		fprintf(stderr, "Signature is not valid\n");
		goto cleanup;
	}

	ret = true;

cleanup:
	if (trusted)
		free_fingerprint_list(trusted);
	if (revoked)
		free_fingerprint_list(revoked);
	if (sc) {
		if (sc->cert)
			free(sc->cert);
		if (sc->sig)
			free(sc->sig);
		if (sc->name)
			free(sc->name);
		free(sc);
	}

	return (ret);
}

static int
bootstrap_pkg(bool force)
{
	int fd_pkg, fd_sig;
	int ret;
	char url[MAXPATHLEN];
	char tmppkg[MAXPATHLEN];
	char tmpsig[MAXPATHLEN];
	const char *packagesite;
	const char *signature_type;
	char pkgstatic[MAXPATHLEN];

	fd_sig = -1;
	ret = -1;

	if (config_string(PACKAGESITE, &packagesite) != 0) {
		warnx("No PACKAGESITE defined");
		return (-1);
	}

	if (config_string(SIGNATURE_TYPE, &signature_type) != 0) {
		warnx("Error looking up SIGNATURE_TYPE");
		return (-1);
	}

	printf("Bootstrapping pkg from %s, please wait...\n", packagesite);

	/* Support pkg+http:// for PACKAGESITE which is the new format
	   in 1.2 to avoid confusion on why http://pkg.FreeBSD.org has
	   no A record. */
	if (strncmp(URL_SCHEME_PREFIX, packagesite,
	    strlen(URL_SCHEME_PREFIX)) == 0)
		packagesite += strlen(URL_SCHEME_PREFIX);
	snprintf(url, MAXPATHLEN, "%s/Latest/pkg.txz", packagesite);

	snprintf(tmppkg, MAXPATHLEN, "%s/pkg.txz.XXXXXX",
	    getenv("TMPDIR") ? getenv("TMPDIR") : _PATH_TMP);

	if ((fd_pkg = fetch_to_fd(url, tmppkg)) == -1)
		goto fetchfail;

	if (signature_type != NULL &&
	    strcasecmp(signature_type, "FINGERPRINTS") == 0) {
		snprintf(tmpsig, MAXPATHLEN, "%s/pkg.txz.sig.XXXXXX",
		    getenv("TMPDIR") ? getenv("TMPDIR") : _PATH_TMP);
		snprintf(url, MAXPATHLEN, "%s/Latest/pkg.txz.sig",
		    packagesite);

		if ((fd_sig = fetch_to_fd(url, tmpsig)) == -1) {
			fprintf(stderr, "Signature for pkg not available.\n");
			goto fetchfail;
		}

		if (verify_signature(fd_pkg, fd_sig) == false)
			goto cleanup;
	}

	if ((ret = extract_pkg_static(fd_pkg, pkgstatic, MAXPATHLEN)) == 0)
		ret = install_pkg_static(pkgstatic, tmppkg, force);

	goto cleanup;

fetchfail:
	warnx("Error fetching %s: %s", url, fetchLastErrString);
	fprintf(stderr, "A pre-built version of pkg could not be found for "
	    "your system.\n");
	fprintf(stderr, "Consider changing PACKAGESITE or installing it from "
	    "ports: 'ports-mgmt/pkg'.\n");

cleanup:
	if (fd_sig != -1) {
		close(fd_sig);
		unlink(tmpsig);
	}
	close(fd_pkg);
	unlink(tmppkg);

	return (ret);
}

static const char confirmation_message[] =
"The package management tool is not yet installed on your system.\n"
"Do you want to fetch and install it now? [y/N]: ";

static int
pkg_query_yes_no(void)
{
	int ret, c;

	c = getchar();

	if (c == 'y' || c == 'Y')
		ret = 1;
	else
		ret = 0;

	while (c != '\n' && c != EOF)
		c = getchar();

	return (ret);
}

static int
bootstrap_pkg_local(const char *pkgpath, bool force)
{
	char path[MAXPATHLEN];
	char pkgstatic[MAXPATHLEN];
	const char *signature_type;
	int fd_pkg, fd_sig, ret;

	fd_sig = -1;
	ret = -1;

	fd_pkg = open(pkgpath, O_RDONLY);
	if (fd_pkg == -1)
		err(EXIT_FAILURE, "Unable to open %s", pkgpath);

	if (config_string(SIGNATURE_TYPE, &signature_type) != 0) {
		warnx("Error looking up SIGNATURE_TYPE");
		return (-1);
	}
	if (signature_type != NULL &&
	    strcasecmp(signature_type, "FINGERPRINTS") == 0) {
		snprintf(path, sizeof(path), "%s.sig", pkgpath);

		if ((fd_sig = open(path, O_RDONLY)) == -1) {
			fprintf(stderr, "Signature for pkg not available.\n");
			goto cleanup;
		}

		if (verify_signature(fd_pkg, fd_sig) == false)
			goto cleanup;
	}

	if ((ret = extract_pkg_static(fd_pkg, pkgstatic, MAXPATHLEN)) == 0)
		ret = install_pkg_static(pkgstatic, pkgpath, force);

cleanup:
	close(fd_pkg);
	if (fd_sig != -1)
		close(fd_sig);

	return (ret);
}

int
main(__unused int argc, char *argv[])
{
	char pkgpath[MAXPATHLEN];
	const char *pkgarg;
	bool bootstrap_only, force, yes;

	bootstrap_only = false;
	force = false;
	pkgarg = NULL;
	yes = false;

	snprintf(pkgpath, MAXPATHLEN, "%s/sbin/pkg",
	    getenv("LOCALBASE") ? getenv("LOCALBASE") : _LOCALBASE);

	if (argc > 1 && strcmp(argv[1], "bootstrap") == 0) {
		bootstrap_only = true;
		if (argc == 3 && strcmp(argv[2], "-f") == 0)
			force = true;
	}

	if ((bootstrap_only && force) || access(pkgpath, X_OK) == -1) {
		/* 
		 * To allow 'pkg -N' to be used as a reliable test for whether
		 * a system is configured to use pkg, don't bootstrap pkg
		 * when that argument is given as argv[1].
		 */
		if (argv[1] != NULL && strcmp(argv[1], "-N") == 0)
			errx(EXIT_FAILURE, "pkg is not installed");

		config_init();

		if (argc > 1 && strcmp(argv[1], "add") == 0) {
			if (argc > 2 && strcmp(argv[2], "-f") == 0) {
				force = true;
				pkgarg = argv[3];
			} else
				pkgarg = argv[2];
			if (pkgarg == NULL) {
				fprintf(stderr, "Path to pkg.txz required\n");
				exit(EXIT_FAILURE);
			}
			if (access(pkgarg, R_OK) == -1) {
				fprintf(stderr, "No such file: %s\n", pkgarg);
				exit(EXIT_FAILURE);
			}
			if (bootstrap_pkg_local(pkgarg, force) != 0)
				exit(EXIT_FAILURE);
			exit(EXIT_SUCCESS);
		}
		/*
		 * Do not ask for confirmation if either of stdin or stdout is
		 * not tty. Check the environment to see if user has answer
		 * tucked in there already.
		 */
		config_bool(ASSUME_ALWAYS_YES, &yes);
		if (!yes) {
			printf("%s", confirmation_message);
			if (!isatty(fileno(stdin)))
				exit(EXIT_FAILURE);

			if (pkg_query_yes_no() == 0)
				exit(EXIT_FAILURE);
		}
		if (bootstrap_pkg(force) != 0)
			exit(EXIT_FAILURE);
		config_finish();

		if (bootstrap_only)
			exit(EXIT_SUCCESS);
	} else if (bootstrap_only) {
		printf("pkg already bootstrapped at %s\n", pkgpath);
		exit(EXIT_SUCCESS);
	}

	execv(pkgpath, argv);

	/* NOT REACHED */
	return (EXIT_FAILURE);
}
