/*
 * gpg_agent.c: GPG Agent provider for SVN_AUTH_CRED_*
 *
 * ====================================================================
 *    Licensed to the Apache Software Foundation (ASF) under one
 *    or more contributor license agreements.  See the NOTICE file
 *    distributed with this work for additional information
 *    regarding copyright ownership.  The ASF licenses this file
 *    to you under the Apache License, Version 2.0 (the
 *    "License"); you may not use this file except in compliance
 *    with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing,
 *    software distributed under the License is distributed on an
 *    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *    KIND, either express or implied.  See the License for the
 *    specific language governing permissions and limitations
 *    under the License.
 * ====================================================================
 */

/* ==================================================================== */

/* This auth provider stores a plaintext password in memory managed by
 * a running gpg-agent. In contrast to other password store providers
 * it does not save the password to disk.
 *
 * Prompting is performed by the gpg-agent using a "pinentry" program
 * which needs to be installed separately. There are several pinentry
 * implementations with different front-ends (e.g. qt, gtk, ncurses).
 *
 * The gpg-agent will let the password time out after a while,
 * or immediately when it receives the SIGHUP signal.
 * When the password has timed out it will automatically prompt the
 * user for the password again. This is transparent to Subversion.
 *
 * SECURITY CONSIDERATIONS:
 *
 * Communication to the agent happens over a UNIX socket, which is located
 * in a directory which only the user running Subversion can access.
 * However, any program the user runs could access this socket and get
 * the Subversion password if the program knows the "cache ID" Subversion
 * uses for the password.
 * The cache ID is very easy to obtain for programs running as the same user.
 * Subversion uses the MD5 of the realmstring as cache ID, and these checksums
 * are also used as filenames within ~/.subversion/auth/svn.simple.
 * Unlike GNOME Keyring or KDE Wallet, the user is not prompted for
 * permission if another program attempts to access the password.
 *
 * Therefore, while the gpg-agent is running and has the password cached,
 * this provider is no more secure than a file storing the password in
 * plaintext.
 */


/*** Includes. ***/

#ifndef WIN32

#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <apr_pools.h>
#include "svn_auth.h"
#include "svn_config.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "svn_cmdline.h"
#include "svn_checksum.h"
#include "svn_string.h"

#include "private/svn_auth_private.h"

#include "svn_private_config.h"

#ifdef SVN_HAVE_GPG_AGENT

#define BUFFER_SIZE 1024

/* Modify STR in-place such that blanks are escaped as required by the
 * gpg-agent protocol. Return a pointer to STR. */
static char *
escape_blanks(char *str)
{
  char *s = str;

  while (*s)
    {
      if (*s == ' ')
        *s = '+';
      s++;
    }

  return str;
}

/* Attempt to read a gpg-agent response message from the socket SD into
 * buffer BUF. Buf is assumed to be N bytes large. Return TRUE if a response
 * message could be read that fits into the buffer. Else return FALSE.
 * If a message could be read it will always be NUL-terminated and the
 * trailing newline is retained. */
static svn_boolean_t
receive_from_gpg_agent(int sd, char *buf, size_t n)
{
  int i = 0;
  size_t recvd;
  char c;

  /* Clear existing buffer content before reading response. */
  if (n > 0)
    *buf = '\0';

  /* Require the message to fit into the buffer and be terminated
   * with a newline. */
  while (i < n)
    {
      recvd = read(sd, &c, 1);
      if (recvd == -1)
        return FALSE;
      buf[i] = c;
      i++;
      if (i < n && c == '\n')
        {
          buf[i] = '\0';
          return TRUE;
        }
    }

    return FALSE;
}

/* Using socket SD, send the option OPTION with the specified VALUE
 * to the gpg agent. Store the response in BUF, assumed to be N bytes
 * in size, and evaluate the response. Return TRUE if the agent liked
 * the smell of the option, if there is such a thing, and doesn't feel
 * saturated by it. Else return FALSE.
 * Do temporary allocations in scratch_pool. */
static svn_boolean_t
send_option(int sd, char *buf, size_t n, const char *option, const char *value,
            apr_pool_t *scratch_pool)
{
  const char *request;

  request = apr_psprintf(scratch_pool, "OPTION %s=%s\n", option, value);

  if (write(sd, request, strlen(request)) == -1)
    return FALSE;

  if (!receive_from_gpg_agent(sd, buf, n))
    return FALSE;

  return (strncmp(buf, "OK", 2) == 0);
}


/* Locate a running GPG Agent, and return an open file descriptor
 * for communication with the agent in *NEW_SD. If no running agent
 * can be found, set *NEW_SD to -1. */
