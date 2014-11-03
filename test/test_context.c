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

#include <stdlib.h>

#include <apr.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include <apr_version.h>

#include "serf.h"
#include "serf_private.h"

#include "test_serf.h"
#include "server/test_server.h"

/* Validate that requests are sent and completed in the order of creation. */
static void test_serf_connection_request_create(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[2];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    apr_status_t status;
    int i;
    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
        {CHUNKED_REQUEST(1, "2")},
    };

    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };

    apr_pool_t *test_pool = tc->testBaton;

    /* Set up a test context with a server */
    status = test_http_server_setup(&tb,
                                    message_list, num_requests,
                                    action_list, num_requests, 0, NULL,
                                    test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);
    create_new_request(tb, &handler_ctx[1], "GET", "/", 2);

    test_helper_run_requests_expect_ok(tc, tb, num_requests, handler_ctx,
                                       test_pool);

    /* Check that the requests were sent in the order we created them */
    for (i = 0; i < tb->sent_requests->nelts; i++) {
        int req_nr = APR_ARRAY_IDX(tb->sent_requests, i, int);
        CuAssertIntEquals(tc, i + 1, req_nr);
    }

    /* Check that the requests were received in the order we created them */
    for (i = 0; i < tb->handled_requests->nelts; i++) {
        int req_nr = APR_ARRAY_IDX(tb->handled_requests, i, int);
        CuAssertIntEquals(tc, i + 1, req_nr);
    }
}

/* Validate that priority requests are sent and completed before normal
   requests. */
static void test_serf_connection_priority_request_create(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[3];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    apr_status_t status;
    int i;

    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
        {CHUNKED_REQUEST(1, "2")},
        {CHUNKED_REQUEST(1, "3")},
    };

    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };

    apr_pool_t *test_pool = tc->testBaton;

    /* Set up a test context with a server */
    status = test_http_server_setup(&tb,
                                    message_list, num_requests,
                                    action_list, num_requests, 0, NULL,
                                    test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    create_new_request(tb, &handler_ctx[0], "GET", "/", 2);
    create_new_request(tb, &handler_ctx[1], "GET", "/", 3);
    create_new_prio_request(tb, &handler_ctx[2], "GET", "/", 1);

    test_helper_run_requests_expect_ok(tc, tb, num_requests, handler_ctx,
                                       test_pool);

    /* Check that the requests were sent in the order we created them */
    for (i = 0; i < tb->sent_requests->nelts; i++) {
        int req_nr = APR_ARRAY_IDX(tb->sent_requests, i, int);
        CuAssertIntEquals(tc, i + 1, req_nr);
    }

    /* Check that the requests were received in the order we created them */
    for (i = 0; i < tb->handled_requests->nelts; i++) {
        int req_nr = APR_ARRAY_IDX(tb->handled_requests, i, int);
        CuAssertIntEquals(tc, i + 1, req_nr);
    }
}

/* Test that serf correctly handles the 'Connection:close' header when the
   server is planning to close the connection. */
static void test_closed_connection(CuTest *tc)
{
    test_baton_t *tb;
    apr_status_t status;
    handler_baton_t handler_ctx[10];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    int done = FALSE, i;

    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
        {CHUNKED_REQUEST(1, "2")},
        {CHUNKED_REQUEST(1, "3")},
        {CHUNKED_REQUEST(1, "4")},
        {CHUNKED_REQUEST(1, "5")},
        {CHUNKED_REQUEST(1, "6")},
        {CHUNKED_REQUEST(1, "7")},
        {CHUNKED_REQUEST(1, "8")},
        {CHUNKED_REQUEST(1, "9")},
        {CHUNKED_REQUEST(2, "10")}
        };

    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND,
         "HTTP/1.1 200 OK" CRLF
         "Transfer-Encoding: chunked" CRLF
         "Connection: close" CRLF
         CRLF
         "0" CRLF
         CRLF
        },
        {SERVER_IGNORE_AND_KILL_CONNECTION},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND,
         "HTTP/1.1 200 OK" CRLF
         "Transfer-Encoding: chunked" CRLF
         "Connection: close" CRLF
         CRLF
         "0" CRLF
         CRLF
        },
        {SERVER_IGNORE_AND_KILL_CONNECTION},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };

    apr_pool_t *test_pool = tc->testBaton;

    /* Set up a test context with a server. */
    status = test_http_server_setup(&tb,
                                    message_list, num_requests,
                                    action_list, 12,
                                    0,
                                    NULL,
                                    test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    /* Send some requests on the connections */
    for (i = 0 ; i < num_requests ; i++) {
        create_new_request(tb, &handler_ctx[i], "GET", "/", i+1);
    }

    while (1) {
        status = run_test_server(tb->serv_ctx, 0, test_pool);
        if (APR_STATUS_IS_TIMEUP(status))
            status = APR_SUCCESS;
        CuAssertIntEquals(tc, APR_SUCCESS, status);

        status = serf_context_run(tb->context, 0, test_pool);
        if (APR_STATUS_IS_TIMEUP(status))
            status = APR_SUCCESS;
        CuAssertIntEquals(tc, APR_SUCCESS, status);

        /* Debugging purposes only! */
        serf_debug__closed_conn(tb->bkt_alloc);

        done = TRUE;
        for (i = 0 ; i < num_requests ; i++)
            if (handler_ctx[i].done == FALSE) {
                done = FALSE;
                break;
            }
        if (done)
            break;
    }

   /* Check that all requests were received */
   CuAssertTrue(tc, tb->sent_requests->nelts >= num_requests);
   CuAssertIntEquals(tc, num_requests, tb->accepted_requests->nelts);
   CuAssertIntEquals(tc, num_requests, tb->handled_requests->nelts);
}

/* Test if serf is sending the request to the proxy, not to the server
   directly. */
