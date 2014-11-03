/* Copyright 2011 Justin Erenkrantz and Greg Stein
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apr.h"
#include "apr_pools.h"
#include <apr_poll.h>
#include <apr_version.h>
#include <stdlib.h>

#include "serf.h"
#include "serf_private.h" /* for serf__log and serf__bucket_stream_create */

#include "test_server.h"

#define BUFSIZE 8192

/* Cleanup callback for a server. */
static apr_status_t cleanup_server(void *baton)
{
    serv_ctx_t *servctx = baton;
    apr_status_t status;

    if (servctx->serv_sock)
      status = apr_socket_close(servctx->serv_sock);
    else
      status = APR_EGENERAL;

    if (servctx->client_sock) {
        apr_socket_close(servctx->client_sock);
    }

    return status;
}

/* Replay support functions */
static void next_message(serv_ctx_t *servctx)
{
    servctx->cur_message++;
}

static void next_action(serv_ctx_t *servctx)
{
    servctx->cur_action++;
    servctx->action_buf_pos = 0;
}

static apr_status_t
socket_write(serv_ctx_t *serv_ctx, const char *data,
             apr_size_t *len)
{
    return apr_socket_send(serv_ctx->client_sock, data, len);
}

static apr_status_t
socket_read(serv_ctx_t *serv_ctx, char *data,
            apr_size_t *len)
{
    return apr_socket_recv(serv_ctx->client_sock, data, len);
}

static apr_status_t
create_client_socket(apr_socket_t **skt,
                     serv_ctx_t *servctx,
                     const char *url)
{
    apr_sockaddr_t *address;
    apr_uri_t uri;
    apr_status_t status;

    status = apr_uri_parse(servctx->pool, url, &uri);
    if (status != APR_SUCCESS)
        return status;

    status = apr_sockaddr_info_get(&address,
                                   uri.hostname,
                                   APR_UNSPEC,
                                   uri.port,
                                   0,
                                   servctx->pool);
    if (status != APR_SUCCESS)
        return status;

    status = apr_socket_create(skt,
                               address->family,
                               SOCK_STREAM,
#if APR_MAJOR_VERSION > 0
                               APR_PROTO_TCP,
#endif
                               servctx->pool);
    if (status != APR_SUCCESS)
        return status;

    /* Set the socket to be non-blocking */
    status = apr_socket_timeout_set(*skt, 0);
    if (status != APR_SUCCESS)
        return status;

    status = apr_socket_connect(*skt, address);
    if (status != APR_SUCCESS && !APR_STATUS_IS_EINPROGRESS(status))
        return status;

    return APR_SUCCESS;
}

static apr_status_t detect_eof(void *baton, serf_bucket_t *aggregate_bucket)
{
    return APR_EAGAIN;
}

/* Verify received requests and take the necessary actions
   (return a response, kill the connection ...) */
