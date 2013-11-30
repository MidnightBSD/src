/* Copyright 2002-2004 Justin Erenkrantz and Greg Stein
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
#include <apr_uri.h>
#include <apr_strings.h>
#include <apr_atomic.h>
#include <apr_base64.h>
#include <apr_getopt.h>
#include <apr_version.h>

#include "serf.h"

typedef struct {
    int count;
    int using_ssl;
    serf_ssl_context_t *ssl_ctx;
    serf_bucket_alloc_t *bkt_alloc;
} app_baton_t;

typedef struct {
#if APR_MAJOR_VERSION > 0
    apr_uint32_t requests_outstanding;
#else
    apr_atomic_t requests_outstanding;
#endif
    int print_headers;

    serf_response_acceptor_t acceptor;
    app_baton_t *acceptor_baton;

    serf_response_handler_t handler;

    const char *host;
    const char *method;
    const char *path;
    const char *req_body_path;
    const char *authn;
} handler_baton_t;

/* Kludges for APR 0.9 support. */
#if APR_MAJOR_VERSION == 0
#define apr_atomic_inc32 apr_atomic_inc
#define apr_atomic_dec32 apr_atomic_dec
#define apr_atomic_read32 apr_atomic_read
#endif

static void closed_connection(serf_connection_t *conn,
                              void *closed_baton,
                              apr_status_t why,
                              apr_pool_t *pool)
{
    if (why) {
        abort();
    }
}

static apr_status_t ignore_all_cert_errors(void *data, int failures,
                                           const serf_ssl_certificate_t *cert)
{
    /* In a real application, you would normally would not want to do this */
    return APR_SUCCESS;
}

static apr_status_t conn_setup(apr_socket_t *skt,
                                serf_bucket_t **input_bkt,
                                serf_bucket_t **output_bkt,
                                void *setup_baton,
                                apr_pool_t *pool)
{
    serf_bucket_t *c;
    app_baton_t *ctx = setup_baton;

    c = serf_bucket_socket_create(skt, ctx->bkt_alloc);
    if (ctx->using_ssl) {
        c = serf_bucket_ssl_decrypt_create(c, ctx->ssl_ctx, ctx->bkt_alloc);
        if (!ctx->ssl_ctx) {
            ctx->ssl_ctx = serf_bucket_ssl_decrypt_context_get(c);
        }
        serf_ssl_server_cert_callback_set(ctx->ssl_ctx, ignore_all_cert_errors, NULL);

        *output_bkt = serf_bucket_ssl_encrypt_create(*output_bkt, ctx->ssl_ctx,
                                                    ctx->bkt_alloc);
    }

    *input_bkt = c;

    return APR_SUCCESS;
}

static serf_bucket_t* accept_response(serf_request_t *request,
                                      serf_bucket_t *stream,
                                      void *acceptor_baton,
                                      apr_pool_t *pool)
{
    serf_bucket_t *c;
    serf_bucket_alloc_t *bkt_alloc;

    /* get the per-request bucket allocator */
    bkt_alloc = serf_request_get_alloc(request);

    /* Create a barrier so the response doesn't eat us! */
    c = serf_bucket_barrier_create(stream, bkt_alloc);

    return serf_bucket_response_create(c, bkt_alloc);
}

static serf_bucket_t* accept_bwtp(serf_request_t *request,
                                  serf_bucket_t *stream,
                                  void *acceptor_baton,
                                  apr_pool_t *pool)
{
    serf_bucket_t *c;
    app_baton_t *app_ctx = acceptor_baton;

    /* Create a barrier so the response doesn't eat us! */
    c = serf_bucket_barrier_create(stream, app_ctx->bkt_alloc);

    return serf_bucket_bwtp_incoming_frame_create(c, app_ctx->bkt_alloc);
}

/* fwd declare */
static apr_status_t handle_bwtp_upgrade(serf_request_t *request,
                                        serf_bucket_t *response,
                                        void *handler_baton,
                                        apr_pool_t *pool);