static svn_error_t *
find_running_gpg_agent(int *new_sd, apr_pool_t *pool)
{
  char *buffer;
  char *gpg_agent_info = NULL;
  const char *socket_name = NULL;
  const char *request = NULL;
  const char *p = NULL;
  char *ep = NULL;
  int sd;

  *new_sd = -1;

  gpg_agent_info = getenv("GPG_AGENT_INFO");
  if (gpg_agent_info != NULL)
    {
      apr_array_header_t *socket_details;

      socket_details = svn_cstring_split(gpg_agent_info, ":", TRUE,
                                         pool);
      socket_name = APR_ARRAY_IDX(socket_details, 0, const char *);
    }
  else
    return SVN_NO_ERROR;

  if (socket_name != NULL)
    {
      struct sockaddr_un addr;

      addr.sun_family = AF_UNIX;
      strncpy(addr.sun_path, socket_name, sizeof(addr.sun_path) - 1);
      addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

      sd = socket(AF_UNIX, SOCK_STREAM, 0);
      if (sd == -1)
        return SVN_NO_ERROR;

      if (connect(sd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        {
          close(sd);
          return SVN_NO_ERROR;
        }
    }
  else
    return SVN_NO_ERROR;

  /* Receive the connection status from the gpg-agent daemon. */
  buffer = apr_palloc(pool, BUFFER_SIZE);
  if (!receive_from_gpg_agent(sd, buffer, BUFFER_SIZE))
    {
      close(sd);
      return SVN_NO_ERROR;
    }

  if (strncmp(buffer, "OK", 2) != 0)
    {
      close(sd);
      return SVN_NO_ERROR;
    }

  /* The GPG-Agent documentation says:
   *  "Clients should deny to access an agent with a socket name which does
   *   not match its own configuration". */
  request = "GETINFO socket_name\n";
  if (write(sd, request, strlen(request)) == -1)
    {
      close(sd);
      return SVN_NO_ERROR;
    }
  if (!receive_from_gpg_agent(sd, buffer, BUFFER_SIZE))
    {
      close(sd);
      return SVN_NO_ERROR;
    }
  if (strncmp(buffer, "D", 1) == 0)
    p = &buffer[2];
  if (!p)
    {
      close(sd);
      return SVN_NO_ERROR;
    }
  ep = strchr(p, '\n');
  if (ep != NULL)
    *ep = '\0';
  if (strcmp(socket_name, p) != 0)
    {
      close(sd);
      return SVN_NO_ERROR;
    }
  /* The agent will terminate its response with "OK". */
  if (!receive_from_gpg_agent(sd, buffer, BUFFER_SIZE))
    {
      close(sd);
      return SVN_NO_ERROR;
    }
  if (strncmp(buffer, "OK", 2) != 0)
    {
      close(sd);
      return SVN_NO_ERROR;
    }

  *new_sd = sd;
  return SVN_NO_ERROR;
}

/* Implementation of svn_auth__password_get_t that retrieves the password
   from gpg-agent */
static svn_error_t *
password_get_gpg_agent(svn_boolean_t *done,
                       const char **password,
                       apr_hash_t *creds,
                       const char *realmstring,
                       const char *username,
                       apr_hash_t *parameters,
                       svn_boolean_t non_interactive,
                       apr_pool_t *pool)
{
  int sd;
  const char *p = NULL;
  char *ep = NULL;
  char *buffer;
  const char *request = NULL;
  const char *cache_id = NULL;
  const char *tty_name;
  const char *tty_type;
  const char *lc_ctype;
  const char *display;
  svn_checksum_t *digest = NULL;
  char *password_prompt;
  char *realm_prompt;

  *done = FALSE;

  SVN_ERR(find_running_gpg_agent(&sd, pool));
  if (sd == -1)
    return SVN_NO_ERROR;

  buffer = apr_palloc(pool, BUFFER_SIZE);

  /* Send TTY_NAME to the gpg-agent daemon. */
  tty_name = getenv("GPG_TTY");
  if (tty_name != NULL)
    {
      if (!send_option(sd, buffer, BUFFER_SIZE, "ttyname", tty_name, pool))
        {
          close(sd);
          return SVN_NO_ERROR;
        }
    }

  /* Send TTY_TYPE to the gpg-agent daemon. */
  tty_type = getenv("TERM");
  if (tty_type != NULL)
    {
      if (!send_option(sd, buffer, BUFFER_SIZE, "ttytype", tty_type, pool))
        {
          close(sd);
          return SVN_NO_ERROR;
        }
    }

  /* Compute LC_CTYPE. */
  lc_ctype = getenv("LC_ALL");
  if (lc_ctype == NULL)
    lc_ctype = getenv("LC_CTYPE");
  if (lc_ctype == NULL)
    lc_ctype = getenv("LANG");

  /* Send LC_CTYPE to the gpg-agent daemon. */
  if (lc_ctype != NULL)
    {
      if (!send_option(sd, buffer, BUFFER_SIZE, "lc-ctype", lc_ctype, pool))
        {
          close(sd);
          return SVN_NO_ERROR;
        }
    }

  /* Send DISPLAY to the gpg-agent daemon. */
  display = getenv("DISPLAY");
  if (display != NULL)
    {
      if (!send_option(sd, buffer, BUFFER_SIZE, "display", display, pool))
        {
          close(sd);
          return SVN_NO_ERROR;
        }
    }

  /* Create the CACHE_ID which will be generated based on REALMSTRING similar
     to other password caching mechanisms. */
  SVN_ERR(svn_checksum(&digest, svn_checksum_md5, realmstring,
                       strlen(realmstring), pool));
  cache_id = svn_checksum_to_cstring(digest, pool);

  password_prompt = apr_psprintf(pool, _("Password for '%s': "), username);
  realm_prompt = apr_psprintf(pool, _("Enter your Subversion password for %s"),
                              realmstring);
  request = apr_psprintf(pool,
                         "GET_PASSPHRASE --data %s--repeat=1 "
                         "%s X %s %s\n",
                         non_interactive ? "--no-ask " : "",
                         cache_id,
                         escape_blanks(password_prompt),
                         escape_blanks(realm_prompt));

  if (write(sd, request, strlen(request)) == -1)
    {
      close(sd);
      return SVN_NO_ERROR;
    }
  if (!receive_from_gpg_agent(sd, buffer, BUFFER_SIZE))
    {
      close(sd);
      return SVN_NO_ERROR;
    }

  close(sd);

  if (strncmp(buffer, "ERR", 3) == 0)
    return SVN_NO_ERROR;

  p = NULL;
  if (strncmp(buffer, "D", 1) == 0)
    p = &buffer[2];

  if (!p)
    return SVN_NO_ERROR;

  ep = strchr(p, '\n');
  if (ep != NULL)
    *ep = '\0';

  *password = p;

  *done = TRUE;
  return SVN_NO_ERROR;
}


/* Implementation of svn_auth__password_set_t that would store the
   password in GPG Agent if that's how this particular integration
   worked.  But it isn't.  GPG Agent stores the password provided by
   the user via the pinentry program immediately upon its provision
   (and regardless of its accuracy as passwords go), so we just need
   to check if a running GPG Agent exists. */
static svn_error_t *
password_set_gpg_agent(svn_boolean_t *done,
                       apr_hash_t *creds,
                       const char *realmstring,
                       const char *username,
                       const char *password,
                       apr_hash_t *parameters,
                       svn_boolean_t non_interactive,
                       apr_pool_t *pool)
{
  int sd;

  *done = FALSE;

  SVN_ERR(find_running_gpg_agent(&sd, pool));
  if (sd == -1)
    return SVN_NO_ERROR;

  close(sd);
  *done = TRUE;

  return SVN_NO_ERROR;
}


/* An implementation of svn_auth_provider_t::first_credentials() */
static svn_error_t *
simple_gpg_agent_first_creds(void **credentials,
                             void **iter_baton,
                             void *provider_baton,
                             apr_hash_t *parameters,
                             const char *realmstring,
                             apr_pool_t *pool)
{
  return svn_auth__simple_creds_cache_get(credentials, iter_baton,
                                          provider_baton, parameters,
                                          realmstring, password_get_gpg_agent,
                                          SVN_AUTH__GPG_AGENT_PASSWORD_TYPE,
                                          pool);
}


/* An implementation of svn_auth_provider_t::save_credentials() */
static svn_error_t *
simple_gpg_agent_save_creds(svn_boolean_t *saved,
                            void *credentials,
                            void *provider_baton,
                            apr_hash_t *parameters,
                            const char *realmstring,
                            apr_pool_t *pool)
{
  return svn_auth__simple_creds_cache_set(saved, credentials,
                                          provider_baton, parameters,
                                          realmstring, password_set_gpg_agent,
                                          SVN_AUTH__GPG_AGENT_PASSWORD_TYPE,
                                          pool);
}


static const svn_auth_provider_t gpg_agent_simple_provider = {
  SVN_AUTH_CRED_SIMPLE,
  simple_gpg_agent_first_creds,
  NULL,
  simple_gpg_agent_save_creds
};


/* Public API */
void
svn_auth_get_gpg_agent_simple_provider(svn_auth_provider_object_t **provider,
                                       apr_pool_t *pool)
{
  svn_auth_provider_object_t *po = apr_pcalloc(pool, sizeof(*po));

  po->vtable = &gpg_agent_simple_provider;
  *provider = po;
}

#endif /* SVN_HAVE_GPG_AGENT */
#endif /* !WIN32 */