static apr_status_t replay(serv_ctx_t *servctx,
                           apr_int16_t rtnevents,
                           apr_pool_t *pool)
{
    apr_status_t status = APR_SUCCESS;
    test_server_action_t *action;

    if (rtnevents & APR_POLLIN) {
        if (servctx->message_list == NULL) {
            /* we're not expecting any requests to reach this server! */
            serf__log(TEST_VERBOSE, __FILE__,
                      "Received request where none was expected.\n");

            return SERF_ERROR_ISSUE_IN_TESTSUITE;
        }

        if (servctx->cur_action >= servctx->action_count) {
            char buf[128];
            apr_size_t len = sizeof(buf);

            status = servctx->read(servctx, buf, &len);
            if (! APR_STATUS_IS_EAGAIN(status)) {
                /* we're out of actions! */
                serf__log(TEST_VERBOSE, __FILE__,
                          "Received more requests than expected.\n");

                return SERF_ERROR_ISSUE_IN_TESTSUITE;
            }
            return status;
        }

        action = &servctx->action_list[servctx->cur_action];

        serf__log(TEST_VERBOSE, __FILE__,
                  "POLLIN while replaying action %d, kind: %d.\n",
                  servctx->cur_action, action->kind);

        /* Read the remaining data from the client and kill the socket. */
        if (action->kind == SERVER_IGNORE_AND_KILL_CONNECTION) {
            char buf[128];
            apr_size_t len = sizeof(buf);

            status = servctx->read(servctx, buf, &len);

            if (status == APR_EOF) {
                serf__log(TEST_VERBOSE, __FILE__,
                          "Killing this connection.\n");
                apr_socket_close(servctx->client_sock);
                servctx->client_sock = NULL;
                next_action(servctx);
                return APR_SUCCESS;
            }

            return status;
        }
        else if (action->kind == SERVER_RECV ||
                 (action->kind == SERVER_RESPOND &&
                  servctx->outstanding_responses == 0)) {
            apr_size_t msg_len, len;
            char buf[128];
            test_server_message_t *message;

            message = &servctx->message_list[servctx->cur_message];
            msg_len = strlen(message->text);

            do
            {
                len = msg_len - servctx->message_buf_pos;
                if (len > sizeof(buf))
                    len = sizeof(buf);

                status = servctx->read(servctx, buf, &len);
                if (SERF_BUCKET_READ_ERROR(status))
                    return status;

                if (status == APR_EOF) {
                    serf__log(TEST_VERBOSE, __FILE__,
                              "Server: Client hung up the connection.\n");
                    break;
                }
                if (servctx->options & TEST_SERVER_DUMP)
                    fwrite(buf, len, 1, stdout);

                if (strncmp(buf,
                            message->text + servctx->message_buf_pos,
                            len) != 0) {
                    /* ## TODO: Better diagnostics. */
                    printf("Expected: (\n");
                    fwrite(message->text + servctx->message_buf_pos, len, 1,
                           stdout);
                    printf(")\n");
                    printf("Actual: (\n");
                    fwrite(buf, len, 1, stdout);
                    printf(")\n");

                    return SERF_ERROR_ISSUE_IN_TESTSUITE;
                }

                servctx->message_buf_pos += len;

                if (servctx->message_buf_pos >= msg_len) {
                    next_message(servctx);
                    servctx->message_buf_pos -= msg_len;
                    if (action->kind == SERVER_RESPOND)
                        servctx->outstanding_responses++;
                    if (action->kind == SERVER_RECV)
                        next_action(servctx);
                    break;
                }
            } while (!status);
        }
        else if (action->kind == PROXY_FORWARD) {
            apr_size_t len;
            char buf[BUFSIZE];
            serf_bucket_t *tmp;

            /* Read all incoming data from the client to forward it to the
               server later. */
            do
            {
                len = BUFSIZE;

                status = servctx->read(servctx, buf, &len);
                if (SERF_BUCKET_READ_ERROR(status))
                    return status;

                serf__log(TEST_VERBOSE, __FILE__,
                          "proxy: reading %d bytes %.*s from client with "
                          "status %d.\n",
                          len, len, buf, status);

                if (status == APR_EOF) {
                    serf__log(TEST_VERBOSE, __FILE__,
                              "Proxy: client hung up the connection. Reset the "
                              "connection to the server.\n");
                    /* We have to stop forwarding, if a new connection opens
                       the CONNECT request should not be forwarded to the
                       server. */
                    next_action(servctx);
                }
                if (!servctx->servstream)
                    servctx->servstream = serf__bucket_stream_create(
                                              servctx->allocator,
                                              detect_eof,servctx);
                if (len) {
                    tmp = serf_bucket_simple_copy_create(buf, len,
                                                         servctx->allocator);
                    serf_bucket_aggregate_append(servctx->servstream, tmp);
                }
            } while (!status);
        }
    }
    if (rtnevents & APR_POLLOUT) {
        action = &servctx->action_list[servctx->cur_action];

        serf__log(TEST_VERBOSE, __FILE__,
                  "POLLOUT when replaying action %d, kind: %d.\n", servctx->cur_action,
                  action->kind);

        if (action->kind == SERVER_RESPOND && servctx->outstanding_responses) {
            apr_size_t msg_len;
            apr_size_t len;

            msg_len = strlen(action->text);
            len = msg_len - servctx->action_buf_pos;

            status = servctx->send(servctx,
                                   action->text + servctx->action_buf_pos,
                                   &len);
            if (status != APR_SUCCESS)
                return status;

            if (servctx->options & TEST_SERVER_DUMP)
                fwrite(action->text + servctx->action_buf_pos, len, 1, stdout);

            servctx->action_buf_pos += len;

            if (servctx->action_buf_pos >= msg_len) {
                next_action(servctx);
                servctx->outstanding_responses--;
            }
        }
        else if (action->kind == SERVER_KILL_CONNECTION ||
                 action->kind == SERVER_IGNORE_AND_KILL_CONNECTION) {
            serf__log(TEST_VERBOSE, __FILE__,
                      "Killing this connection.\n");
            apr_socket_close(servctx->client_sock);
            servctx->client_sock = NULL;
            next_action(servctx);
        }
        else if (action->kind == PROXY_FORWARD) {
            apr_size_t len;
            char *buf;

            if (!servctx->proxy_client_sock) {
                serf__log(TEST_VERBOSE, __FILE__, "Proxy: setting up connection "
                          "to server.\n");
                status = create_client_socket(&servctx->proxy_client_sock,
                                              servctx, action->text);
                if (!servctx->clientstream)
                    servctx->clientstream = serf__bucket_stream_create(
                                                servctx->allocator,
                                                detect_eof,servctx);
            }

            /* Send all data received from the server to the client. */
            do
            {
                apr_size_t readlen;

                readlen = BUFSIZE;

                status = serf_bucket_read(servctx->clientstream, readlen,
                                          &buf, &readlen);
                if (SERF_BUCKET_READ_ERROR(status))
                    return status;
                if (!readlen)
                    break;

                len = readlen;

                serf__log(TEST_VERBOSE, __FILE__,
                          "proxy: sending %d bytes to client.\n", len);
                status = servctx->send(servctx, buf, &len);
                if (status != APR_SUCCESS) {
                    return status;
                }
                
                if (len != readlen) /* abort for now, return buf to aggregate
                                       if not everything could be sent. */
                    return APR_EGENERAL;
            } while (!status);
        }
    }
    else if (rtnevents & APR_POLLIN) {
        /* ignore */
    }
    else {
        printf("Unknown rtnevents: %d\n", rtnevents);
        abort();
    }

    return status;
}

