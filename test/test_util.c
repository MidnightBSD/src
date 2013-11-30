/* Copyright 2002-2007 Justin Erenkrantz and Greg Stein
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
#include <stdlib.h>

#include "serf.h"

#include "test_serf.h"
#include "server/test_server.h"


/*****************************************************************************/
/* Server setup function(s)
 */

#define HTTP_SERV_URL  "http://localhost:" SERV_PORT_STR
#define HTTPS_SERV_URL "https://localhost:" SERV_PORT_STR

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

    return APR_SUCCESS;
}

static apr_status_t setup(test_baton_t **tb_p,
                          serf_connection_setup_t conn_setup,
                          const char *serv_url,
                          int use_proxy,
                          apr_size_t message_count,
                          apr_pool_t *pool)
{
    apr_status_t status;
    test_baton_t *tb;
    apr_uri_t url;

    tb = apr_pcalloc(pool, sizeof(*tb));
    *tb_p = tb;

    tb->pool = pool;
    tb->context = serf_context_create(pool);
    tb->bkt_alloc = serf_bucket_allocator_create(pool, NULL, NULL);

    tb->accepted_requests = apr_array_make(pool, message_count, sizeof(int));
    tb->sent_requests = apr_array_make(pool, message_count, sizeof(int));
    tb->handled_requests = apr_array_make(pool, message_count, sizeof(int));


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

    status = apr_uri_parse(pool, serv_url, &url);
    if (status != APR_SUCCESS)
        return status;

    status = serf_connection_create2(&tb->connection, tb->context,
                                     url,
                                     conn_setup,
                                     tb,
                                     default_closed_connection,
                                     tb,
                                     pool);

    return status;
}


apr_status_t test_https_server_setup(test_baton_t **tb_p,
                                     test_server_message_t *message_list,
                                     apr_size_t message_count,
                                     test_server_action_t *action_list,
                                     apr_size_t action_count,
                                     apr_int32_t options,
                                     serf_connection_setup_t conn_setup,
                                     const char *keyfile,
                                     const char *certfile,
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
    test_setup_https_server(&tb->serv_ctx, tb->serv_addr,
                            message_list, message_count,
                            action_list, action_count, options,
                            keyfile, certfile,
                            pool);
    status = test_start_server(tb->serv_ctx);

    return status;
}

apr_status_t test_server_setup(test_baton_t **tb_p,
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
    test_setup_server(&tb->serv_ctx, tb->serv_addr,
                      message_list, message_count,
                      action_list, action_count, options,
                      pool);
    status = test_start_server(tb->serv_ctx);

    return status;
}

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
    test_setup_server(&tb->serv_ctx, tb->serv_addr,
                      serv_message_list, serv_message_count,
                      serv_action_list, serv_action_count,
                      options,
                      pool);
    status = test_start_server(tb->serv_ctx);
    if (status != APR_SUCCESS)
        return status;

    /* Prepare the proxy. */
    test_setup_server(&tb->proxy_ctx, tb->proxy_addr,
                      proxy_message_list, proxy_message_count,
                      proxy_action_list, proxy_action_count,
                      options,
                      pool);
    status = test_start_server(tb->proxy_ctx);

    return status;
}

apr_status_t test_server_teardown(test_baton_t *tb, apr_pool_t *pool)
{
    serf_connection_close(tb->connection);

    if (tb->serv_ctx)
        test_server_destroy(tb->serv_ctx, pool);
    if (tb->proxy_ctx)
        test_server_destroy(tb->proxy_ctx, pool);

    return APR_SUCCESS;
}

apr_pool_t *test_setup(void)
{
    apr_pool_t *test_pool;
    apr_pool_create(&test_pool, NULL);
    return test_pool;
}

void test_teardown(apr_pool_t *test_pool)
{
    apr_pool_destroy(test_pool);
}