static void test_setup_proxy(CuTest *tc)
{
    test_baton_t *tb;
    int i;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    apr_pool_t *iter_pool;
    apr_status_t status;

    test_server_message_t message_list[] = {
        {"GET http://localhost:" SERV_PORT_STR " HTTP/1.1" CRLF\
         "Host: localhost:" SERV_PORT_STR CRLF\
         "Transfer-Encoding: chunked" CRLF\
         CRLF\
         "1" CRLF\
         "1" CRLF\
         "0" CRLF\
         CRLF}
    };

    test_server_action_t action_list_proxy[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };

    apr_pool_t *test_pool = tc->testBaton;

    /* Set up a test context with a server, no messages expected. */
    status = test_server_proxy_setup(&tb,
                                     /* server messages and actions */
                                     NULL, 0,
                                     NULL, 0,
                                     /* server messages and actions */
                                     message_list, 1,
                                     action_list_proxy, 1,
                                     0,
                                     NULL, test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    apr_pool_create(&iter_pool, test_pool);

    while (!handler_ctx[0].done)
    {
        apr_pool_clear(iter_pool);

        status = run_test_server(tb->serv_ctx, 0, iter_pool);
        if (APR_STATUS_IS_TIMEUP(status))
            status = APR_SUCCESS;
        CuAssertIntEquals(tc, APR_SUCCESS, status);

        status = run_test_server(tb->proxy_ctx, 0, iter_pool);
        if (APR_STATUS_IS_TIMEUP(status))
            status = APR_SUCCESS;
        CuAssertIntEquals(tc, APR_SUCCESS, status);

        status = serf_context_run(tb->context, 0, iter_pool);
        if (APR_STATUS_IS_TIMEUP(status))
            status = APR_SUCCESS;
        CuAssertIntEquals(tc, APR_SUCCESS, status);

        /* Debugging purposes only! */
        serf_debug__closed_conn(tb->bkt_alloc);
    }
    apr_pool_destroy(iter_pool);

    /* Check that all requests were received */
    CuAssertIntEquals(tc, num_requests, tb->sent_requests->nelts);
    CuAssertIntEquals(tc, num_requests, tb->accepted_requests->nelts);
    CuAssertIntEquals(tc, num_requests, tb->handled_requests->nelts);


    /* Check that the requests were sent in the order we created them */
    for (i = 0; i < tb->sent_requests->nelts; i++) {
        int req_nr = APR_ARRAY_IDX(tb->sent_requests, i, int);
        CuAssertIntEquals(tc, i + 1, req_nr);
    }

    /* Check that the requests were received in the order we created them */
    for (i = 0; i < tb->handled_requests->nelts; i++) {
        int req_nr = APR_ARRAY_IDX(tb->handled_requests, i, int);
        CuAssertIntEquals(tc, i + 1, req_nr);
    }
}

/*****************************************************************************
 * Test if we can make serf send requests one by one.
 *****************************************************************************/

/* Resend the first request 4 times by reducing the pipeline bandwidth to
   one request at a time, and by adding the first request again at the start of
   the outgoing queue. */
static apr_status_t
handle_response_keepalive_limit(serf_request_t *request,
                                serf_bucket_t *response,
                                void *handler_baton,
                                apr_pool_t *pool)
{
    handler_baton_t *ctx = handler_baton;

    if (! response) {
        return APR_SUCCESS;
    }

    while (1) {
        apr_status_t status;
        const char *data;
        apr_size_t len;

        status = serf_bucket_read(response, 2048, &data, &len);
        if (SERF_BUCKET_READ_ERROR(status)) {
            return status;
        }

        if (APR_STATUS_IS_EOF(status)) {
            APR_ARRAY_PUSH(ctx->handled_requests, int) = ctx->req_id;
            ctx->done = TRUE;
            if (ctx->req_id == 1 && ctx->handled_requests->nelts < 3) {
                serf_connection_priority_request_create(ctx->tb->connection,
                                                        setup_request,
                                                        ctx);
                ctx->done = FALSE;
            }
            return APR_EOF;
        }
    }

    return APR_SUCCESS;
}

#define SEND_REQUESTS 5
#define RCVD_REQUESTS 7
static void test_keepalive_limit_one_by_one(CuTest *tc)
{
    test_baton_t *tb;
    apr_status_t status;
    handler_baton_t handler_ctx[SEND_REQUESTS];
    int done = FALSE, i;

    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
        {CHUNKED_REQUEST(1, "1")},
        {CHUNKED_REQUEST(1, "1")},
        {CHUNKED_REQUEST(1, "2")},
        {CHUNKED_REQUEST(1, "3")},
        {CHUNKED_REQUEST(1, "4")},
        {CHUNKED_REQUEST(1, "5")},
    };

    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };

    apr_pool_t *test_pool = tc->testBaton;

    /* Set up a test context with a server. */
    status = test_http_server_setup(&tb,
                                    message_list, RCVD_REQUESTS,
                                    action_list, RCVD_REQUESTS, 0, NULL,
                                    test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    for (i = 0 ; i < SEND_REQUESTS ; i++) {
        create_new_request_with_resp_hdlr(tb, &handler_ctx[i], "GET", "/", i+1,
                                          handle_response_keepalive_limit);
        /* TODO: don't think this needs to be done in the loop. */
        serf_connection_set_max_outstanding_requests(tb->connection, 1);
    }

    while (1) {
        status = run_test_server(tb->serv_ctx, 0, test_pool);
        if (APR_STATUS_IS_TIMEUP(status))
            status = APR_SUCCESS;
        CuAssertIntEquals(tc, APR_SUCCESS, status);

        status = serf_context_run(tb->context, 0, test_pool);
        if (APR_STATUS_IS_TIMEUP(status))
            status = APR_SUCCESS;
        CuAssertIntEquals(tc, APR_SUCCESS, status);

        /* Debugging purposes only! */
        serf_debug__closed_conn(tb->bkt_alloc);

        done = TRUE;
        for (i = 0 ; i < SEND_REQUESTS ; i++)
            if (handler_ctx[i].done == FALSE) {
                done = FALSE;
                break;
            }
        if (done)
            break;
    }

    /* Check that all requests were received */
    CuAssertIntEquals(tc, RCVD_REQUESTS, tb->sent_requests->nelts);
    CuAssertIntEquals(tc, RCVD_REQUESTS, tb->accepted_requests->nelts);
    CuAssertIntEquals(tc, RCVD_REQUESTS, tb->handled_requests->nelts);
}
#undef SEND_REQUESTS
#undef RCVD_REQUESTS

/*****************************************************************************
 * Test if we can make serf first send requests one by one, and then change
 * back to burst mode.
 *****************************************************************************/
#define SEND_REQUESTS 5
#define RCVD_REQUESTS 7
/* Resend the first request 2 times by reducing the pipeline bandwidth to
   one request at a time, and by adding the first request again at the start of
   the outgoing queue. */
static apr_status_t
handle_response_keepalive_limit_burst(serf_request_t *request,
                                      serf_bucket_t *response,
                                      void *handler_baton,
                                      apr_pool_t *pool)
{
    handler_baton_t *ctx = handler_baton;

    if (! response) {
        return APR_SUCCESS;
    }

    while (1) {
        apr_status_t status;
        const char *data;
        apr_size_t len;

        status = serf_bucket_read(response, 2048, &data, &len);
        if (SERF_BUCKET_READ_ERROR(status)) {
            return status;
        }

        if (APR_STATUS_IS_EOF(status)) {
            APR_ARRAY_PUSH(ctx->handled_requests, int) = ctx->req_id;
            ctx->done = TRUE;
            if (ctx->req_id == 1 && ctx->handled_requests->nelts < 3) {
                serf_connection_priority_request_create(ctx->tb->connection,
                                                        setup_request,
                                                        ctx);
                ctx->done = FALSE;
            }
            else  {
                /* No more one-by-one. */
                serf_connection_set_max_outstanding_requests(ctx->tb->connection,
                                                             0);
            }
            return APR_EOF;
        }

        if (APR_STATUS_IS_EAGAIN(status)) {
            return status;
        }
    }

    return APR_SUCCESS;
}

static void test_keepalive_limit_one_by_one_and_burst(CuTest *tc)
{
    test_baton_t *tb;
        apr_status_t status;
    handler_baton_t handler_ctx[SEND_REQUESTS];
    int done = FALSE, i;

    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
        {CHUNKED_REQUEST(1, "1")},
        {CHUNKED_REQUEST(1, "1")},
        {CHUNKED_REQUEST(1, "2")},
        {CHUNKED_REQUEST(1, "3")},
        {CHUNKED_REQUEST(1, "4")},
        {CHUNKED_REQUEST(1, "5")},
    };

    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };

    apr_pool_t *test_pool = tc->testBaton;

    /* Set up a test context with a server. */
    status = test_http_server_setup(&tb,
                                    message_list, RCVD_REQUESTS,
                                    action_list, RCVD_REQUESTS, 0, NULL,
                                    test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    for (i = 0 ; i < SEND_REQUESTS ; i++) {
        create_new_request_with_resp_hdlr(tb, &handler_ctx[i], "GET", "/", i+1,
                                          handle_response_keepalive_limit_burst);
        serf_connection_set_max_outstanding_requests(tb->connection, 1);
    }

    while (1) {
        status = run_test_server(tb->serv_ctx, 0, test_pool);
        if (APR_STATUS_IS_TIMEUP(status))
            status = APR_SUCCESS;
        CuAssertIntEquals(tc, APR_SUCCESS, status);

        status = serf_context_run(tb->context, 0, test_pool);
        if (APR_STATUS_IS_TIMEUP(status))
            status = APR_SUCCESS;
        CuAssertIntEquals(tc, APR_SUCCESS, status);

        /* Debugging purposes only! */
        serf_debug__closed_conn(tb->bkt_alloc);

        done = TRUE;
        for (i = 0 ; i < SEND_REQUESTS ; i++)
            if (handler_ctx[i].done == FALSE) {
                done = FALSE;
                break;
            }
        if (done)
            break;
    }

    /* Check that all requests were received */
    CuAssertIntEquals(tc, RCVD_REQUESTS, tb->sent_requests->nelts);
    CuAssertIntEquals(tc, RCVD_REQUESTS, tb->accepted_requests->nelts);
    CuAssertIntEquals(tc, RCVD_REQUESTS, tb->handled_requests->nelts);
}
#undef SEND_REQUESTS
#undef RCVD_REQUESTS

typedef struct {
  apr_off_t read;
  apr_off_t written;
} progress_baton_t;

static void
progress_cb(void *progress_baton, apr_off_t read, apr_off_t written)
{
    test_baton_t *tb = progress_baton;
    progress_baton_t *pb = tb->user_baton;

    pb->read = read;
    pb->written = written;
}

static apr_status_t progress_conn_setup(apr_socket_t *skt,
                                          serf_bucket_t **input_bkt,
                                          serf_bucket_t **output_bkt,
                                          void *setup_baton,
                                          apr_pool_t *pool)
{
    test_baton_t *tb = setup_baton;
    *input_bkt = serf_context_bucket_socket_create(tb->context, skt, tb->bkt_alloc);
    return APR_SUCCESS;
}