/* Exchange data between proxy and server */
static apr_status_t proxy_replay(serv_ctx_t *servctx,
                                 apr_int16_t rtnevents,
                                 apr_pool_t *pool)
{
    apr_status_t status;

    if (rtnevents & APR_POLLIN) {
        apr_size_t len;
        char buf[BUFSIZE];
        serf_bucket_t *tmp;

        serf__log(TEST_VERBOSE, __FILE__, "proxy_replay: POLLIN\n");
        /* Read all incoming data from the server to forward it to the
           client later. */
        do
        {
            len = BUFSIZE;

            status = apr_socket_recv(servctx->proxy_client_sock, buf, &len);
            if (SERF_BUCKET_READ_ERROR(status))
                return status;

            serf__log(TEST_VERBOSE, __FILE__,
                      "proxy: reading %d bytes %.*s from server.\n",
                      len, len, buf);
            tmp = serf_bucket_simple_copy_create(buf, len,
                                                 servctx->allocator);
            serf_bucket_aggregate_append(servctx->clientstream, tmp);
        } while (!status);
    }

    if (rtnevents & APR_POLLOUT) {
        apr_size_t len;
        char *buf;

        serf__log(TEST_VERBOSE, __FILE__, "proxy_replay: POLLOUT\n");
        /* Send all data received from the client to the server. */
        do
        {
            apr_size_t readlen;

            readlen = BUFSIZE;

            if (!servctx->servstream)
                servctx->servstream = serf__bucket_stream_create(
                                          servctx->allocator,
                                          detect_eof,servctx);
            status = serf_bucket_read(servctx->servstream, BUFSIZE,
                                      &buf, &readlen);
            if (SERF_BUCKET_READ_ERROR(status))
                return status;
            if (!readlen)
                break;

            len = readlen;

            serf__log(TEST_VERBOSE, __FILE__,
                      "proxy: sending %d bytes %.*s to server.\n",
                      len, len, buf);
            status = apr_socket_send(servctx->proxy_client_sock, buf, &len);
            if (status != APR_SUCCESS) {
                return status;
            }

            if (len != readlen) /* abort for now */
                return APR_EGENERAL;
        } while (!status);
    }
    else if (rtnevents & APR_POLLIN) {
        /* ignore */
    }
    else {
        printf("Unknown rtnevents: %d\n", rtnevents);
        abort();
    }

    return status;
}

