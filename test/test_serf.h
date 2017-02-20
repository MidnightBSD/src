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

#ifndef TEST_SERF_H
#define TEST_SERF_H

#include "CuTest.h"

#include <apr.h>
#include <apr_pools.h>
#include <apr_uri.h>

#include "serf.h"
#include "server/test_server.h"

/** These macros are provided by APR itself from version 1.3.
 * Definitions are provided here for when using older versions of APR.
 */

/** index into an apr_array_header_t */
#ifndef APR_ARRAY_IDX
#define APR_ARRAY_IDX(ary,i,type) (((type *)(ary)->elts)[i])
#endif

/** easier array-pushing syntax */
#ifndef APR_ARRAY_PUSH
#define APR_ARRAY_PUSH(ary,type) (*((type *)apr_array_push(ary)))
#endif

/* CuTest declarations */
CuSuite *getsuite(void);

CuSuite *test_context(void);
CuSuite *test_buckets(void);
CuSuite *test_ssl(void);
CuSuite *test_auth(void);
CuSuite *test_mock_bucket(void);

/* Test setup declarations */
#define CRLF "\r\n"
#define CR "\r"
#define LF "\n"

#define CHUNKED_REQUEST(len, body)\
        "GET / HTTP/1.1" CRLF\
        "Host: localhost:12345" CRLF\
        "Transfer-Encoding: chunked" CRLF\
        CRLF\
        #len CRLF\
        body CRLF\
        "0" CRLF\
        CRLF

#define CHUNKED_REQUEST_URI(uri, len, body)\
        "GET " uri " HTTP/1.1" CRLF\
        "Host: localhost:12345" CRLF\
        "Transfer-Encoding: chunked" CRLF\
        CRLF\
        #len CRLF\
        body CRLF\
        "0" CRLF\
        CRLF

#define CHUNKED_RESPONSE(len, body)\
        "HTTP/1.1 200 OK" CRLF\
        "Transfer-Encoding: chunked" CRLF\
        CRLF\
        #len CRLF\
        body CRLF\
        "0" CRLF\
        CRLF

#define CHUNKED_EMPTY_RESPONSE\
        "HTTP/1.1 200 OK" CRLF\
        "Transfer-Encoding: chunked" CRLF\
        CRLF\
        "0" CRLF\
        CRLF

typedef struct {
    /* Pool for resource allocation. */
    apr_pool_t *pool;

    serf_context_t *context;
    serf_connection_t *connection;
    serf_bucket_alloc_t *bkt_alloc;

    serv_ctx_t *serv_ctx;
    apr_sockaddr_t *serv_addr;

    serv_ctx_t *proxy_ctx;
    apr_sockaddr_t *proxy_addr;

    /* Cache connection params here so it gets user for a test to switch to a
       new connection. */
    const char *serv_url;
    serf_connection_setup_t conn_setup;

    /* Extra batons which can be freely used by tests. */
    void *user_baton;
    long user_baton_l;

    /* Flags that can be used to report situations, e.g. that a callback was
       called. */
    int result_flags;

    apr_array_header_t *accepted_requests, *handled_requests, *sent_requests;

    serf_ssl_context_t *ssl_context;
    serf_ssl_need_server_cert_t server_cert_cb;
} test_baton_t;

apr_status_t default_https_conn_setup(apr_socket_t *skt,
                                      serf_bucket_t **input_bkt,
                                      serf_bucket_t **output_bkt,
                                      void *setup_baton,
                                      apr_pool_t *pool);

apr_status_t use_new_connection(test_baton_t *tb,
                                apr_pool_t *pool);

apr_status_t test_https_server_setup(test_baton_t **tb_p,
                                     test_server_message_t *message_list,
                                     apr_size_t message_count,
                                     test_server_action_t *action_list,
                                     apr_size_t action_count,
                                     apr_int32_t options,
                                     serf_connection_setup_t conn_setup,
                                     const char *keyfile,
                                     const char **certfile,
                                     const char *client_cn,
                                     serf_ssl_need_server_cert_t server_cert_cb,
                                     apr_pool_t *pool);