static void test_progress_callback(CuTest *tc)
{
    test_baton_t *tb;
    apr_status_t status;
    handler_baton_t handler_ctx[5];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    int i;
    progress_baton_t *pb;

    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
        {CHUNKED_REQUEST(1, "2")},
        {CHUNKED_REQUEST(1, "3")},
        {CHUNKED_REQUEST(1, "4")},
        {CHUNKED_REQUEST(1, "5")},
    };

    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_RESPONSE(1, "2")},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };

    apr_pool_t *test_pool = tc->testBaton;
    
    /* Set up a test context with a server. */
    status = test_http_server_setup(&tb,
                                    message_list, num_requests,
                                    action_list, num_requests, 0,
                                    progress_conn_setup, test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    /* Set up the progress callback. */
    pb = apr_pcalloc(test_pool, sizeof(*pb));
    tb->user_baton = pb;
    serf_context_set_progress_cb(tb->context, progress_cb, tb);

    /* Send some requests on the connections */
    for (i = 0 ; i < num_requests ; i++) {
        create_new_request(tb, &handler_ctx[i], "GET", "/", i+1);
    }

    test_helper_run_requests_expect_ok(tc, tb, num_requests, handler_ctx,
                                       test_pool);

    /* Check that progress was reported. */
    CuAssertTrue(tc, pb->written > 0);
    CuAssertTrue(tc, pb->read > 0);
}

/* Test that username:password components in url are ignored. */
static void test_connection_userinfo_in_url(CuTest *tc)
{
    test_baton_t *tb;
    apr_status_t status;
    handler_baton_t handler_ctx[2];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    int i;
    progress_baton_t *pb;

    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
        {CHUNKED_REQUEST(1, "2")},
    };

    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        {SERVER_RESPOND, CHUNKED_RESPONSE(1, "2")},
    };

    apr_pool_t *test_pool = tc->testBaton;

    /* Set up a test context with a server. */
    status = test_http_server_setup(&tb,
                                    message_list, num_requests,
                                    action_list, num_requests, 0,
                                    progress_conn_setup, test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    /* Create a connection using user:password@hostname syntax */
    tb->serv_url = "http://user:password@localhost:" SERV_PORT_STR;
    use_new_connection(tb, test_pool);

    /* Send some requests on the connections */
    for (i = 0 ; i < num_requests ; i++) {
        create_new_request(tb, &handler_ctx[i], "GET", "/", i+1);
    }

    test_helper_run_requests_expect_ok(tc, tb, num_requests, handler_ctx,
                                       test_pool);
}

/*****************************************************************************
 * Issue #91: test that serf correctly handle an incoming 4xx reponse while
 * the outgoing request wasn't written completely yet.
 *****************************************************************************/

#define REQUEST_PART1 "PROPFIND / HTTP/1.1" CRLF\
"Host: lgo-ubuntu.local" CRLF\
"User-Agent: SVN/1.8.0-dev (x86_64-apple-darwin11.4.2) serf/2.0.0" CRLF\
"Content-Type: text/xml" CRLF\
"Transfer-Encoding: chunked" CRLF \
CRLF\
"12d" CRLF\
"<?xml version=""1.0"" encoding=""utf-8""?><propfind xmlns=""DAV:""><prop>"

#define REQUEST_PART2 \
"<resourcetype xmlns=""DAV:""/><getcontentlength xmlns=""DAV:""/>"\
"<deadprop-count xmlns=""http://subversion.tigris.org/xmlns/dav/""/>"\
"<version-name xmlns=""DAV:""/><creationdate xmlns=""DAV:""/>"\
"<creator-displayname xmlns=""DAV:""/></prop></propfind>" CRLF\
"0" CRLF \
CRLF

#define RESPONSE_408 "HTTP/1.1 408 Request Time-out" CRLF\
"Date: Wed, 14 Nov 2012 19:50:35 GMT" CRLF\
"Server: Apache/2.2.17 (Ubuntu)" CRLF\
"Vary: Accept-Encoding" CRLF\
"Content-Length: 305" CRLF\
"Connection: close" CRLF\
"Content-Type: text/html; charset=iso-8859-1" CRLF \
CRLF\
"<!DOCTYPE HTML PUBLIC ""-//IETF//DTD HTML 2.0//EN""><html><head>"\
"<title>408 Request Time-out</title></head><body><h1>Request Time-out</h1>"\
"<p>Server timeout waiting for the HTTP request from the client.</p><hr>"\
"<address>Apache/2.2.17 (Ubuntu) Server at lgo-ubuntu.local Port 80</address>"\
"</body></html>"


static apr_status_t detect_eof(void *baton, serf_bucket_t *aggregate_bucket)
{
    serf_bucket_t *body_bkt;
    handler_baton_t *ctx = baton;

    if (ctx->done) {
        body_bkt = serf_bucket_simple_create(REQUEST_PART1, strlen(REQUEST_PART2),
                                             NULL, NULL,
                                             ctx->tb->bkt_alloc);
        serf_bucket_aggregate_append(aggregate_bucket, body_bkt);
    }

    return APR_EAGAIN;
}

static apr_status_t setup_request_timeout(
                                  serf_request_t *request,
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

    *req_bkt = serf__bucket_stream_create(serf_request_get_alloc(request),
                                          detect_eof,
                                          ctx);

    /* create a simple body text */
    body_bkt = serf_bucket_simple_create(REQUEST_PART1, strlen(REQUEST_PART1),
                                         NULL, NULL,
                                         serf_request_get_alloc(request));
    serf_bucket_aggregate_append(*req_bkt, body_bkt);

    APR_ARRAY_PUSH(ctx->sent_requests, int) = ctx->req_id;

    *acceptor = ctx->acceptor;
    *acceptor_baton = ctx;
    *handler = ctx->handler;
    *handler_baton = ctx;

    return APR_SUCCESS;
}

static apr_status_t handle_response_timeout(
                                    serf_request_t *request,
                                    serf_bucket_t *response,
                                    void *handler_baton,
                                    apr_pool_t *pool)
{
    handler_baton_t *ctx = handler_baton;
    serf_status_line sl;
    apr_status_t status;

    if (! response) {
        serf_connection_request_create(ctx->tb->connection,
                                       setup_request,
                                       ctx);
        return APR_SUCCESS;
    }

    if (serf_request_is_written(request) != APR_EBUSY) {
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    }


    status = serf_bucket_response_status(response, &sl);
    if (SERF_BUCKET_READ_ERROR(status)) {
        return status;
    }
    if (!sl.version && (APR_STATUS_IS_EOF(status) ||
                        APR_STATUS_IS_EAGAIN(status))) {
        return status;
    }
    if (sl.code == 408) {
        APR_ARRAY_PUSH(ctx->handled_requests, int) = ctx->req_id;
        ctx->done = TRUE;
    }

    /* discard the rest of the body */
    while (1) {
        const char *data;
        apr_size_t len;

        status = serf_bucket_read(response, 2048, &data, &len);
        if (SERF_BUCKET_READ_ERROR(status) ||
            APR_STATUS_IS_EAGAIN(status) ||
            APR_STATUS_IS_EOF(status))
            return status;
    }

    return APR_SUCCESS;
}

static void test_request_timeout(CuTest *tc)
{
    test_baton_t *tb;
        apr_status_t status;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);

    test_server_message_t message_list[] = {
        {REQUEST_PART1},
        {REQUEST_PART2},
    };

    test_server_action_t action_list[] = {
        {SERVER_RESPOND, RESPONSE_408},
    };

    apr_pool_t *test_pool = tc->testBaton;

    /* Set up a test context with a server. */
    status = test_http_server_setup(&tb,
                                    message_list, 2,
                                    action_list, 1, 0,
                                    NULL, test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    /* Send some requests on the connection */
    handler_ctx[0].method = "PROPFIND";
    handler_ctx[0].path = "/";
    handler_ctx[0].done = FALSE;

    handler_ctx[0].acceptor = accept_response;
    handler_ctx[0].acceptor_baton = NULL;
    handler_ctx[0].handler = handle_response_timeout;
    handler_ctx[0].req_id = 1;
    handler_ctx[0].accepted_requests = tb->accepted_requests;
    handler_ctx[0].sent_requests = tb->sent_requests;
    handler_ctx[0].handled_requests = tb->handled_requests;
    handler_ctx[0].tb = tb;

    serf_connection_request_create(tb->connection,
                                   setup_request_timeout,
                                   &handler_ctx[0]);

    test_helper_run_requests_expect_ok(tc, tb, num_requests, handler_ctx,
                                       test_pool);
}

