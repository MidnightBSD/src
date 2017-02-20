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

#include "serf.h"

static apr_status_t drain_bucket(serf_bucket_t *bucket)
{
    apr_status_t status;
    const char *data;
    apr_size_t len;

    while (1) {
        status = serf_bucket_read(bucket, 2048, &data, &len);
        if (SERF_BUCKET_READ_ERROR(status))
            return status;

        /* got some data. print it out. */
        fwrite(data, 1, len, stdout);

        /* are we done yet? */
        if (APR_STATUS_IS_EOF(status)) {
            return APR_EOF;
        }

        /* have we drained the response so far? */
        if (APR_STATUS_IS_EAGAIN(status))
            return APR_SUCCESS;

        /* loop to read some more. */
    }
    /* NOTREACHED */
}

int main(int argc, const char **argv)
{
    apr_pool_t *pool;
    serf_bucket_t *req_bkt;
    serf_bucket_t *hdrs_bkt;
    serf_bucket_alloc_t *allocator;

    apr_initialize();
    atexit(apr_terminate);

    apr_pool_create(&pool, NULL);
    /* serf_initialize(); */

    allocator = serf_bucket_allocator_create(pool, NULL, NULL);

    req_bkt = serf_bucket_request_create("GET", "/", NULL, allocator);

    hdrs_bkt = serf_bucket_request_get_headers(req_bkt);

    /* FIXME: Shouldn't we be able to figure out the host ourselves? */
    serf_bucket_headers_setn(hdrs_bkt, "Host", "localhost");
    serf_bucket_headers_setn(hdrs_bkt, "User-Agent",
                             "Serf/" SERF_VERSION_STRING);

    (void) drain_bucket(req_bkt);

    serf_bucket_destroy(req_bkt);

    apr_pool_destroy(pool);

    return 0;
}