static apr_status_t setup_request(serf_request_t *request,
                                  void *setup_baton,
                                  serf_bucket_t **req_bkt,
                                  serf_response_acceptor_t *acceptor,
                                  void **acceptor_baton,
                                  serf_response_handler_t *handler,
                                  void **handler_baton,
                                  apr_pool_t *pool)
{
    handler_baton_t *ctx = setup_baton;
    serf_bucket_t *hdrs_bkt;
    serf_bucket_t *body_bkt;

    if (ctx->req_body_path) {
        apr_file_t *file;
        apr_status_t status;

        status = apr_file_open(&file, ctx->req_body_path, APR_READ,
                               APR_OS_DEFAULT, pool);

        if (status) {
            printf("Error opening file (%s)\n", ctx->req_body_path);
            return status;
        }

        body_bkt = serf_bucket_file_create(file,
                                           serf_request_get_alloc(request));
    }
    else {
        body_bkt = NULL;
    }

    /*
    *req_bkt = serf_bucket_bwtp_message_create(0, body_bkt,
                                               serf_request_get_alloc(request));
    */

    *req_bkt = serf_bucket_bwtp_header_create(0, "MESSAGE",
                                              serf_request_get_alloc(request));

    hdrs_bkt = serf_bucket_bwtp_frame_get_headers(*req_bkt);

    /* FIXME: Shouldn't we be able to figure out the host ourselves? */
    serf_bucket_headers_setn(hdrs_bkt, "Host", ctx->host);
    serf_bucket_headers_setn(hdrs_bkt, "User-Agent",
                             "Serf/" SERF_VERSION_STRING);
    /* Shouldn't serf do this for us? */
    serf_bucket_headers_setn(hdrs_bkt, "Accept-Encoding", "gzip");

    if (ctx->authn != NULL) {
        serf_bucket_headers_setn(hdrs_bkt, "Authorization", ctx->authn);
    }

    *acceptor = ctx->acceptor;
    *acceptor_baton = ctx->acceptor_baton;
    *handler = ctx->handler;
    *handler_baton = ctx;

    return APR_SUCCESS;
}

static apr_status_t setup_bwtp_upgrade(serf_request_t *request,
                                       void *setup_baton,
                                       serf_bucket_t **req_bkt,
                                       serf_response_acceptor_t *acceptor,
                                       void **acceptor_baton,
                                       serf_response_handler_t *handler,
                                       void **handler_baton,
                                       apr_pool_t *pool)
{
    serf_bucket_t *hdrs_bkt;
    handler_baton_t *ctx = setup_baton;

    *req_bkt = serf_bucket_request_create("OPTIONS", "*", NULL,
                                          serf_request_get_alloc(request));

    hdrs_bkt = serf_bucket_request_get_headers(*req_bkt);

    serf_bucket_headers_setn(hdrs_bkt, "Upgrade", "BWTP/1.0");
    serf_bucket_headers_setn(hdrs_bkt, "Connection", "Upgrade");

    *acceptor = ctx->acceptor;
    *acceptor_baton = ctx->acceptor_baton;
    *handler = handle_bwtp_upgrade;
    *handler_baton = ctx;

    return APR_SUCCESS;
}

static apr_status_t setup_channel(serf_request_t *request,
                                  void *setup_baton,
                                  serf_bucket_t **req_bkt,
                                  serf_response_acceptor_t *acceptor,
                                  void **acceptor_baton,
                                  serf_response_handler_t *handler,
                                  void **handler_baton,
                                  apr_pool_t *pool)
{
    handler_baton_t *ctx = setup_baton;
    serf_bucket_t *hdrs_bkt;

    *req_bkt = serf_bucket_bwtp_channel_open(0, ctx->path,
                                             serf_request_get_alloc(request));

    hdrs_bkt = serf_bucket_bwtp_frame_get_headers(*req_bkt);

    /* FIXME: Shouldn't we be able to figure out the host ourselves? */
    serf_bucket_headers_setn(hdrs_bkt, "Host", ctx->host);
    serf_bucket_headers_setn(hdrs_bkt, "User-Agent",
                             "Serf/" SERF_VERSION_STRING);
    /* Shouldn't serf do this for us? */
    serf_bucket_headers_setn(hdrs_bkt, "Accept-Encoding", "gzip");

    if (ctx->authn != NULL) {
        serf_bucket_headers_setn(hdrs_bkt, "Authorization", ctx->authn);
    }

    *acceptor = ctx->acceptor;
    *acceptor_baton = ctx->acceptor_baton;
    *handler = ctx->handler;
    *handler_baton = ctx;

    return APR_SUCCESS;
}

static apr_status_t setup_close(serf_request_t *request,
                                void *setup_baton,
                                serf_bucket_t **req_bkt,
                                serf_response_acceptor_t *acceptor,
                                void **acceptor_baton,
                                serf_response_handler_t *handler,
                                void **handler_baton,
                                apr_pool_t *pool)
{
    handler_baton_t *ctx = setup_baton;

    *req_bkt = serf_bucket_bwtp_channel_close(0, serf_request_get_alloc(request));

    *acceptor = ctx->acceptor;
    *acceptor_baton = ctx->acceptor_baton;
    *handler = ctx->handler;
    *handler_baton = ctx;

    return APR_SUCCESS;
}