static const char *create_large_response_message(apr_pool_t *pool)
{
    const char *response = "HTTP/1.1 200 OK" CRLF
                     "Transfer-Encoding: chunked" CRLF
                     CRLF;
    struct iovec vecs[500];
    const int num_vecs = 500;
    int i, j;
    apr_size_t len;

    vecs[0].iov_base = (char *)response;
    vecs[0].iov_len = strlen(response);

    for (i = 1; i < num_vecs; i++)
    {
        int chunk_len = 10 * i * 3;
        char *chunk;
        char *buf;

        /* end with empty chunk */
        if (i == num_vecs - 1)
            chunk_len = 0;

        buf = apr_pcalloc(pool, chunk_len + 1);
        for (j = 0; j < chunk_len; j += 10)
            memcpy(buf + j, "0123456789", 10);

        chunk = apr_pstrcat(pool,
                            apr_psprintf(pool, "%x", chunk_len),
                            CRLF, buf, CRLF, NULL);
        vecs[i].iov_base = chunk;
        vecs[i].iov_len = strlen(chunk);
    }

    return apr_pstrcatv(pool, vecs, num_vecs, &len);
}

/* Validate reading a large chunked response. */
static void test_connection_large_response(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    apr_status_t status;
    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
    };
    test_server_action_t action_list[1];

    apr_pool_t *test_pool = tc->testBaton;

    /* create large chunked response message */
    const char *response = create_large_response_message(test_pool);
    action_list[0].kind = SERVER_RESPOND;
    action_list[0].text = response;

    /* Set up a test context with a server */
    status = test_http_server_setup(&tb,
                                    message_list, num_requests,
                                    action_list, num_requests, 0, NULL,
                                    test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    test_helper_run_requests_expect_ok(tc, tb, num_requests, handler_ctx,
                                       test_pool);
}

static const char *create_large_request_message(apr_pool_t *pool)
{
    const char *request = "GET / HTTP/1.1" CRLF
                          "Host: localhost:12345" CRLF
                          "Transfer-Encoding: chunked" CRLF
                          CRLF;
    struct iovec vecs[500];
    const int num_vecs = 500;
    int i, j;
    apr_size_t len;

    vecs[0].iov_base = (char *)request;
    vecs[0].iov_len = strlen(request);

    for (i = 1; i < num_vecs; i++)
    {
        int chunk_len = 10 * i * 3;
        char *chunk;
        char *buf;

        /* end with empty chunk */
        if (i == num_vecs - 1)
            chunk_len = 0;

        buf = apr_pcalloc(pool, chunk_len + 1);
        for (j = 0; j < chunk_len; j += 10)
            memcpy(buf + j, "0123456789", 10);

        chunk = apr_pstrcat(pool,
                            apr_psprintf(pool, "%x", chunk_len),
                            CRLF, buf, CRLF, NULL);
        vecs[i].iov_base = chunk;
        vecs[i].iov_len = strlen(chunk);
    }

    return apr_pstrcatv(pool, vecs, num_vecs, &len);
}

/* Validate sending a large chunked response. */
static void test_connection_large_request(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    test_server_message_t message_list[1];
    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };
    const char *request;
    apr_status_t status;

    apr_pool_t *test_pool = tc->testBaton;

    /* Set up a test context with a server */
    status = test_http_server_setup(&tb,
                                    message_list, num_requests,
                                    action_list, num_requests, 0, NULL,
                                    test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    /* create large chunked request message */
    request = create_large_request_message(test_pool);
    message_list[0].text = request;

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);
    handler_ctx[0].request = request;

    test_helper_run_requests_expect_ok(tc, tb, num_requests, handler_ctx,
                                       test_pool);
}

/*****************************************************************************
 * SSL handshake tests
 *****************************************************************************/
static const char *server_certs[] = {
    "test/server/serfservercert.pem",
    "test/server/serfcacert.pem",
    NULL };

static const char *all_server_certs[] = {
    "test/server/serfservercert.pem",
    "test/server/serfcacert.pem",
    "test/server/serfrootcacert.pem",
    NULL };

