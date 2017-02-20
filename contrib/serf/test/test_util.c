/* ====================================================================
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

#include "apr.h"
#include "apr_pools.h"
#include <apr_env.h>
#include <apr_strings.h>

#include <stdlib.h>

#include "serf.h"

#include "test_serf.h"
#include "server/test_server.h"


/*****************************************************************************/
/* Server setup function(s)
 */

#define HTTP_SERV_URL  "http://localhost:" SERV_PORT_STR
#define HTTPS_SERV_URL "https://localhost:" SERV_PORT_STR

const char * get_srcdir_file(apr_pool_t *pool, const char * file)
{
    char *srcdir = "";

    if (apr_env_get(&srcdir, "srcdir", pool) == APR_SUCCESS) {
        return apr_pstrcat(pool, srcdir, "/", file, NULL);
    }
    else {
        return file;
    }
}

/* cleanup for conn */
static apr_status_t cleanup_conn(void *baton)
{
    serf_connection_t *conn = baton;

    serf_connection_close(conn);

    return APR_SUCCESS;
}

static apr_status_t default_server_address(apr_sockaddr_t **address,
                                           apr_pool_t *pool)
{
    return apr_sockaddr_info_get(address,
                                 "localhost", APR_UNSPEC, SERV_PORT, 0,
                                 pool);
}

static apr_status_t default_proxy_address(apr_sockaddr_t **address,
                                          apr_pool_t *pool)
{
    return apr_sockaddr_info_get(address,
                                 "localhost", APR_UNSPEC, PROXY_PORT, 0,
                                 pool);
}

/* Default implementation of a serf_connection_closed_t callback. */
static void default_closed_connection(serf_connection_t *conn,
                                      void *closed_baton,
                                      apr_status_t why,
                                      apr_pool_t *pool)
{
    if (why) {
        abort();
    }
}

/* Default implementation of a serf_connection_setup_t callback. */
static apr_status_t default_http_conn_setup(apr_socket_t *skt,
                                            serf_bucket_t **input_bkt,
                                            serf_bucket_t **output_bkt,
                                            void *setup_baton,
                                            apr_pool_t *pool)
{
    test_baton_t *tb = setup_baton;

    *input_bkt = serf_bucket_socket_create(skt, tb->bkt_alloc);
    return APR_SUCCESS;
}

/* This function makes serf use SSL on the connection. */
apr_status_t default_https_conn_setup(apr_socket_t *skt,
                                      serf_bucket_t **input_bkt,
                                      serf_bucket_t **output_bkt,
                                      void *setup_baton,
                                      apr_pool_t *pool)
{
    test_baton_t *tb = setup_baton;

    *input_bkt = serf_bucket_socket_create(skt, tb->bkt_alloc);
    *input_bkt = serf_bucket_ssl_decrypt_create(*input_bkt, NULL,
                                                tb->bkt_alloc);
    tb->ssl_context = serf_bucket_ssl_encrypt_context_get(*input_bkt);

    if (output_bkt) {
        *output_bkt = serf_bucket_ssl_encrypt_create(*output_bkt,
                                                     tb->ssl_context,
                                                     tb->bkt_alloc);
    }

    if (tb->server_cert_cb)
        serf_ssl_server_cert_callback_set(tb->ssl_context,
                                          tb->server_cert_cb,
                                          tb);

    serf_ssl_set_hostname(tb->ssl_context, "localhost");

    return APR_SUCCESS;
}

apr_status_t use_new_connection(test_baton_t *tb,
                                apr_pool_t *pool)
{
    apr_uri_t url;
    apr_status_t status;

    if (tb->connection)
        cleanup_conn(tb->connection);
    tb->connection = NULL;

    status = apr_uri_parse(pool, tb->serv_url, &url);
    if (status != APR_SUCCESS)
        return status;

    status = serf_connection_create2(&tb->connection, tb->context,
                                     url,
                                     tb->conn_setup,
                                     tb,
                                     default_closed_connection,
                                     tb,
                                     pool);
    apr_pool_cleanup_register(pool, tb->connection, cleanup_conn,
                              apr_pool_cleanup_null);

    return status;
}

/* Setup the client context, ready to connect and send requests to a
   server.*/