static apr_status_t handle_bwtp(serf_request_t *request,
                                serf_bucket_t *response,
                                void *handler_baton,
                                apr_pool_t *pool)
{
    const char *data;
    apr_size_t len;
    apr_status_t status;

    if (!response) {
        /* A NULL response can come back if the request failed completely */
        return APR_EGENERAL;
    }
    status = serf_bucket_bwtp_incoming_frame_wait_for_headers(response);
    if (SERF_BUCKET_READ_ERROR(status) || APR_STATUS_IS_EAGAIN(status)) {
        return status;
    }
    printf("BWTP %p frame: %d %d %s\n",
           response, serf_bucket_bwtp_frame_get_channel(response),
           serf_bucket_bwtp_frame_get_type(response),
           serf_bucket_bwtp_frame_get_phrase(response));


    while (1) {
        status = serf_bucket_read(response, 2048, &data, &len);
        if (SERF_BUCKET_READ_ERROR(status))
            return status;

        /* got some data. print it out. */
        if (len) {
            puts("BWTP body:\n---");
            fwrite(data, 1, len, stdout);
            puts("\n---");
        }

        /* are we done yet? */
        if (APR_STATUS_IS_EOF(status)) {
            return APR_EOF;
        }

        /* have we drained the response so far? */
        if (APR_STATUS_IS_EAGAIN(status))
            return status;

        /* loop to read some more. */
    }
    /* NOTREACHED */
}

static apr_status_t handle_bwtp_upgrade(serf_request_t *request,
                                        serf_bucket_t *response,
                                        void *handler_baton,
                                        apr_pool_t *pool)
{
    const char *data;
    apr_size_t len;
    serf_status_line sl;
    apr_status_t status;
    handler_baton_t *ctx = handler_baton;

    if (!response) {
        /* A NULL response can come back if the request failed completely */
        return APR_EGENERAL;
    }
    status = serf_bucket_response_status(response, &sl);
    if (status) {
        return status;
    }

    while (1) {
        status = serf_bucket_read(response, 2048, &data, &len);
        if (SERF_BUCKET_READ_ERROR(status))
            return status;

        /* got some data. print it out. */
        fwrite(data, 1, len, stdout);

        /* are we done yet? */
        if (APR_STATUS_IS_EOF(status)) {
            int i;
            serf_connection_t *conn;
            serf_request_t *new_req;

            conn = serf_request_get_conn(request);

            serf_connection_set_async_responses(conn,
                accept_bwtp, ctx->acceptor_baton, handle_bwtp, NULL);

            new_req = serf_connection_request_create(conn, setup_channel, ctx);
            for (i = 0; i < ctx->acceptor_baton->count; i++) {
                new_req = serf_connection_request_create(conn,
                                                         setup_request,
                                                         ctx);
            }

            return APR_EOF;
        }

        /* have we drained the response so far? */
        if (APR_STATUS_IS_EAGAIN(status))
            return status;

        /* loop to read some more. */
    }
    /* NOTREACHED */
}

static apr_status_t handle_response(serf_request_t *request,
                                    serf_bucket_t *response,
                                    void *handler_baton,
                                    apr_pool_t *pool)
{
    const char *data;
    apr_size_t len;
    serf_status_line sl;
    apr_status_t status;
    handler_baton_t *ctx = handler_baton;

    if (!response) {
        /* A NULL response can come back if the request failed completely */
        return APR_EGENERAL;
    }
    status = serf_bucket_response_status(response, &sl);
    if (status) {
        return status;
    }

    while (1) {
        status = serf_bucket_read(response, 2048, &data, &len);
        if (SERF_BUCKET_READ_ERROR(status))
            return status;

        /* got some data. print it out. */
        fwrite(data, 1, len, stdout);

        /* are we done yet? */
        if (APR_STATUS_IS_EOF(status)) {
            if (ctx->print_headers) {
                serf_bucket_t *hdrs;
                hdrs = serf_bucket_response_get_headers(response);
                while (1) {
                    status = serf_bucket_read(hdrs, 2048, &data, &len);
                    if (SERF_BUCKET_READ_ERROR(status))
                        return status;

                    fwrite(data, 1, len, stdout);
                    if (APR_STATUS_IS_EOF(status)) {
                        break;
                    }
                }
            }

            apr_atomic_dec32(&ctx->requests_outstanding);
            if (!ctx->requests_outstanding) {
                serf_connection_t *conn;
                serf_request_t *new_req;

                conn = serf_request_get_conn(request);
                new_req =
                    serf_connection_request_create(conn, setup_close, ctx);
            }
            return APR_EOF;
        }

        /* have we drained the response so far? */
        if (APR_STATUS_IS_EAGAIN(status))
            return status;

        /* loop to read some more. */
    }
    /* NOTREACHED */
}

static void print_usage(apr_pool_t *pool)
{
    puts("serf_get [options] URL");
    puts("-h\tDisplay this help");
    puts("-v\tDisplay version");
    puts("-H\tPrint response headers");
    puts("-n <count> Fetch URL <count> times");
    puts("-a <user:password> Present Basic authentication credentials");
    puts("-m <method> Use the <method> HTTP Method");
    puts("-f <file> Use the <file> as the request body");
}