static apr_status_t validate_servercert(const serf_ssl_certificate_t *cert,
                                        apr_pool_t *pool)
{
    apr_hash_t *subject;
    subject = serf_ssl_cert_subject(cert, pool);
    if (strcmp("localhost",
               apr_hash_get(subject, "CN", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("Test Suite Server",
               apr_hash_get(subject, "OU", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("In Serf we trust, Inc.",
               apr_hash_get(subject, "O", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("Mechelen",
               apr_hash_get(subject, "L", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("Antwerp",
               apr_hash_get(subject, "ST", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("BE",
               apr_hash_get(subject, "C", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("serfserver@example.com",
               apr_hash_get(subject, "E", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;

    return APR_SUCCESS;
}

static apr_status_t validate_cacert(const serf_ssl_certificate_t *cert,
                                    apr_pool_t *pool)
{
    apr_hash_t *subject;
    subject = serf_ssl_cert_subject(cert, pool);
    if (strcmp("Serf CA",
               apr_hash_get(subject, "CN", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("Test Suite CA",
               apr_hash_get(subject, "OU", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("In Serf we trust, Inc.",
               apr_hash_get(subject, "O", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("Mechelen",
               apr_hash_get(subject, "L", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("Antwerp",
               apr_hash_get(subject, "ST", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("BE",
               apr_hash_get(subject, "C", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("serfca@example.com",
               apr_hash_get(subject, "E", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;

    return APR_SUCCESS;
}

static apr_status_t validate_rootcacert(const serf_ssl_certificate_t *cert,
                                        apr_pool_t *pool)
{
    apr_hash_t *subject;
    subject = serf_ssl_cert_subject(cert, pool);
    if (strcmp("Serf Root CA",
               apr_hash_get(subject, "CN", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("Test Suite Root CA",
               apr_hash_get(subject, "OU", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("In Serf we trust, Inc.",
               apr_hash_get(subject, "O", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("Mechelen",
               apr_hash_get(subject, "L", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("Antwerp",
               apr_hash_get(subject, "ST", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("BE",
               apr_hash_get(subject, "C", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("serfrootca@example.com",
               apr_hash_get(subject, "E", APR_HASH_KEY_STRING)) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;

    return APR_SUCCESS;
}

static apr_status_t
ssl_server_cert_cb_expect_failures(void *baton, int failures,
                                   const serf_ssl_certificate_t *cert)
{
    test_baton_t *tb = baton;
    int expected_failures = *(int *)tb->user_baton;

    tb->result_flags |= TEST_RESULT_SERVERCERTCB_CALLED;

    /* We expect an error from the certificate validation function. */
    if (failures & expected_failures)
        return APR_SUCCESS;
    else
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
}

static apr_status_t
ssl_server_cert_cb_expect_allok(void *baton, int failures,
                                const serf_ssl_certificate_t *cert)
{
    test_baton_t *tb = baton;
    tb->result_flags |= TEST_RESULT_SERVERCERTCB_CALLED;

    /* No error expected, certificate is valid. */
    if (failures)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    else
        return APR_SUCCESS;
}

static apr_status_t
ssl_server_cert_cb_reject(void *baton, int failures,
                          const serf_ssl_certificate_t *cert)
{
    return SERF_ERROR_ISSUE_IN_TESTSUITE;
}

/* Validate that we can connect successfully to an https server. This
   certificate is not trusted, so a cert validation failure is expected. */
static void test_ssl_handshake(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    int expected_failures;
    apr_status_t status;
    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
    };

    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };
    static const char *server_cert[] = { "test/server/serfservercert.pem",
        NULL };


    /* Set up a test context with a server */
    apr_pool_t *test_pool = tc->testBaton;

    status = test_https_server_setup(&tb,
                                     message_list, num_requests,
                                     action_list, num_requests, 0,
                                     NULL, /* default conn setup */
                                     "test/server/serfserverkey.pem",
                                     server_cert,
                                     NULL, /* no client cert */
                                     ssl_server_cert_cb_expect_failures,
                                     test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    /* This unknown failures is X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE, 
       meaning the chain has only the server cert. A good candidate for its
       own failure code. */
    expected_failures = SERF_SSL_CERT_UNKNOWNCA;
    tb->user_baton = &expected_failures;

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    test_helper_run_requests_expect_ok(tc, tb, num_requests, handler_ctx,
                                       test_pool);
}

/* Set up the ssl context with the CA and root CA certificates needed for
   successful valiation of the server certificate. */
static apr_status_t
https_set_root_ca_conn_setup(apr_socket_t *skt,
                             serf_bucket_t **input_bkt,
                             serf_bucket_t **output_bkt,
                             void *setup_baton,
                             apr_pool_t *pool)
{
    serf_ssl_certificate_t *rootcacert;
    test_baton_t *tb = setup_baton;
    apr_status_t status;

    status = default_https_conn_setup(skt, input_bkt, output_bkt,
                                      setup_baton, pool);
    if (status)
        return status;

    status = serf_ssl_load_cert_file(&rootcacert,
                                     "test/server/serfrootcacert.pem",
                                     pool);
    if (status)
        return status;
    status = serf_ssl_trust_cert(tb->ssl_context, rootcacert);
    if (status)
        return status;

    return status;
}

/* Validate that server certificate validation is ok when we
   explicitly trust our self-signed root ca. */
static void test_ssl_trust_rootca(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    apr_status_t status;
    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
    };

    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };

    /* Set up a test context with a server */
    apr_pool_t *test_pool = tc->testBaton;
    status = test_https_server_setup(&tb,
                                     message_list, num_requests,
                                     action_list, num_requests, 0,
                                     https_set_root_ca_conn_setup,
                                     "test/server/serfserverkey.pem",
                                     server_certs,
                                     NULL, /* no client cert */
                                     ssl_server_cert_cb_expect_allok,
                                     test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    test_helper_run_requests_expect_ok(tc, tb, num_requests, handler_ctx,
                                       test_pool);
}

/* Validate that when the application rejects the cert, the context loop
   bails out with an error. */
static void test_ssl_application_rejects_cert(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    apr_status_t status;
    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
    };

    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };

    /* Set up a test context with a server */
    apr_pool_t *test_pool = tc->testBaton;

    /* The certificate is valid, but we tell serf to reject it. */
    status = test_https_server_setup(&tb,
                                     message_list, num_requests,
                                     action_list, num_requests, 0,
                                     https_set_root_ca_conn_setup,
                                     "test/server/serfserverkey.pem",
                                     server_certs,
                                     NULL, /* no client cert */
                                     ssl_server_cert_cb_reject,
                                     test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    status = test_helper_run_requests_no_check(tc, tb, num_requests,
                                               handler_ctx, test_pool);
    /* We expect an error from the certificate validation function. */
    CuAssert(tc, "Application told serf the certificate should be rejected,"
                 " expected error!", status != APR_SUCCESS);
}

/* Test for ssl certificate chain callback. */
static apr_status_t
cert_chain_cb(void *baton,
              int failures,
              int error_depth,
              const serf_ssl_certificate_t * const * certs,
              apr_size_t certs_len)
{
    test_baton_t *tb = baton;
    apr_status_t status;

    tb->result_flags |= TEST_RESULT_SERVERCERTCHAINCB_CALLED;

    if (failures)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;

    if (certs_len != 3)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;

    status = validate_rootcacert(certs[2], tb->pool);
    if (status)
        return status;

    status = validate_cacert(certs[1], tb->pool);
    if (status)
        return status;

    status = validate_servercert(certs[0], tb->pool);
    if (status)
        return status;

    return APR_SUCCESS;
}

static apr_status_t
chain_rootca_callback_conn_setup(apr_socket_t *skt,
                                 serf_bucket_t **input_bkt,
                                 serf_bucket_t **output_bkt,
                                 void *setup_baton,
                                 apr_pool_t *pool)
{
    test_baton_t *tb = setup_baton;
    apr_status_t status;

    status = https_set_root_ca_conn_setup(skt, input_bkt, output_bkt,
                                          setup_baton, pool);
    if (status)
        return status;

    serf_ssl_server_cert_chain_callback_set(tb->ssl_context,
                                            ssl_server_cert_cb_expect_allok,
                                            cert_chain_cb,
                                            tb);

    return APR_SUCCESS;
}

/* Make the server return a partial certificate chain (server cert, CA cert),
   the root CA cert is trusted explicitly in the client. Test the chain
   callback. */
static void test_ssl_certificate_chain_with_anchor(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    apr_status_t status;
    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
    };

    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };

    /* Set up a test context with a server */
    apr_pool_t *test_pool = tc->testBaton;

    status = test_https_server_setup(&tb,
                                     message_list, num_requests,
                                     action_list, num_requests, 0,
                                     chain_rootca_callback_conn_setup,
                                     "test/server/serfserverkey.pem",
                                     server_certs,
                                     NULL, /* no client cert */
                                     ssl_server_cert_cb_expect_allok,
                                     test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    test_helper_run_requests_expect_ok(tc, tb, num_requests,
                                       handler_ctx, test_pool);

    CuAssertTrue(tc, tb->result_flags & TEST_RESULT_SERVERCERTCB_CALLED);
    CuAssertTrue(tc, tb->result_flags & TEST_RESULT_SERVERCERTCHAINCB_CALLED);
}

static apr_status_t
cert_chain_all_certs_cb(void *baton,
                        int failures,
                        int error_depth,
                        const serf_ssl_certificate_t * const * certs,
                        apr_size_t certs_len)
{
    /* Root CA cert is selfsigned, ignore this 'failure'. */
    failures &= ~SERF_SSL_CERT_SELF_SIGNED;

    return cert_chain_cb(baton, failures, error_depth, certs, certs_len);
}

static apr_status_t
chain_callback_conn_setup(apr_socket_t *skt,
                          serf_bucket_t **input_bkt,
                          serf_bucket_t **output_bkt,
                          void *setup_baton,
                          apr_pool_t *pool)
{
    test_baton_t *tb = setup_baton;
    apr_status_t status;

    status = default_https_conn_setup(skt, input_bkt, output_bkt,
                                      setup_baton, pool);
    if (status)
        return status;

    serf_ssl_server_cert_chain_callback_set(tb->ssl_context,
                                            ssl_server_cert_cb_expect_allok,
                                            cert_chain_all_certs_cb,
                                            tb);

    return APR_SUCCESS;
}

/* Make the server return the complete certificate chain (server cert, CA cert
   and root CA cert). Test the chain callback. */
static void test_ssl_certificate_chain_all_from_server(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    apr_status_t status;
    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
    };

    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };

    /* Set up a test context with a server */
    apr_pool_t *test_pool = tc->testBaton;

    status = test_https_server_setup(&tb,
                                     message_list, num_requests,
                                     action_list, num_requests, 0,
                                     chain_callback_conn_setup,
                                     "test/server/serfserverkey.pem",
                                     all_server_certs,
                                     NULL, /* no client cert */
                                     ssl_server_cert_cb_expect_allok,
                                     test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    test_helper_run_requests_expect_ok(tc, tb, num_requests,
                                       handler_ctx, test_pool);

    CuAssertTrue(tc, tb->result_flags & TEST_RESULT_SERVERCERTCB_CALLED);
    CuAssertTrue(tc, tb->result_flags & TEST_RESULT_SERVERCERTCHAINCB_CALLED);
}

/* Validate that the ssl handshake succeeds if no application callbacks
   are set, and the ssl server certificate chains is ok. */
static void test_ssl_no_servercert_callback_allok(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
    };
    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };
    apr_status_t status;

    /* Set up a test context with a server */
    apr_pool_t *test_pool = tc->testBaton;

    status = test_https_server_setup(&tb,
                                     message_list, num_requests,
                                     action_list, num_requests, 0,
                                     https_set_root_ca_conn_setup,
                                     "test/server/serfserverkey.pem",
                                     server_certs,
                                     NULL, /* no client cert */
                                     NULL, /* No server cert callback */
                                     test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    test_helper_run_requests_expect_ok(tc, tb, num_requests,
                                       handler_ctx, test_pool);
}

/* Validate that the ssl handshake fails if no application callbacks
 are set, and the ssl server certificate chains is NOT ok. */
static void test_ssl_no_servercert_callback_fail(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
    };
    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };
    apr_status_t status;

    /* Set up a test context with a server */
    apr_pool_t *test_pool = tc->testBaton;

    status = test_https_server_setup(&tb,
                                     message_list, num_requests,
                                     action_list, num_requests, 0,
                                     NULL, /* default conn setup, no certs */
                                     "test/server/serfserverkey.pem",
                                     server_certs,
                                     NULL, /* no client cert */
                                     NULL, /* No server cert callback */
                                     test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    status = test_helper_run_requests_no_check(tc, tb, num_requests,
                                               handler_ctx, test_pool);
    /* We expect an error from the certificate validation function. */
    CuAssertIntEquals(tc, SERF_ERROR_SSL_CERT_FAILED, status);
}

/* Similar to test_connection_large_response, validate reading a large
   chunked response over SSL. */
static void test_ssl_large_response(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
    };
    test_server_action_t action_list[1];
    apr_status_t status;

    /* Set up a test context with a server */
    apr_pool_t *test_pool = tc->testBaton;
    const char *response;

    status = test_https_server_setup(&tb,
                                     message_list, num_requests,
                                     action_list, num_requests, 0,
                                     https_set_root_ca_conn_setup,
                                     "test/server/serfserverkey.pem",
                                     server_certs,
                                     NULL, /* no client cert */
                                     NULL, /* No server cert callback */
                                     test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    /* create large chunked response message */
    response = create_large_response_message(test_pool);
    action_list[0].kind = SERVER_RESPOND;
    action_list[0].text = response;

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    test_helper_run_requests_expect_ok(tc, tb, num_requests,
                                       handler_ctx, test_pool);
}

/* Similar to test_connection_large_request, validate sending a large
   chunked request over SSL. */
static void test_ssl_large_request(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    test_server_message_t message_list[1];
    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };
    const char *request;
    apr_status_t status;

    /* Set up a test context with a server */
    apr_pool_t *test_pool = tc->testBaton;

    status = test_https_server_setup(&tb,
                                     message_list, num_requests,
                                     action_list, num_requests, 0,
                                     https_set_root_ca_conn_setup,
                                     "test/server/serfserverkey.pem",
                                     server_certs,
                                     NULL, /* no client cert */
                                     NULL, /* No server cert callback */
                                     test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    /* create large chunked request message */
    request = create_large_request_message(test_pool);
    message_list[0].text = request;

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);
    handler_ctx[0].request = request;

    test_helper_run_requests_expect_ok(tc, tb, num_requests,
                                       handler_ctx, test_pool);
}

static apr_status_t client_cert_cb(void *data, const char **cert_path)
{
    test_baton_t *tb = data;

    tb->result_flags |= TEST_RESULT_CLIENT_CERTCB_CALLED;

    *cert_path = "test/server/serfclientcert.p12";

    return APR_SUCCESS;
}

static apr_status_t client_cert_pw_cb(void *data,
                                      const char *cert_path,
                                      const char **password)
{
    test_baton_t *tb = data;

    tb->result_flags |= TEST_RESULT_CLIENT_CERTPWCB_CALLED;
    
    if (strcmp(cert_path, "test/server/serfclientcert.p12") == 0)
    {
        *password = "serftest";
        return APR_SUCCESS;
    }

    return SERF_ERROR_ISSUE_IN_TESTSUITE;
}

static apr_status_t
client_cert_conn_setup(apr_socket_t *skt,
                       serf_bucket_t **input_bkt,
                       serf_bucket_t **output_bkt,
                       void *setup_baton,
                       apr_pool_t *pool)
{
    test_baton_t *tb = setup_baton;
    apr_status_t status;

    status = https_set_root_ca_conn_setup(skt, input_bkt, output_bkt,
                                          setup_baton, pool);
    if (status)
        return status;

    serf_ssl_client_cert_provider_set(tb->ssl_context,
                                      client_cert_cb,
                                      tb,
                                      pool);

    serf_ssl_client_cert_password_set(tb->ssl_context,
                                      client_cert_pw_cb,
                                      tb,
                                      pool);

    return APR_SUCCESS;
}

static void test_ssl_client_certificate(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
    };
    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };
    apr_status_t status;

    /* Set up a test context with a server */
    apr_pool_t *test_pool = tc->testBaton;

    /* The SSL server the complete certificate chain to validate the client
       certificate. */
    status = test_https_server_setup(&tb,
                                     message_list, num_requests,
                                     action_list, num_requests, 0,
                                     client_cert_conn_setup,
                                     "test/server/serfserverkey.pem",
                                     all_server_certs,
                                     "Serf Client",
                                     NULL, /* No server cert callback */
                                     test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    test_helper_run_requests_expect_ok(tc, tb, num_requests,
                                       handler_ctx, test_pool);

    CuAssertTrue(tc, tb->result_flags & TEST_RESULT_CLIENT_CERTCB_CALLED);
    CuAssertTrue(tc, tb->result_flags & TEST_RESULT_CLIENT_CERTPWCB_CALLED);
}

/* Validate that the expired certificate is reported as failure in the
   callback. */
static void test_ssl_expired_server_cert(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    int expected_failures;
    apr_status_t status;
    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
    };

    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };
    static const char *expired_server_certs[] = {
        "test/server/serfserver_expired_cert.pem",
        "test/server/serfcacert.pem",
        "test/server/serfrootcacert.pem",
        NULL };

    /* Set up a test context with a server */
    apr_pool_t *test_pool = tc->testBaton;

    status = test_https_server_setup(&tb,
                                     message_list, num_requests,
                                     action_list, num_requests, 0,
                                     NULL, /* default conn setup */
                                     "test/server/serfserverkey.pem",
                                     expired_server_certs,
                                     NULL, /* no client cert */
                                     ssl_server_cert_cb_expect_failures,
                                     test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    expected_failures = SERF_SSL_CERT_SELF_SIGNED |
                        SERF_SSL_CERT_EXPIRED;
    tb->user_baton = &expected_failures;

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    test_helper_run_requests_expect_ok(tc, tb, num_requests, handler_ctx,
                                       test_pool);
}

/* Validate that the expired certificate is reported as failure in the
 callback. */
static void test_ssl_future_server_cert(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    int expected_failures;
    apr_status_t status;
    test_server_message_t message_list[] = {
        {CHUNKED_REQUEST(1, "1")},
    };

    test_server_action_t action_list[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };
    static const char *future_server_certs[] = {
        "test/server/serfserver_future_cert.pem",
        "test/server/serfcacert.pem",
        "test/server/serfrootcacert.pem",
        NULL };

    /* Set up a test context with a server */
    apr_pool_t *test_pool = tc->testBaton;

    status = test_https_server_setup(&tb,
                                     message_list, num_requests,
                                     action_list, num_requests, 0,
                                     NULL, /* default conn setup */
                                     "test/server/serfserverkey.pem",
                                     future_server_certs,
                                     NULL, /* no client cert */
                                     ssl_server_cert_cb_expect_failures,
                                     test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    expected_failures = SERF_SSL_CERT_SELF_SIGNED |
                        SERF_SSL_CERT_NOTYETVALID;
    tb->user_baton = &expected_failures;

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    test_helper_run_requests_expect_ok(tc, tb, num_requests, handler_ctx,
                                       test_pool);
}


/* Test if serf is sets up an SSL tunnel to the proxy and doesn't contact the
 https server directly. */
static void test_setup_ssltunnel(CuTest *tc)
{
    test_baton_t *tb;
    int i;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    apr_status_t status;

    /* TODO: issue 83: should be relative uri instead of absolute. */
    test_server_message_t message_list_server[] = {
        {"GET / HTTP/1.1" CRLF\
            "Host: localhost:" SERV_PORT_STR CRLF\
            "Transfer-Encoding: chunked" CRLF\
            CRLF\
            "1" CRLF\
            "1" CRLF\
            "0" CRLF\
            CRLF}
    };
    test_server_action_t action_list_server[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };
    test_server_message_t message_list_proxy[] = {
        {"CONNECT localhost:" SERV_PORT_STR " HTTP/1.1" CRLF\
         "Host: localhost:" SERV_PORT_STR CRLF\
         CRLF },
        { NULL }
    };
    test_server_action_t action_list_proxy[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        /* Forward the remainder of the data to the server without validation */
        {PROXY_FORWARD, "https://localhost:" SERV_PORT_STR},
    };

    apr_pool_t *test_pool = tc->testBaton;

    /* Set up a test context with a server and a proxy. Serf should send a
       CONNECT request to the server. */
    status = test_https_server_proxy_setup(&tb,
                                           /* server messages and actions */
                                           message_list_server, 1,
                                           action_list_server, 1,
                                           /* proxy messages and actions */
                                           message_list_proxy, 2,
                                           action_list_proxy, 2,
                                           0,
                                           https_set_root_ca_conn_setup,
                                           "test/server/serfserverkey.pem",
                                           server_certs,
                                           NULL, /* no client cert */
                                           NULL, /* No server cert callback */
                                           test_pool);
                                     
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    test_helper_run_requests_expect_ok(tc, tb, num_requests,
                                       handler_ctx, test_pool);

    /* Check that the requests were sent in the order we created them */
    for (i = 0; i < tb->sent_requests->nelts; i++) {
        int req_nr = APR_ARRAY_IDX(tb->sent_requests, i, int);
        CuAssertIntEquals(tc, i + 1, req_nr);
    }

    /* Check that the requests were received in the order we created them */
    for (i = 0; i < tb->handled_requests->nelts; i++) {
        int req_nr = APR_ARRAY_IDX(tb->handled_requests, i, int);
        CuAssertIntEquals(tc, i + 1, req_nr);
    }
}

/* Test error if no creds callback */
static void test_ssltunnel_no_creds_cb(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    apr_status_t status;

    test_server_message_t message_list_proxy[] = {
        {"CONNECT localhost:" SERV_PORT_STR " HTTP/1.1" CRLF\
            "Host: localhost:" SERV_PORT_STR CRLF\
            CRLF },
    };
    test_server_action_t action_list_proxy[] = {
        {SERVER_RESPOND, "HTTP/1.1 407 Unauthorized" CRLF
            "Transfer-Encoding: chunked" CRLF
            "Proxy-Authenticate: Basic realm=""Test Suite Proxy""" CRLF
            CRLF
            "1" CRLF CRLF
            "0" CRLF CRLF},
    };

    apr_pool_t *test_pool = tc->testBaton;

    /* Set up a test context with a server and a proxy. Serf should send a
     CONNECT request to the server. */
    status = test_https_server_proxy_setup(&tb,
                                           /* server messages and actions */
                                           NULL, 0,
                                           NULL, 0,
                                           /* proxy messages and actions */
                                           message_list_proxy, 1,
                                           action_list_proxy, 1,
                                           0,
                                           https_set_root_ca_conn_setup,
                                           "test/server/serfserverkey.pem",
                                           server_certs,
                                           NULL, /* no client cert */
                                           NULL, /* No server cert callback */
                                           test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    /* No credentials callback configured. */
    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    status = test_helper_run_requests_no_check(tc, tb, num_requests,
                                               handler_ctx, test_pool);
    CuAssertIntEquals(tc, SERF_ERROR_SSLTUNNEL_SETUP_FAILED, status);
}

static apr_status_t
ssltunnel_basic_authn_callback(char **username,
                               char **password,
                               serf_request_t *request, void *baton,
                               int code, const char *authn_type,
                               const char *realm,
                               apr_pool_t *pool)
{
    handler_baton_t *handler_ctx = baton;
    test_baton_t *tb = handler_ctx->tb;

    serf__log(TEST_VERBOSE, __FILE__, "ssltunnel_basic_authn_callback\n");

    tb->result_flags |= TEST_RESULT_AUTHNCB_CALLED;

    if (strcmp("Basic", authn_type) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;

    if (code == 401) {
        if (strcmp("<https://localhost:12345> Test Suite", realm) != 0)
            return SERF_ERROR_ISSUE_IN_TESTSUITE;

        *username = "serf";
        *password = "serftest";
    }
    else if (code == 407) {
        if (strcmp("<http://localhost:23456> Test Suite Proxy", realm) != 0)
            return SERF_ERROR_ISSUE_IN_TESTSUITE;

        *username = "serfproxy";
        *password = "serftest";
    } else
        return SERF_ERROR_ISSUE_IN_TESTSUITE;

    serf__log(TEST_VERBOSE, __FILE__, "ssltunnel_basic_authn_callback finished successfully.\n");

    return APR_SUCCESS;
}

/* Test if serf can successfully authenticate to a proxy used for an ssl
   tunnel. Retry the authentication a few times to test requeueing of the 
   CONNECT request. */
static void ssltunnel_basic_auth(CuTest *tc, const char *server_resp_hdrs,
                                 const char *proxy_407_resp_hdrs,
                                 const char *proxy_200_resp_hdrs)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    int num_requests_sent, num_requests_recvd;
    test_server_message_t message_list_server[2];
    test_server_action_t action_list_proxy[7];
    test_server_action_t action_list_server[2];
    apr_pool_t *test_pool = tc->testBaton;
    apr_status_t status;

    test_server_message_t message_list_proxy[] = {
        {"CONNECT localhost:" SERV_PORT_STR " HTTP/1.1" CRLF
            "Host: localhost:" SERV_PORT_STR CRLF
            CRLF },
        {"CONNECT localhost:" SERV_PORT_STR " HTTP/1.1" CRLF
            "Host: localhost:" SERV_PORT_STR CRLF
            "Proxy-Authorization: Basic c2VyZnByb3h5OnNlcmZ0ZXN0" CRLF
            CRLF },
        {"CONNECT localhost:" SERV_PORT_STR " HTTP/1.1" CRLF
            "Host: localhost:" SERV_PORT_STR CRLF
            "Proxy-Authorization: Basic c2VyZnByb3h5OnNlcmZ0ZXN0" CRLF
            CRLF },
        {"CONNECT localhost:" SERV_PORT_STR " HTTP/1.1" CRLF
            "Host: localhost:" SERV_PORT_STR CRLF
            "Proxy-Authorization: Basic c2VyZnByb3h5OnNlcmZ0ZXN0" CRLF
            CRLF },
        {"CONNECT localhost:" SERV_PORT_STR " HTTP/1.1" CRLF
            "Host: localhost:" SERV_PORT_STR CRLF
            "Proxy-Authorization: Basic c2VyZnByb3h5OnNlcmZ0ZXN0" CRLF
            CRLF },
    };

    action_list_proxy[0].kind = SERVER_RESPOND;
    action_list_proxy[0].text = apr_psprintf(test_pool,
        "HTTP/1.1 407 Unauthorized" CRLF
        "Transfer-Encoding: chunked" CRLF
        "Proxy-Authenticate: Basic realm=""Test Suite Proxy""" CRLF
        "%s"
        CRLF
        "1" CRLF CRLF
        "0" CRLF CRLF, proxy_407_resp_hdrs);
    action_list_proxy[1].kind = SERVER_RESPOND;
    action_list_proxy[1].text = apr_psprintf(test_pool,
        "HTTP/1.1 407 Unauthorized" CRLF
        "Transfer-Encoding: chunked" CRLF
        "Proxy-Authenticate: Basic realm=""Test Suite Proxy""" CRLF
        "%s"
        CRLF
        "1" CRLF CRLF
        "0" CRLF CRLF, proxy_407_resp_hdrs);

    action_list_proxy[2].kind = SERVER_RESPOND;
    action_list_proxy[2].text = apr_psprintf(test_pool,
        "HTTP/1.1 407 Unauthorized" CRLF
        "Transfer-Encoding: chunked" CRLF
        "Proxy-Authenticate: Basic realm=""Test Suite Proxy""" CRLF
        "%s"
        CRLF
        "1" CRLF CRLF
        "0" CRLF CRLF, proxy_407_resp_hdrs);

    action_list_proxy[3].kind = SERVER_RESPOND;
    action_list_proxy[3].text = apr_psprintf(test_pool,
        "HTTP/1.1 200 Connection Established" CRLF
        "%s"
        CRLF, proxy_200_resp_hdrs);
    /* Forward the remainder of the data to the server without validation */
    action_list_proxy[4].kind = PROXY_FORWARD;
    action_list_proxy[4].text = "https://localhost:" SERV_PORT_STR;
    /* If the client or the server closes the connection, stop forwarding.*/
    action_list_proxy[5].kind = SERVER_RESPOND;
    action_list_proxy[5].text = CHUNKED_EMPTY_RESPONSE;
    /* Again after disconnect. */
    action_list_proxy[6].kind = PROXY_FORWARD;
    action_list_proxy[6].text = "https://localhost:" SERV_PORT_STR;

    /* Make the server also require Basic authentication */
    message_list_server[0].text =
        "GET / HTTP/1.1" CRLF
        "Host: localhost:" SERV_PORT_STR CRLF
        "Transfer-Encoding: chunked" CRLF
        CRLF
        "1" CRLF
        "1" CRLF
        "0" CRLF
        CRLF;
    message_list_server[1].text =
        "GET / HTTP/1.1" CRLF
        "Host: localhost:" SERV_PORT_STR CRLF
        "Authorization: Basic c2VyZjpzZXJmdGVzdA==" CRLF
        "Transfer-Encoding: chunked" CRLF
        CRLF
        "1" CRLF
        "1" CRLF
        "0" CRLF
        CRLF;

    action_list_server[0].kind = SERVER_RESPOND;
    action_list_server[0].text = apr_psprintf(test_pool,
        "HTTP/1.1 401 Unauthorized" CRLF
        "Transfer-Encoding: chunked" CRLF
        "WWW-Authenticate: Basic realm=""Test Suite""" CRLF
        "%s"
        CRLF
        "1" CRLF CRLF
        "0" CRLF CRLF, server_resp_hdrs);
    action_list_server[1].kind = SERVER_RESPOND;
    action_list_server[1].text = CHUNKED_EMPTY_RESPONSE;

    /* Set up a test context with a server and a proxy. Serf should send a
       CONNECT request to the server. */
    status = test_https_server_proxy_setup(&tb,
                                           /* server messages and actions */
                                           message_list_server, 2,
                                           action_list_server, 2,
                                           /* proxy messages and actions */
                                           message_list_proxy, 5,
                                           action_list_proxy, 7,
                                           0,
                                           https_set_root_ca_conn_setup,
                                           "test/server/serfserverkey.pem",
                                           server_certs,
                                           NULL, /* no client cert */
                                           NULL, /* No server cert callback */
                                           test_pool);

    CuAssertIntEquals(tc, APR_SUCCESS, status);

    serf_config_authn_types(tb->context, SERF_AUTHN_BASIC);
    serf_config_credentials_callback(tb->context, ssltunnel_basic_authn_callback);

    create_new_request(tb, &handler_ctx[0], "GET", "/", 1);

    /* Test that a request is retried and authentication headers are set
     correctly. */
    num_requests_sent = 1;
    num_requests_recvd = 2;

    status = test_helper_run_requests_no_check(tc, tb, num_requests_sent,
                                               handler_ctx, test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);
    CuAssertIntEquals(tc, num_requests_recvd, tb->sent_requests->nelts);
    CuAssertIntEquals(tc, num_requests_recvd, tb->accepted_requests->nelts);
    CuAssertIntEquals(tc, num_requests_sent, tb->handled_requests->nelts);

    CuAssertTrue(tc, tb->result_flags & TEST_RESULT_AUTHNCB_CALLED);
}

static void test_ssltunnel_basic_auth(CuTest *tc)
{
    /* KeepAlive On for both proxy and server */
    ssltunnel_basic_auth(tc, "", "", "");
}

static void test_ssltunnel_basic_auth_server_has_keepalive_off(CuTest *tc)
{
    /* Add Connection:Close header to server response */
    ssltunnel_basic_auth(tc, "Connection: close" CRLF, "", "");
}

static void test_ssltunnel_basic_auth_proxy_has_keepalive_off(CuTest *tc)
{
    /* Add Connection:Close header to proxy 407 response */
    ssltunnel_basic_auth(tc, "", "Connection: close" CRLF, "");
}

static void test_ssltunnel_basic_auth_proxy_close_conn_on_200resp(CuTest *tc)
{
    /* Add Connection:Close header to proxy 200 Conn. Establ. response  */
    ssltunnel_basic_auth(tc, "", "", "Connection: close" CRLF);
}

static apr_status_t
proxy_digest_authn_callback(char **username,
                            char **password,
                            serf_request_t *request, void *baton,
                            int code, const char *authn_type,
                            const char *realm,
                            apr_pool_t *pool)
{
    handler_baton_t *handler_ctx = baton;
    test_baton_t *tb = handler_ctx->tb;

    tb->result_flags |= TEST_RESULT_AUTHNCB_CALLED;

    if (code != 407)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("Digest", authn_type) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;
    if (strcmp("<http://localhost:23456> Test Suite Proxy", realm) != 0)
        return SERF_ERROR_ISSUE_IN_TESTSUITE;

    *username = "serf";
    *password = "serftest";

    return APR_SUCCESS;
}

/* Test if serf can successfully authenticate to a proxy used for an ssl
   tunnel. */
static void test_ssltunnel_digest_auth(CuTest *tc)
{
    test_baton_t *tb;
    handler_baton_t handler_ctx[1];
    const int num_requests = sizeof(handler_ctx)/sizeof(handler_ctx[0]);
    apr_status_t status;

    test_server_message_t message_list_server[] = {
        {"GET /test/index.html HTTP/1.1" CRLF\
            "Host: localhost:" SERV_PORT_STR CRLF\
            "Transfer-Encoding: chunked" CRLF\
            CRLF\
            "1" CRLF\
            "1" CRLF\
            "0" CRLF\
            CRLF}
    };
    test_server_action_t action_list_server[] = {
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
    };

#define CONNECT_REQ\
    "CONNECT localhost:" SERV_PORT_STR " HTTP/1.1" CRLF\
    "Host: localhost:" SERV_PORT_STR CRLF

    test_server_message_t message_list_proxy[] = {
        { CONNECT_REQ CRLF },
        { CONNECT_REQ
            "Proxy-Authorization: Digest realm=\"Test Suite Proxy\", "
            "username=\"serf\", "
            "nonce=\"ABCDEF1234567890\", uri=\"localhost:" SERV_PORT_STR "\", "
            "response=\"15b1841822273b0fd44d2f6457f64213\", opaque=\"myopaque\", "
            "algorithm=\"MD5\"" CRLF
          CRLF },
    };
    /* Add a Basic header before Digest header, to test that serf uses the most
       secure authentication scheme first, instead of following the order of
       the headers. */
    /* Use non standard case for Proxy-Authenticate header to test case
       insensitivity for http headers. */
    test_server_action_t action_list_proxy[] = {
        {SERVER_RESPOND, "HTTP/1.1 407 Unauthorized" CRLF
            "Transfer-Encoding: chunked" CRLF
            "Proxy-Authenticate: Basic c2VyZjpzZXJmdGVzdA==" CRLF
            "Proxy-Authenticate: NonExistent blablablabla" CRLF
            "proXy-Authenticate: Digest realm=\"Test Suite Proxy\","
            "nonce=\"ABCDEF1234567890\",opaque=\"myopaque\","
            "algorithm=\"MD5\",qop-options=\"auth\"" CRLF
            CRLF
            "1" CRLF CRLF
            "0" CRLF CRLF},
        {SERVER_RESPOND, CHUNKED_EMPTY_RESPONSE},
        /* Forward the remainder of the data to the server without validation */
        {PROXY_FORWARD, "https://localhost:" SERV_PORT_STR},
    };

    apr_pool_t *test_pool = tc->testBaton;

    /* Set up a test context with a server and a proxy. Serf should send a
       CONNECT request to the server. */
    status = test_https_server_proxy_setup(&tb,
                                           /* server messages and actions */
                                           message_list_server, 1,
                                           action_list_server, 1,
                                           /* proxy messages and actions */
                                           message_list_proxy, 2,
                                           action_list_proxy, 3,
                                           0,
                                           https_set_root_ca_conn_setup,
                                           "test/server/serfserverkey.pem",
                                           server_certs,
                                           NULL, /* no client cert */
                                           NULL, /* No server cert callback */
                                           test_pool);

    CuAssertIntEquals(tc, APR_SUCCESS, status);

    serf_config_authn_types(tb->context, SERF_AUTHN_BASIC | SERF_AUTHN_DIGEST);
    serf_config_credentials_callback(tb->context, proxy_digest_authn_callback);

    create_new_request(tb, &handler_ctx[0], "GET", "/test/index.html", 1);
    test_helper_run_requests_expect_ok(tc, tb, num_requests,
                                       handler_ctx, test_pool);

    CuAssertTrue(tc, tb->result_flags & TEST_RESULT_AUTHNCB_CALLED);
}

/*****************************************************************************/
CuSuite *test_context(void)
{
    CuSuite *suite = CuSuiteNew();

    CuSuiteSetSetupTeardownCallbacks(suite, test_setup, test_teardown);

    SUITE_ADD_TEST(suite, test_serf_connection_request_create);
    SUITE_ADD_TEST(suite, test_serf_connection_priority_request_create);
    SUITE_ADD_TEST(suite, test_closed_connection);
    SUITE_ADD_TEST(suite, test_setup_proxy);
    SUITE_ADD_TEST(suite, test_keepalive_limit_one_by_one);
    SUITE_ADD_TEST(suite, test_keepalive_limit_one_by_one_and_burst);
    SUITE_ADD_TEST(suite, test_progress_callback);
    SUITE_ADD_TEST(suite, test_request_timeout);
    SUITE_ADD_TEST(suite, test_connection_large_response);
    SUITE_ADD_TEST(suite, test_connection_large_request);
    SUITE_ADD_TEST(suite, test_connection_userinfo_in_url);
    SUITE_ADD_TEST(suite, test_ssl_handshake);
    SUITE_ADD_TEST(suite, test_ssl_trust_rootca);
    SUITE_ADD_TEST(suite, test_ssl_application_rejects_cert);
    SUITE_ADD_TEST(suite, test_ssl_certificate_chain_with_anchor);
    SUITE_ADD_TEST(suite, test_ssl_certificate_chain_all_from_server);
    SUITE_ADD_TEST(suite, test_ssl_no_servercert_callback_allok);
    SUITE_ADD_TEST(suite, test_ssl_no_servercert_callback_fail);
    SUITE_ADD_TEST(suite, test_ssl_large_response);
    SUITE_ADD_TEST(suite, test_ssl_large_request);
    SUITE_ADD_TEST(suite, test_ssl_client_certificate);
    SUITE_ADD_TEST(suite, test_ssl_expired_server_cert);
    SUITE_ADD_TEST(suite, test_ssl_future_server_cert);
    SUITE_ADD_TEST(suite, test_setup_ssltunnel);
    SUITE_ADD_TEST(suite, test_ssltunnel_no_creds_cb);
    SUITE_ADD_TEST(suite, test_ssltunnel_basic_auth);
    SUITE_ADD_TEST(suite, test_ssltunnel_basic_auth_server_has_keepalive_off);
    SUITE_ADD_TEST(suite, test_ssltunnel_basic_auth_proxy_has_keepalive_off);
    SUITE_ADD_TEST(suite, test_ssltunnel_basic_auth_proxy_close_conn_on_200resp);
    SUITE_ADD_TEST(suite, test_ssltunnel_digest_auth);

    return suite;
}