static apr_status_t setup(test_baton_t **tb_p,
                          serf_connection_setup_t conn_setup,
                          const char *serv_url,
                          int use_proxy,
                          apr_size_t message_count,
                          apr_pool_t *pool)
{
    test_baton_t *tb;
    apr_status_t status;

    tb = apr_pcalloc(pool, sizeof(*tb));
    *tb_p = tb;

    tb->pool = pool;
    tb->context = serf_context_create(pool);
    tb->bkt_alloc = serf_bucket_allocator_create(pool, NULL, NULL);

    tb->accepted_requests = apr_array_make(pool, message_count, sizeof(int));
    tb->sent_requests = apr_array_make(pool, message_count, sizeof(int));
    tb->handled_requests = apr_array_make(pool, message_count, sizeof(int));

    tb->serv_url = serv_url;
    tb->conn_setup = conn_setup;

    status = default_server_address(&tb->serv_addr, pool);
    if (status != APR_SUCCESS)
        return status;

    if (use_proxy) {
        status = default_proxy_address(&tb->proxy_addr, pool);
        if (status != APR_SUCCESS)
            return status;

        /* Configure serf to use the proxy server */
        serf_config_proxy(tb->context, tb->proxy_addr);
    }

    status = use_new_connection(tb, pool);

    return status;
}

/* Setup an https server and the client context to connect to that server */
apr_status_t test_https_server_setup(test_baton_t **tb_p,
                                     test_server_message_t *message_list,
                                     apr_size_t message_count,
                                     test_server_action_t *action_list,
                                     apr_size_t action_count,
                                     apr_int32_t options,
                                     serf_connection_setup_t conn_setup,
                                     const char *keyfile,
                                     const char **certfiles,
                                     const char *client_cn,
                                     serf_ssl_need_server_cert_t server_cert_cb,
                                     apr_pool_t *pool)
{
    apr_status_t status;
    test_baton_t *tb;

    status = setup(tb_p,
                   conn_setup ? conn_setup : default_https_conn_setup,
                   HTTPS_SERV_URL,
                   FALSE,
                   message_count,
                   pool);
    if (status != APR_SUCCESS)
        return status;

    tb = *tb_p;
    tb->server_cert_cb = server_cert_cb;

    /* Prepare a server. */
    setup_https_test_server(&tb->serv_ctx, tb->serv_addr,
                            message_list, message_count,
                            action_list, action_count, options,
                            keyfile, certfiles, client_cn,
                            pool);
    status = start_test_server(tb->serv_ctx);

    return status;
}

/* Setup an http server and the client context to connect to that server */
apr_status_t test_http_server_setup(test_baton_t **tb_p,
                                    test_server_message_t *message_list,
                                    apr_size_t message_count,
                                    test_server_action_t *action_list,
                                    apr_size_t action_count,
                                    apr_int32_t options,
                                    serf_connection_setup_t conn_setup,
                                    apr_pool_t *pool)
{
    apr_status_t status;
    test_baton_t *tb;

    status = setup(tb_p,
                   conn_setup ? conn_setup : default_http_conn_setup,
                   HTTP_SERV_URL,
                   FALSE,
                   message_count,
                   pool);
    if (status != APR_SUCCESS)
        return status;

    tb = *tb_p;

    /* Prepare a server. */
    setup_test_server(&tb->serv_ctx, tb->serv_addr,
                      message_list, message_count,
                      action_list, action_count, options,
                      pool);
    status = start_test_server(tb->serv_ctx);

    return status;
}

/* Setup a proxy server and an http server and the client context to connect to
   that proxy server */
apr_status_t
test_server_proxy_setup(test_baton_t **tb_p,
                        test_server_message_t *serv_message_list,
                        apr_size_t serv_message_count,
                        test_server_action_t *serv_action_list,
                        apr_size_t serv_action_count,
                        test_server_message_t *proxy_message_list,
                        apr_size_t proxy_message_count,
                        test_server_action_t *proxy_action_list,
                        apr_size_t proxy_action_count,
                        apr_int32_t options,
                        serf_connection_setup_t conn_setup,
                        apr_pool_t *pool)
{
    apr_status_t status;
    test_baton_t *tb;

    status = setup(tb_p,
                   conn_setup ? conn_setup : default_http_conn_setup,
                   HTTP_SERV_URL,
                   TRUE,
                   serv_message_count,
                   pool);
    if (status != APR_SUCCESS)
        return status;

    tb = *tb_p;

    /* Prepare the server. */
    setup_test_server(&tb->serv_ctx, tb->serv_addr,
                      serv_message_list, serv_message_count,
                      serv_action_list, serv_action_count,
                      options,
                      pool);
    status = start_test_server(tb->serv_ctx);
    if (status != APR_SUCCESS)
        return status;

    /* Prepare the proxy. */
    setup_test_server(&tb->proxy_ctx, tb->proxy_addr,
                      proxy_message_list, proxy_message_count,
                      proxy_action_list, proxy_action_count,
                      options,
                      pool);
    status = start_test_server(tb->proxy_ctx);

    return status;
}

