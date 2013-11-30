/* $OpenBSD: auth-rsa.c,v 1.72 2006/11/06 21:25:27 markus Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * RSA-based authentication.  This code determines whether to admit a login
 * based on RSA authentication.  This file also contains functions to check
 * validity of the host key.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <openssl/rsa.h>
#include <openssl/md5.h>

#include <pwd.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "xmalloc.h"
#include "rsa.h"
#include "packet.h"
#include "ssh1.h"
#include "uidswap.h"
#include "match.h"
#include "buffer.h"
#include "auth-options.h"
#include "pathnames.h"
#include "log.h"
#include "servconf.h"
#include "key.h"
#include "hostfile.h"
#include "auth.h"
#ifdef GSSAPI
#include "ssh-gss.h"
#endif
#include "monitor_wrap.h"
#include "ssh.h"
#include "misc.h"

/* import */
extern ServerOptions options;

/*
 * Session identifier that is used to bind key exchange and authentication
 * responses to a particular session.
 */
extern u_char session_id[16];

/*
 * The .ssh/authorized_keys file contains public keys, one per line, in the
 * following format:
 *   options bits e n comment
 * where bits, e and n are decimal numbers,
 * and comment is any string of characters up to newline.  The maximum
 * length of a line is SSH_MAX_PUBKEY_BYTES characters.  See sshd(8) for a
 * description of the options.
 */

BIGNUM *
auth_rsa_generate_challenge(Key *key)
{
	BIGNUM *challenge;
	BN_CTX *ctx;

	if ((challenge = BN_new()) == NULL)
		fatal("auth_rsa_generate_challenge: BN_new() failed");
	/* Generate a random challenge. */
	if (BN_rand(challenge, 256, 0, 0) == 0)
		fatal("auth_rsa_generate_challenge: BN_rand failed");
	if ((ctx = BN_CTX_new()) == NULL)
		fatal("auth_rsa_generate_challenge: BN_CTX_new failed");
	if (BN_mod(challenge, challenge, key->rsa->n, ctx) == 0)
		fatal("auth_rsa_generate_challenge: BN_mod failed");
	BN_CTX_free(ctx);

	return challenge;
}

int
auth_rsa_verify_response(Key *key, BIGNUM *challenge, u_char response[16])
{
	u_char buf[32], mdbuf[16];
	MD5_CTX md;
	int len;

	/* don't allow short keys */
	if (BN_num_bits(key->rsa->n) < SSH_RSA_MINIMUM_MODULUS_SIZE) {
		error("auth_rsa_verify_response: RSA modulus too small: %d < minimum %d bits",
		    BN_num_bits(key->rsa->n), SSH_RSA_MINIMUM_MODULUS_SIZE);
		return (0);
	}

	/* The response is MD5 of decrypted challenge plus session id. */
	len = BN_num_bytes(challenge);
	if (len <= 0 || len > 32)
		fatal("auth_rsa_verify_response: bad challenge length %d", len);
	memset(buf, 0, 32);
	BN_bn2bin(challenge, buf + 32 - len);
	MD5_Init(&md);
	MD5_Update(&md, buf, 32);
	MD5_Update(&md, session_id, 16);
	MD5_Final(mdbuf, &md);

	/* Verify that the response is the original challenge. */
	if (memcmp(response, mdbuf, 16) != 0) {
		/* Wrong answer. */
		return (0);
	}
	/* Correct answer. */
	return (1);
}

/*
 * Performs the RSA authentication challenge-response dialog with the client,
 * and returns true (non-zero) if the client gave the correct answer to
 * our challenge; returns zero if the client gives a wrong answer.
 */

int
auth_rsa_challenge_dialog(Key *key)
{
	BIGNUM *challenge, *encrypted_challenge;
	u_char response[16];
	int i, success;

	if ((encrypted_challenge = BN_new()) == NULL)
		fatal("auth_rsa_challenge_dialog: BN_new() failed");

	challenge = PRIVSEP(auth_rsa_generate_challenge(key));

	/* Encrypt the challenge with the public key. */
	rsa_public_encrypt(encrypted_challenge, challenge, key->rsa);

	/* Send the encrypted challenge to the client. */
	packet_start(SSH_SMSG_AUTH_RSA_CHALLENGE);
	packet_put_bignum(encrypted_challenge);
	packet_send();
	BN_clear_free(encrypted_challenge);
	packet_write_wait();

	/* Wait for a response. */
	packet_read_expect(SSH_CMSG_AUTH_RSA_RESPONSE);
	for (i = 0; i < 16; i++)
		response[i] = (u_char)packet_get_char();
	packet_check_eom();

	success = PRIVSEP(auth_rsa_verify_response(key, challenge, response));
	BN_clear_free(challenge);
	return (success);
}

/*
 * check if there's user key matching client_n,
 * return key if login is allowed, NULL otherwise
 */

