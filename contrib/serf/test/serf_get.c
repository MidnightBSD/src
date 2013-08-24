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
    const char *hostinfo;
    int using_ssl;
    serf_ssl_context_t *ssl_ctx;
    serf_bucket_alloc_t *bkt_alloc;
} app_baton_t;

static void closed_connection(serf_connection_t *conn,
                              void *closed_baton,
                              apr_status_t why,
                              apr_pool_t *pool)
{
    if (why) {
        abort();
    }
}

static void print_ssl_cert_errors(int failures)
{
    if (failures) {
        fprintf(stderr, "INVALID CERTIFICATE:\n");
        if (failures & SERF_SSL_CERT_NOTYETVALID)
            fprintf(stderr, "* The certificate is not yet valid.\n");
        if (failures & SERF_SSL_CERT_EXPIRED)
            fprintf(stderr, "* The certificate expired.\n");
        if (failures & SERF_SSL_CERT_SELF_SIGNED)
            fprintf(stderr, "* The certificate is self-signed.\n");
        if (failures & SERF_SSL_CERT_UNKNOWNCA)
            fprintf(stderr, "* The CA is unknown.\n");
        if (failures & SERF_SSL_CERT_UNKNOWN_FAILURE)
            fprintf(stderr, "* Unknown failure.\n");
    }
}

static apr_status_t ignore_all_cert_errors(void *data, int failures,
                                           const serf_ssl_certificate_t *cert)
{
    print_ssl_cert_errors(failures);   

     /* In a real application, you would normally would not want to do this */
    return APR_SUCCESS;
}

static char *
convert_organisation_to_str(apr_hash_t *org, apr_pool_t *pool)
{
    return apr_psprintf(pool, "%s, %s, %s, %s, %s (%s)",
                        (char*)apr_hash_get(org, "OU", APR_HASH_KEY_STRING),
                        (char*)apr_hash_get(org, "O", APR_HASH_KEY_STRING),
                        (char*)apr_hash_get(org, "L", APR_HASH_KEY_STRING),
                        (char*)apr_hash_get(org, "ST", APR_HASH_KEY_STRING),
                        (char*)apr_hash_get(org, "C", APR_HASH_KEY_STRING),
                        (char*)apr_hash_get(org, "E", APR_HASH_KEY_STRING));
}

static apr_status_t print_certs(void *data, int failures, int error_depth,
                                const serf_ssl_certificate_t * const * certs,
                                apr_size_t certs_len)
{
    apr_pool_t *pool;
    const serf_ssl_certificate_t *current;

    apr_pool_create(&pool, NULL);

    fprintf(stderr, "Received certificate chain with length %d\n",
            (int)certs_len);
    print_ssl_cert_errors(failures);
    if (failures)
        fprintf(stderr, "Error at depth=%d\n", error_depth);
    else
        fprintf(stderr, "Chain provided with depth=%d\n", error_depth);

    while ((current = *certs) != NULL)
    {
        apr_hash_t *issuer, *subject, *serf_cert;
        apr_array_header_t *san;

        subject = serf_ssl_cert_subject(current, pool);
        issuer = serf_ssl_cert_issuer(current, pool);
        serf_cert = serf_ssl_cert_certificate(current, pool);
        
        fprintf(stderr, "\n-----BEGIN CERTIFICATE-----\n");
        fprintf(stderr, "Hostname: %s\n",
                (const char *)apr_hash_get(subject, "CN", APR_HASH_KEY_STRING));
        fprintf(stderr, "Sha1: %s\n",
                (const char *)apr_hash_get(serf_cert, "sha1", APR_HASH_KEY_STRING));
        fprintf(stderr, "Valid from: %s\n",
                (const char *)apr_hash_get(serf_cert, "notBefore", APR_HASH_KEY_STRING));
        fprintf(stderr, "Valid until: %s\n",
                (const char *)apr_hash_get(serf_cert, "notAfter", APR_HASH_KEY_STRING));
        fprintf(stderr, "Issuer: %s\n", convert_organisation_to_str(issuer, pool));

        san = apr_hash_get(serf_cert, "subjectAltName", APR_HASH_KEY_STRING);
        if (san) {
            int i;
            for (i = 0; i < san->nelts; i++) {
                char *s = APR_ARRAY_IDX(san, i, char*);
                fprintf(stderr, "SubjectAltName: %s\n", s);
            }
        }

        fprintf(stderr, "%s\n", serf_ssl_cert_export(current, pool));
        fprintf(stderr, "-----END CERTIFICATE-----\n");
        ++certs;
    }    

    apr_pool_destroy(pool);
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
        serf_ssl_server_cert_chain_callback_set(ctx->ssl_ctx, 
                                                ignore_all_cert_errors, 
                                                print_certs, NULL);
        serf_ssl_set_hostname(ctx->ssl_ctx, ctx->hostinfo);

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

typedef struct {
#if APR_MAJOR_VERSION > 0
    apr_uint32_t completed_requests;
#else
    apr_atomic_t completed_requests;
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

            apr_atomic_inc32(&ctx->completed_requests);
            return APR_EOF;
        }

        /* have we drained the response so far? */
        if (APR_STATUS_IS_EAGAIN(status))
            return status;

        /* loop to read some more. */
    }
    /* NOTREACHED */
}

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

    *req_bkt = serf_request_bucket_request_create(request, ctx->method,
                                                  ctx->path, body_bkt,
                                                  serf_request_get_alloc(request));

    hdrs_bkt = serf_bucket_request_get_headers(*req_bkt);

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
    puts("-p <hostname:port> Use the <host:port> as proxy server");
}