apr_status_t run_test_server(serv_ctx_t *servctx,
                             apr_short_interval_time_t duration,
                             apr_pool_t *pool)
{
    apr_status_t status;
    apr_pollset_t *pollset;
    apr_int32_t num;
    const apr_pollfd_t *desc;

    /* create a new pollset */
#ifdef BROKEN_WSAPOLL
    status = apr_pollset_create_ex(&pollset, 32, pool, 0,
                                 APR_POLLSET_SELECT);
#else
    status = apr_pollset_create(&pollset, 32, pool, 0);
#endif

    if (status != APR_SUCCESS)
        return status;

    /* Don't accept new connection while processing client connection. At
       least for present time.*/
    if (servctx->client_sock) {
        apr_pollfd_t pfd = { 0 };

        pfd.desc_type = APR_POLL_SOCKET;
        pfd.desc.s = servctx->client_sock;
        pfd.reqevents = APR_POLLIN | APR_POLLOUT;

        status = apr_pollset_add(pollset, &pfd);
        if (status != APR_SUCCESS)
            goto cleanup;

        if (servctx->proxy_client_sock) {
            apr_pollfd_t pfd = { 0 };

            pfd.desc_type = APR_POLL_SOCKET;
            pfd.desc.s = servctx->proxy_client_sock;
            pfd.reqevents = APR_POLLIN | APR_POLLOUT;

            status = apr_pollset_add(pollset, &pfd);
            if (status != APR_SUCCESS)
                goto cleanup;
        }
    }
    else {
        apr_pollfd_t pfd = { 0 };

        pfd.desc_type = APR_POLL_SOCKET;
        pfd.desc.s = servctx->serv_sock;
        pfd.reqevents = APR_POLLIN;

        status = apr_pollset_add(pollset, &pfd);
        if (status != APR_SUCCESS)
            goto cleanup;
    }

    status = apr_pollset_poll(pollset, APR_USEC_PER_SEC >> 1, &num, &desc);
    if (status != APR_SUCCESS)
        goto cleanup;

    while (num--) {
        if (desc->desc.s == servctx->serv_sock) {
            status = apr_socket_accept(&servctx->client_sock, servctx->serv_sock,
                                       servctx->pool);
            if (status != APR_SUCCESS)
                goto cleanup;

            serf__log_skt(TEST_VERBOSE, __FILE__, servctx->client_sock,
                          "server/proxy accepted incoming connection.\n");


            apr_socket_opt_set(servctx->client_sock, APR_SO_NONBLOCK, 1);
            apr_socket_timeout_set(servctx->client_sock, 0);

            status = APR_SUCCESS;
            goto cleanup;
        }

        if (desc->desc.s == servctx->client_sock) {
            if (servctx->handshake) {
                status = servctx->handshake(servctx);
                if (status)
                    goto cleanup;
            }

            /* Replay data to socket. */
            status = replay(servctx, desc->rtnevents, pool);

            if (APR_STATUS_IS_EOF(status)) {
                apr_socket_close(servctx->client_sock);
                servctx->client_sock = NULL;
                if (servctx->reset)
                    servctx->reset(servctx);

                /* If this is a proxy and the client closed the connection, also
                   close the connection to the server. */
                if (servctx->proxy_client_sock) {
                    apr_socket_close(servctx->proxy_client_sock);
                    servctx->proxy_client_sock = NULL;
                    goto cleanup;
                }
            }
            else if (APR_STATUS_IS_EAGAIN(status)) {
                status = APR_SUCCESS;
            }
            else if (status != APR_SUCCESS) {
                /* Real error. */
                goto cleanup;
            }
        }
        if (desc->desc.s == servctx->proxy_client_sock) {
            /* Replay data to proxy socket. */
            status = proxy_replay(servctx, desc->rtnevents, pool);
            if (APR_STATUS_IS_EOF(status)) {
                apr_socket_close(servctx->proxy_client_sock);
                servctx->proxy_client_sock = NULL;
            }
            else if (APR_STATUS_IS_EAGAIN(status)) {
                status = APR_SUCCESS;
            }
            else if (status != APR_SUCCESS) {
                /* Real error. */
                goto cleanup;
            }
        }

        desc++;
    }

cleanup:
    apr_pollset_destroy(pollset);

    return status;
}