/* Setup a proxy server and a https server and the client context to connect to
   that proxy server */
apr_status_t
test_https_server_proxy_setup(test_baton_t **tb_p,
                              test_server_message_t *serv_message_list,
                              apr_size_t serv_message_count,
                              test_server_action_t *serv_action_list,
                              apr_size_t serv_action_count,
                              test_server_message_t *proxy_message_list,
                              apr_size_t proxy_message_count,
                              test_server_action_t *proxy_action_list,
                              apr_size_t proxy_action_count,
                              apr_int32_t options,
                              serf_connection_setup_t conn_setup,
                              const char *keyfile,
                              const char **certfiles,
                              const char *client_cn,
                              serf_ssl_need_server_cert_t server_cert_cb,
                              apr_pool_t *pool)
{
    apr_status_t status;
    test_baton_t *tb;

    status = setup(tb_p,
                   conn_setup ? conn_setup : default_https_conn_setup,
                   HTTPS_SERV_URL,
                   TRUE, /* use proxy */
                   serv_message_count,
                   pool);
    if (status != APR_SUCCESS)
        return status;

    tb = *tb_p;
    tb->server_cert_cb = server_cert_cb;

    /* Prepare a https server. */
    setup_https_test_server(&tb->serv_ctx, tb->serv_addr,
                            serv_message_list, serv_message_count,
                            serv_action_list, serv_action_count,
                            options,
                            keyfile, certfiles, client_cn,
                            pool);
    status = start_test_server(tb->serv_ctx);

    /* Prepare the proxy. */
    setup_test_server(&tb->proxy_ctx, tb->proxy_addr,
                      proxy_message_list, proxy_message_count,
                      proxy_action_list, proxy_action_count,
                      options,
                      pool);
    status = start_test_server(tb->proxy_ctx);

    return status;
}

void *test_setup(void *dummy)
{
    apr_pool_t *test_pool;
    apr_pool_create(&test_pool, NULL);
    return test_pool;
}

void *test_teardown(void *baton)
{
    apr_pool_t *pool = baton;
    apr_pool_destroy(pool);

    return NULL;
}

/* Helper function, runs the client and server context loops and validates
 that no errors were encountered, and all messages were sent and received. */
apr_status_t
test_helper_run_requests_no_check(CuTest *tc, test_baton_t *tb,
                                  int num_requests,
                                  handler_baton_t handler_ctx[],
                                  apr_pool_t *pool)
{
    apr_pool_t *iter_pool;
    int i, done = 0;
    apr_status_t status;

    apr_pool_create(&iter_pool, pool);

    while (!done)
    {
        apr_pool_clear(iter_pool);

        /* run server event loop */
        status = run_test_server(tb->serv_ctx, 0, iter_pool);
        if (!APR_STATUS_IS_TIMEUP(status) &&
            SERF_BUCKET_READ_ERROR(status))
            return status;

        /* run proxy event loop */
        if (tb->proxy_ctx) {
            status = run_test_server(tb->proxy_ctx, 0, iter_pool);
            if (!APR_STATUS_IS_TIMEUP(status) &&
                SERF_BUCKET_READ_ERROR(status))
                return status;
        }

        /* run client event loop */
        status = serf_context_run(tb->context, 0, iter_pool);
        if (!APR_STATUS_IS_TIMEUP(status) &&
            SERF_BUCKET_READ_ERROR(status))
            return status;

        done = 1;
        for (i = 0; i < num_requests; i++)
            done &= handler_ctx[i].done;
    }
    apr_pool_destroy(iter_pool);

    return APR_SUCCESS;
}