int main(int argc, const char **argv)
{
    apr_status_t status;
    apr_pool_t *pool;
    serf_context_t *context;
    serf_connection_t *connection;
    serf_request_t *request;
    app_baton_t app_ctx;
    handler_baton_t handler_ctx;
    apr_uri_t url;
    const char *proxy = NULL;
    const char *raw_url, *method, *req_body_path = NULL;
    int count;
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
    count = 1;
    /* Default to GET. */
    method = "GET";
    /* Do not print headers by default. */
    print_headers = 0;

    apr_getopt_init(&opt, pool, argc, argv);

    while ((status = apr_getopt(opt, "a:f:hHm:n:vp:", &opt_c, &opt_arg)) ==
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
            count = apr_strtoi64(opt_arg, NULL, 10);
            if (errno) {
                printf("Problem converting number of times to fetch URL (%d)\n",
                       errno);
                return errno;
            }
            break;
        case 'p':
            proxy = opt_arg;
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

    app_ctx.hostinfo = url.hostinfo;

    context = serf_context_create(pool);

    if (proxy)
    {
        apr_sockaddr_t *proxy_address = NULL;
        apr_port_t proxy_port;
        char *proxy_host;
        char *proxy_scope;

        status = apr_parse_addr_port(&proxy_host, &proxy_scope, &proxy_port, proxy, pool);
        if (status)
        {
            printf("Cannot parse proxy hostname/port: %d\n", status);
            apr_pool_destroy(pool);
            exit(1);
        }

        if (!proxy_host)
        {
            printf("Proxy hostname must be specified\n");
            apr_pool_destroy(pool);
            exit(1);
        }

        if (!proxy_port)
        {
            printf("Proxy port must be specified\n");
            apr_pool_destroy(pool);
            exit(1);
        }

        status = apr_sockaddr_info_get(&proxy_address, proxy_host, APR_UNSPEC,
                                       proxy_port, 0, pool);

        if (status)
        {
            printf("Cannot resolve proxy address '%s': %d\n", proxy_host, status);
            apr_pool_destroy(pool);
            exit(1);
        }

        serf_config_proxy(context, proxy_address);
    }

    /* ### Connection or Context should have an allocator? */
    app_ctx.bkt_alloc = serf_bucket_allocator_create(pool, NULL, NULL);
    app_ctx.ssl_ctx = NULL;

    status = serf_connection_create2(&connection, context, url,
                                     conn_setup, &app_ctx,
                                     closed_connection, &app_ctx,
                                     pool);
    if (status) {
        printf("Error creating connection: %d\n", status);
        apr_pool_destroy(pool);
        exit(1);
    }

    handler_ctx.completed_requests = 0;
    handler_ctx.print_headers = print_headers;

    handler_ctx.host = url.hostinfo;
    handler_ctx.method = method;
    handler_ctx.path = url.path;
    handler_ctx.authn = authn;

    handler_ctx.req_body_path = req_body_path;

    handler_ctx.acceptor = accept_response;
    handler_ctx.acceptor_baton = &app_ctx;
    handler_ctx.handler = handle_response;

    for (i = 0; i < count; i++) {
        request = serf_connection_request_create(connection, setup_request,
                                                 &handler_ctx);
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
        if (apr_atomic_read32(&handler_ctx.completed_requests) >= count) {
            break;
        }
        /* Debugging purposes only! */
        serf_debug__closed_conn(app_ctx.bkt_alloc);
    }

    serf_connection_close(connection);

    apr_pool_destroy(pool);
    return 0;
}