/* Setup the context needed to start a TCP server on adress.
   message_list is a list of expected requests.
   action_list is the list of responses to be returned in order.
 */
void setup_test_server(serv_ctx_t **servctx_p,
                       apr_sockaddr_t *address,
                       test_server_message_t *message_list,
                       apr_size_t message_count,
                       test_server_action_t *action_list,
                       apr_size_t action_count,
                       apr_int32_t options,
                       apr_pool_t *pool)
{
    serv_ctx_t *servctx;

    servctx = apr_pcalloc(pool, sizeof(*servctx));
    apr_pool_cleanup_register(pool, servctx,
                              cleanup_server,
                              apr_pool_cleanup_null);
    *servctx_p = servctx;

    servctx->serv_addr = address;
    servctx->options = options;
    servctx->pool = pool;
    servctx->allocator = serf_bucket_allocator_create(pool, NULL, NULL);
    servctx->message_list = message_list;
    servctx->message_count = message_count;
    servctx->action_list = action_list;
    servctx->action_count = action_count;

    /* Start replay from first action. */
    servctx->cur_action = 0;
    servctx->action_buf_pos = 0;
    servctx->outstanding_responses = 0;

    servctx->read = socket_read;
    servctx->send = socket_write;

    *servctx_p = servctx;
}

apr_status_t start_test_server(serv_ctx_t *servctx)
{
    apr_status_t status;
    apr_socket_t *serv_sock;

    /* create server socket */
#if APR_VERSION_AT_LEAST(1, 0, 0)
    status = apr_socket_create(&serv_sock, servctx->serv_addr->family,
                               SOCK_STREAM, 0,
                               servctx->pool);
#else
    status = apr_socket_create(&serv_sock, servctx->serv_addr->family,
                               SOCK_STREAM,
                               servctx->pool);
#endif

    if (status != APR_SUCCESS)
        return status;

    apr_socket_opt_set(serv_sock, APR_SO_NONBLOCK, 1);
    apr_socket_timeout_set(serv_sock, 0);
    apr_socket_opt_set(serv_sock, APR_SO_REUSEADDR, 1);

    status = apr_socket_bind(serv_sock, servctx->serv_addr);
    if (status != APR_SUCCESS)
        return status;

    /* listen for clients */
    status = apr_socket_listen(serv_sock, SOMAXCONN);
    if (status != APR_SUCCESS)
        return status;

    servctx->serv_sock = serv_sock;
    servctx->client_sock = NULL;

    return APR_SUCCESS;
}