int
auth_rsa_key_allowed(struct passwd *pw, BIGNUM *client_n, Key **rkey)
{
	char line[SSH_MAX_PUBKEY_BYTES], *file;
	int allowed = 0;
	u_int bits;
	FILE *f;
	u_long linenum = 0;
	struct stat st;
	Key *key;

	/* Temporarily use the user's uid. */
	temporarily_use_uid(pw);

	/* The authorized keys. */
	file = authorized_keys_file(pw);
	debug("trying public RSA key file %s", file);

	/* Fail quietly if file does not exist */
	if (stat(file, &st) < 0) {
		/* Restore the privileged uid. */
		restore_uid();
		xfree(file);
		return (0);
	}
	/* Open the file containing the authorized keys. */
	f = fopen(file, "r");
	if (!f) {
		/* Restore the privileged uid. */
		restore_uid();
		xfree(file);
		return (0);
	}
	if (options.strict_modes &&
	    secure_filename(f, file, pw, line, sizeof(line)) != 0) {
		xfree(file);
		fclose(f);
		logit("Authentication refused: %s", line);
		restore_uid();
		return (0);
	}

	/* Flag indicating whether the key is allowed. */
	allowed = 0;

	key = key_new(KEY_RSA1);

	/*
	 * Go though the accepted keys, looking for the current key.  If
	 * found, perform a challenge-response dialog to verify that the
	 * user really has the corresponding private key.
	 */
	while (read_keyfile_line(f, file, line, sizeof(line), &linenum) != -1) {
		char *cp;
		char *key_options;
		int keybits;

		/* Skip leading whitespace, empty and comment lines. */
		for (cp = line; *cp == ' ' || *cp == '\t'; cp++)
			;
		if (!*cp || *cp == '\n' || *cp == '#')
			continue;

		/*
		 * Check if there are options for this key, and if so,
		 * save their starting address and skip the option part
		 * for now.  If there are no options, set the starting
		 * address to NULL.
		 */
		if (*cp < '0' || *cp > '9') {
			int quoted = 0;
			key_options = cp;
			for (; *cp && (quoted || (*cp != ' ' && *cp != '\t')); cp++) {
				if (*cp == '\\' && cp[1] == '"')
					cp++;	/* Skip both */
				else if (*cp == '"')
					quoted = !quoted;
			}
		} else
			key_options = NULL;

		/* Parse the key from the line. */
		if (hostfile_read_key(&cp, &bits, key) == 0) {
			debug("%.100s, line %lu: non ssh1 key syntax",
			    file, linenum);
			continue;
		}
		/* cp now points to the comment part. */

		/* Check if the we have found the desired key (identified by its modulus). */
		if (BN_cmp(key->rsa->n, client_n) != 0)
			continue;

		/* check the real bits  */
		keybits = BN_num_bits(key->rsa->n);
		if (keybits < 0 || bits != (u_int)keybits)
			logit("Warning: %s, line %lu: keysize mismatch: "
			    "actual %d vs. announced %d.",
			    file, linenum, BN_num_bits(key->rsa->n), bits);

		/* We have found the desired key. */
		/*
		 * If our options do not allow this key to be used,
		 * do not send challenge.
		 */
		if (!auth_parse_options(pw, key_options, file, linenum))
			continue;

		/* break out, this key is allowed */
		allowed = 1;
		break;
	}

	/* Restore the privileged uid. */
	restore_uid();

	/* Close the file. */
	xfree(file);
	fclose(f);

	/* return key if allowed */
	if (allowed && rkey != NULL)
		*rkey = key;
	else
		key_free(key);
	return (allowed);
}

/*
 * Performs the RSA authentication dialog with the client.  This returns
 * 0 if the client could not be authenticated, and 1 if authentication was
 * successful.  This may exit if there is a serious protocol violation.
 */
int
auth_rsa(Authctxt *authctxt, BIGNUM *client_n)
{
	Key *key;
	char *fp;
	struct passwd *pw = authctxt->pw;

	/* no user given */
	if (!authctxt->valid)
		return 0;

	if (!PRIVSEP(auth_rsa_key_allowed(pw, client_n, &key))) {
		auth_clear_options();
		return (0);
	}

	/* Perform the challenge-response dialog for this key. */
	if (!auth_rsa_challenge_dialog(key)) {
		/* Wrong response. */
		verbose("Wrong response to RSA authentication challenge.");
		packet_send_debug("Wrong response to RSA authentication challenge.");
		/*
		 * Break out of the loop. Otherwise we might send
		 * another challenge and break the protocol.
		 */
		key_free(key);
		return (0);
	}
	/*
	 * Correct response.  The client has been successfully
	 * authenticated. Note that we have not yet processed the
	 * options; this will be reset if the options cause the
	 * authentication to be rejected.
	 */
	fp = key_fingerprint(key, SSH_FP_MD5, SSH_FP_HEX);
	verbose("Found matching %s key: %s",
	    key_type(key), fp);
	xfree(fp);
	key_free(key);

	packet_send_debug("RSA authentication accepted.");
	return (1);
}