void
test_helper_run_requests_expect_ok(CuTest *tc, test_baton_t *tb,
                                   int num_requests,
                                   handler_baton_t handler_ctx[],
                                   apr_pool_t *pool)
{
    apr_status_t status;

    status = test_helper_run_requests_no_check(tc, tb, num_requests,
                                               handler_ctx, pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    /* Check that all requests were received */
    CuAssertIntEquals(tc, num_requests, tb->sent_requests->nelts);
    CuAssertIntEquals(tc, num_requests, tb->accepted_requests->nelts);
    CuAssertIntEquals(tc, num_requests, tb->handled_requests->nelts);
}

serf_bucket_t* accept_response(serf_request_t *request,
                               serf_bucket_t *stream,
                               void *acceptor_baton,
                               apr_pool_t *pool)
{
    serf_bucket_t *c;
    serf_bucket_alloc_t *bkt_alloc;
    handler_baton_t *ctx = acceptor_baton;
    serf_bucket_t *response;

    /* get the per-request bucket allocator */
    bkt_alloc = serf_request_get_alloc(request);

    /* Create a barrier so the response doesn't eat us! */
    c = serf_bucket_barrier_create(stream, bkt_alloc);

    APR_ARRAY_PUSH(ctx->accepted_requests, int) = ctx->req_id;

    response = serf_bucket_response_create(c, bkt_alloc);

    if (strcasecmp(ctx->method, "HEAD") == 0)
      serf_bucket_response_set_head(response);

    return response;
}

apr_status_t setup_request(serf_request_t *request,
                           void *setup_baton,
                           serf_bucket_t **req_bkt,
                           serf_response_acceptor_t *acceptor,
                           void **acceptor_baton,
                           serf_response_handler_t *handler,
                           void **handler_baton,
                           apr_pool_t *pool)
{
    handler_baton_t *ctx = setup_baton;
    serf_bucket_t *body_bkt;

    if (ctx->request)
    {
        /* Create a raw request bucket. */
        *req_bkt = serf_bucket_simple_create(ctx->request, strlen(ctx->request),
                                             NULL, NULL,
                                             serf_request_get_alloc(request));
    }
    else
    {
        if (ctx->req_id >= 0) {
            /* create a simple body text */
            const char *str = apr_psprintf(pool, "%d", ctx->req_id);

            body_bkt = serf_bucket_simple_create(
                                        str, strlen(str), NULL, NULL,
                                        serf_request_get_alloc(request));
        }
        else
            body_bkt = NULL;

        *req_bkt =
        serf_request_bucket_request_create(request,
                                           ctx->method, ctx->path,
                                           body_bkt,
                                           serf_request_get_alloc(request));
    }

    APR_ARRAY_PUSH(ctx->sent_requests, int) = ctx->req_id;

    *acceptor = ctx->acceptor;
    *acceptor_baton = ctx;
    *handler = ctx->handler;
    *handler_baton = ctx;

    return APR_SUCCESS;
}

apr_status_t handle_response(serf_request_t *request,
                             serf_bucket_t *response,
                             void *handler_baton,
                             apr_pool_t *pool)
{
    handler_baton_t *ctx = handler_baton;

    if (! response) {
        serf_connection_request_create(ctx->tb->connection,
                                       setup_request,
                                       ctx);
        return APR_SUCCESS;
    }

    while (1) {
        apr_status_t status;
        const char *data;
        apr_size_t len;

        status = serf_bucket_read(response, 2048, &data, &len);
        if (SERF_BUCKET_READ_ERROR(status))
            return status;

        if (APR_STATUS_IS_EOF(status)) {
            APR_ARRAY_PUSH(ctx->handled_requests, int) = ctx->req_id;
            ctx->done = TRUE;
            return APR_EOF;
        }

        if (APR_STATUS_IS_EAGAIN(status)) {
            return status;
        }

    }

    return APR_SUCCESS;
}

void setup_handler(test_baton_t *tb, handler_baton_t *handler_ctx,
                   const char *method, const char *path,
                   int req_id,
                   serf_response_handler_t handler)
{
    handler_ctx->method = method;
    handler_ctx->path = path;
    handler_ctx->done = FALSE;

    handler_ctx->acceptor = accept_response;
    handler_ctx->acceptor_baton = NULL;
    handler_ctx->handler = handler ? handler : handle_response;
    handler_ctx->req_id = req_id;
    handler_ctx->accepted_requests = tb->accepted_requests;
    handler_ctx->sent_requests = tb->sent_requests;
    handler_ctx->handled_requests = tb->handled_requests;
    handler_ctx->tb = tb;
    handler_ctx->request = NULL;
}

void create_new_prio_request(test_baton_t *tb,
                             handler_baton_t *handler_ctx,
                             const char *method, const char *path,
                             int req_id)
{
    setup_handler(tb, handler_ctx, method, path, req_id, NULL);
    serf_connection_priority_request_create(tb->connection,
                                            setup_request,
                                            handler_ctx);
}

void create_new_request(test_baton_t *tb,
                        handler_baton_t *handler_ctx,
                        const char *method, const char *path,
                        int req_id)
{
    setup_handler(tb, handler_ctx, method, path, req_id, NULL);
    serf_connection_request_create(tb->connection,
                                   setup_request,
                                   handler_ctx);
}

void
create_new_request_with_resp_hdlr(test_baton_t *tb,
                                  handler_baton_t *handler_ctx,
                                  const char *method, const char *path,
                                  int req_id,
                                  serf_response_handler_t handler)
{
    setup_handler(tb, handler_ctx, method, path, req_id, handler);
    serf_connection_request_create(tb->connection,
                                   setup_request,
                                   handler_ctx);
}