int main(int argc, const char **argv)
{
    apr_status_t status;
    apr_pool_t *pool;
    apr_sockaddr_t *address;
    serf_context_t *context;
    serf_connection_t *connection;
    serf_request_t *request;
    app_baton_t app_ctx;
    handler_baton_t handler_ctx;
    apr_uri_t url;
    const char *raw_url, *method, *req_body_path = NULL;
    int i;
    int print_headers;
    char *authn = NULL;
    apr_getopt_t *opt;
    char opt_c;
    const char *opt_arg;

    apr_initialize();
    atexit(apr_terminate);

    apr_pool_create(&pool, NULL);
    /* serf_initialize(); */

    /* Default to one round of fetching. */
    app_ctx.count = 1;
    /* Default to GET. */
    method = "GET";
    /* Do not print headers by default. */
    print_headers = 0;

    apr_getopt_init(&opt, pool, argc, argv);

    while ((status = apr_getopt(opt, "a:f:hHm:n:v", &opt_c, &opt_arg)) ==
           APR_SUCCESS) {
        int srclen, enclen;

        switch (opt_c) {
        case 'a':
            srclen = strlen(opt_arg);
            enclen = apr_base64_encode_len(srclen);
            authn = apr_palloc(pool, enclen + 6);
            strcpy(authn, "Basic ");
            (void) apr_base64_encode(&authn[6], opt_arg, srclen);
            break;
        case 'f':
            req_body_path = opt_arg;
            break;
        case 'h':
            print_usage(pool);
            exit(0);
            break;
        case 'H':
            print_headers = 1;
            break;
        case 'm':
            method = opt_arg;
            break;
        case 'n':
            errno = 0;
            app_ctx.count = apr_strtoi64(opt_arg, NULL, 10);
            if (errno) {
                printf("Problem converting number of times to fetch URL (%d)\n",
                       errno);
                return errno;
            }
            break;
        case 'v':
            puts("Serf version: " SERF_VERSION_STRING);
            exit(0);
        default:
            break;
        }
    }

    if (opt->ind != opt->argc - 1) {
        print_usage(pool);
        exit(-1);
    }

    raw_url = argv[opt->ind];

    apr_uri_parse(pool, raw_url, &url);
    if (!url.port) {
        url.port = apr_uri_port_of_scheme(url.scheme);
    }
    if (!url.path) {
        url.path = "/";
    }

    if (strcasecmp(url.scheme, "https") == 0) {
        app_ctx.using_ssl = 1;
    }
    else {
        app_ctx.using_ssl = 0;
    }

    status = apr_sockaddr_info_get(&address,
                                   url.hostname, APR_UNSPEC, url.port, 0,
                                   pool);
    if (status) {
        printf("Error creating address: %d\n", status);
        apr_pool_destroy(pool);
        exit(1);
    }

    context = serf_context_create(pool);

    /* ### Connection or Context should have an allocator? */
    app_ctx.bkt_alloc = serf_bucket_allocator_create(pool, NULL, NULL);
    app_ctx.ssl_ctx = NULL;

    connection = serf_connection_create(context, address,
                                        conn_setup, &app_ctx,
                                        closed_connection, &app_ctx,
                                        pool);

    handler_ctx.requests_outstanding = 0;
    handler_ctx.print_headers = print_headers;

    handler_ctx.host = url.hostinfo;
    handler_ctx.method = method;
    handler_ctx.path = url.path;
    handler_ctx.authn = authn;

    handler_ctx.req_body_path = req_body_path;

    handler_ctx.acceptor = accept_response;
    handler_ctx.acceptor_baton = &app_ctx;
    handler_ctx.handler = handle_response;

    request = serf_connection_request_create(connection, setup_bwtp_upgrade,
                                             &handler_ctx);

    for (i = 0; i < app_ctx.count; i++) {
        apr_atomic_inc32(&handler_ctx.requests_outstanding);
    }

    while (1) {
        status = serf_context_run(context, SERF_DURATION_FOREVER, pool);
        if (APR_STATUS_IS_TIMEUP(status))
            continue;
        if (status) {
            char buf[200];

            printf("Error running context: (%d) %s\n", status,
                   apr_strerror(status, buf, sizeof(buf)));
            apr_pool_destroy(pool);
            exit(1);
        }
        if (!apr_atomic_read32(&handler_ctx.requests_outstanding)) {
            break;
        }
        /* Debugging purposes only! */
        serf_debug__closed_conn(app_ctx.bkt_alloc);
    }

    serf_connection_close(connection);

    apr_pool_destroy(pool);
    return 0;
}