apr_status_t test_http_server_setup(test_baton_t **tb_p,
                                    test_server_message_t *message_list,
                                    apr_size_t message_count,
                                    test_server_action_t *action_list,
                                    apr_size_t action_count,
                                    apr_int32_t options,
                                    serf_connection_setup_t conn_setup,
                                    apr_pool_t *pool);

apr_status_t test_server_proxy_setup(
                 test_baton_t **tb_p,
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
                 apr_pool_t *pool);

apr_status_t test_https_server_proxy_setup(
                 test_baton_t **tb_p,
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
                 apr_pool_t *pool);

void *test_setup(void *baton);
void *test_teardown(void *baton);

typedef struct {
    serf_response_acceptor_t acceptor;
    void *acceptor_baton;

    serf_response_handler_t handler;

    apr_array_header_t *sent_requests;
    apr_array_header_t *accepted_requests;
    apr_array_header_t *handled_requests;
    int req_id;

    const char *method;
    const char *path;
    /* Use this for a raw request message */
    const char *request;
    int done;

    test_baton_t *tb;
} handler_baton_t;

/* These defines are used with the test_baton_t result_flags variable. */
#define TEST_RESULT_SERVERCERTCB_CALLED      0x0001
#define TEST_RESULT_SERVERCERTCHAINCB_CALLED 0x0002
#define TEST_RESULT_CLIENT_CERTCB_CALLED     0x0004
#define TEST_RESULT_CLIENT_CERTPWCB_CALLED   0x0008
#define TEST_RESULT_AUTHNCB_CALLED           0x001A

apr_status_t
test_helper_run_requests_no_check(CuTest *tc, test_baton_t *tb,
                                  int num_requests,
                                  handler_baton_t handler_ctx[],
                                  apr_pool_t *pool);
void
test_helper_run_requests_expect_ok(CuTest *tc, test_baton_t *tb,
                                   int num_requests,
                                   handler_baton_t handler_ctx[],
                                   apr_pool_t *pool);
serf_bucket_t* accept_response(serf_request_t *request,
                               serf_bucket_t *stream,
                               void *acceptor_baton,
                               apr_pool_t *pool);
apr_status_t setup_request(serf_request_t *request,
                           void *setup_baton,
                           serf_bucket_t **req_bkt,
                           serf_response_acceptor_t *acceptor,
                           void **acceptor_baton,
                           serf_response_handler_t *handler,
                           void **handler_baton,
                           apr_pool_t *pool);
apr_status_t handle_response(serf_request_t *request,
                             serf_bucket_t *response,
                             void *handler_baton,
                             apr_pool_t *pool);
void setup_handler(test_baton_t *tb, handler_baton_t *handler_ctx,
                   const char *method, const char *path,
                   int req_id,
                   serf_response_handler_t handler);
void create_new_prio_request(test_baton_t *tb,
                             handler_baton_t *handler_ctx,
                             const char *method, const char *path,
                             int req_id);
void create_new_request(test_baton_t *tb,
                        handler_baton_t *handler_ctx,
                        const char *method, const char *path,
                        int req_id);
void
create_new_request_with_resp_hdlr(test_baton_t *tb,
                                  handler_baton_t *handler_ctx,
                                  const char *method, const char *path,
                                  int req_id,
                                  serf_response_handler_t handler);

/* Mock bucket type and constructor */
typedef struct {
    int times;
    const char *data;
    apr_status_t status;
} mockbkt_action;

void read_and_check_bucket(CuTest *tc, serf_bucket_t *bkt,
                           const char *expected);
void readlines_and_check_bucket(CuTest *tc, serf_bucket_t *bkt,
                                int acceptable,
                                const char *expected,
                                int expected_nr_of_lines);

extern const serf_bucket_type_t serf_bucket_type_mock;
#define SERF_BUCKET_IS_MOCK(b) SERF_BUCKET_CHECK((b), mock)

serf_bucket_t *serf_bucket_mock_create(mockbkt_action *actions,
                                       int len,
                                       serf_bucket_alloc_t *allocator);
apr_status_t serf_bucket_mock_more_data_arrived(serf_bucket_t *bucket);

/* Helper to get a file relative to our source directory by looking at
 * 'srcdir' env variable. */
const char * get_srcdir_file(apr_pool_t *pool, const char * file);

#endif /* TEST_SERF_H */
