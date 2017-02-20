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

#include <stdlib.h>

#include <apr.h>
#include <apr_uri.h>
#include <apr_strings.h>
#include <apr_atomic.h>
#include <apr_version.h>

#include "serf.h"

typedef struct {
    const char *resp_file;
    serf_bucket_t *bkt;
} accept_baton_t;

static serf_bucket_t* accept_response(void *acceptor_baton,
                                      serf_bucket_alloc_t *bkt_alloc,
                                      apr_pool_t *pool)
{
    accept_baton_t *ctx = acceptor_baton;
    serf_bucket_t *c;
    apr_file_t *file;
    apr_status_t status;

    status = apr_file_open(&file, ctx->resp_file,
                           APR_READ, APR_OS_DEFAULT, pool);
    if (status) {
        return NULL;
    }

    c = ctx->bkt = serf_bucket_file_create(file, bkt_alloc);

    c = serf_bucket_barrier_create(c, bkt_alloc);

    return serf_bucket_response_create(c, bkt_alloc);
}

typedef struct {
#if APR_MAJOR_VERSION > 0
    apr_uint32_t requests_outstanding;
#else
    apr_atomic_t requests_outstanding;
#endif
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
    const char *data, *s;
    apr_size_t len;
    serf_status_line sl;
    apr_status_t status;
    handler_baton_t *ctx = handler_baton;

    status = serf_bucket_response_status(response, &sl);
    if (status) {
        if (APR_STATUS_IS_EAGAIN(status)) {
            return APR_SUCCESS;
        }
        abort();
    }

    status = serf_bucket_read(response, 2048, &data, &len);

    if (!status || APR_STATUS_IS_EOF(status)) {
        if (len) {
            s = apr_pstrmemdup(pool, data, len);
            printf("%s", s);
        }
    }
    else if (APR_STATUS_IS_EAGAIN(status)) {
        status = APR_SUCCESS;
    }
    if (APR_STATUS_IS_EOF(status)) {
        serf_bucket_t *hdrs;
        const char *v;

        hdrs = serf_bucket_response_get_headers(response);
        v = serf_bucket_headers_get(hdrs, "Trailer-Test");
        if (v) {
            printf("Trailer-Test: %s\n", v);
        }

        apr_atomic_dec32(&ctx->requests_outstanding);
    }

    return status;
}

int main(int argc, const char **argv)
{
    apr_status_t status;
    apr_pool_t *pool;
    serf_bucket_t *resp_bkt;
    accept_baton_t accept_ctx;
    handler_baton_t handler_ctx;
    serf_bucket_alloc_t *allocator;

    if (argc != 2) {
        printf("%s: [Resp. File]\n", argv[0]);
        exit(-1);
    }
    accept_ctx.resp_file = argv[1];
    accept_ctx.bkt = NULL;

    apr_initialize();
    atexit(apr_terminate);

    apr_pool_create(&pool, NULL);
    apr_atomic_init(pool);
    /* serf_initialize(); */

    allocator = serf_bucket_allocator_create(pool, NULL, NULL);

    handler_ctx.requests_outstanding = 0;
    apr_atomic_inc32(&handler_ctx.requests_outstanding);

    resp_bkt = accept_response(&accept_ctx, allocator, pool);
    while (1) {
        status = handle_response(NULL, resp_bkt, &handler_ctx, pool);
        if (APR_STATUS_IS_TIMEUP(status))
            continue;
        if (SERF_BUCKET_READ_ERROR(status)) {
            printf("Error running context: %d\n", status);
            exit(1);
        }
        if (!apr_atomic_read32(&handler_ctx.requests_outstanding)) {
            break;
        }
    }
    serf_bucket_destroy(resp_bkt);
    serf_bucket_destroy(accept_ctx.bkt);

    apr_pool_destroy(pool);

    return 0;
}
