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

#include <apr.h>
#include <apr_pools.h>
#include <apr_strings.h>

#include "serf.h"
#include "test_serf.h"

/* test case has access to internal functions. */
#include "serf_private.h"

#define CRLF "\r\n"

static apr_status_t read_all(serf_bucket_t *bkt,
                             char *buf,
                             apr_size_t buf_len,
                             apr_size_t *read_len)
{
    const char *data;
    apr_size_t data_len;
    apr_status_t status;
    apr_size_t read;

    read = 0;

    do
    {
        status = serf_bucket_read(bkt, SERF_READ_ALL_AVAIL, &data, &data_len);

        if (!SERF_BUCKET_READ_ERROR(status))
        {
            if (data_len > buf_len - read)
            {
                /* Buffer is not large enough to read all data */
                data_len = buf_len - read;
                status = APR_EGENERAL;
            }
            memcpy(buf + read, data, data_len);
            read += data_len;
        }
    } while(status == APR_SUCCESS);

    *read_len = read;
    return status;
}

static void test_simple_bucket_readline(CuTest *tc)
{
    apr_status_t status;
    serf_bucket_t *bkt;
    const char *data;
    int found;
    apr_size_t len;

    apr_pool_t *test_pool = test_setup();
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);

    bkt = SERF_BUCKET_SIMPLE_STRING(
        "line1" CRLF
        "line2",
        alloc);

    /* Initialize parameters to check that they will be initialized. */
    len = 0x112233;
    data = 0;
    status = serf_bucket_readline(bkt, SERF_NEWLINE_CRLF, &found, &data, &len);

    CuAssertIntEquals(tc, APR_SUCCESS, status);
    CuAssertIntEquals(tc, SERF_NEWLINE_CRLF, found);
    CuAssertIntEquals(tc, 7, len);
    CuAssert(tc, data, strncmp("line1" CRLF, data, len) == 0);

    /* Initialize parameters to check that they will be initialized. */
    len = 0x112233;
    data = 0;
    status = serf_bucket_readline(bkt, SERF_NEWLINE_CRLF, &found, &data, &len);

    CuAssertIntEquals(tc, APR_EOF, status);
    CuAssertIntEquals(tc, SERF_NEWLINE_NONE, found);
    CuAssertIntEquals(tc, 5, len);
    CuAssert(tc, data, strncmp("line2", data, len) == 0);
    test_teardown(test_pool);
}

/* Reads bucket until EOF found and compares read data with zero terminated
   string expected. Report all failures using CuTest. */
static void read_and_check_bucket(CuTest *tc, serf_bucket_t *bkt,
                                  const char *expected)
{
    apr_status_t status;
    do
    {
        const char *data;
        apr_size_t len;

        status = serf_bucket_read(bkt, SERF_READ_ALL_AVAIL, &data, &len);
        CuAssert(tc, "Got error during bucket reading.",
                 !SERF_BUCKET_READ_ERROR(status));
        CuAssert(tc, "Read more data than expected.",
                 strlen(expected) >= len);
        CuAssert(tc, "Read data is not equal to expected.",
                 strncmp(expected, data, len) == 0);

        expected += len;
    } while(!APR_STATUS_IS_EOF(status));

    CuAssert(tc, "Read less data than expected.", strlen(expected) == 0);
}

static void test_response_bucket_read(CuTest *tc)
{
    serf_bucket_t *bkt, *tmp;

    apr_pool_t *test_pool = test_setup();
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);

    tmp = SERF_BUCKET_SIMPLE_STRING(
        "HTTP/1.1 200 OK" CRLF
        "Content-Length: 7" CRLF
        CRLF
        "abc1234",
        alloc);

    bkt = serf_bucket_response_create(tmp, alloc);

    /* Read all bucket and check it content. */
    read_and_check_bucket(tc, bkt, "abc1234");
    test_teardown(test_pool);
}

static void test_response_bucket_headers(CuTest *tc)
{
    serf_bucket_t *bkt, *tmp, *hdr;

    apr_pool_t *test_pool = test_setup();
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);

    tmp = SERF_BUCKET_SIMPLE_STRING(
        "HTTP/1.1 405 Method Not Allowed" CRLF
        "Date: Sat, 12 Jun 2010 14:17:10 GMT"  CRLF
        "Server: Apache"  CRLF
        "Allow: "  CRLF
        "Content-Length: 7"  CRLF
        "Content-Type: text/html; charset=iso-8859-1" CRLF
        "NoSpace:" CRLF
        CRLF
        "abc1234",
        alloc);

    bkt = serf_bucket_response_create(tmp, alloc);

    /* Read all bucket and check it content. */
    read_and_check_bucket(tc, bkt, "abc1234");

    hdr = serf_bucket_response_get_headers(bkt);
    CuAssertStrEquals(tc,
        "",
        serf_bucket_headers_get(hdr, "Allow"));
    CuAssertStrEquals(tc,
        "7",
        serf_bucket_headers_get(hdr, "Content-Length"));
    CuAssertStrEquals(tc,
        "",
        serf_bucket_headers_get(hdr, "NoSpace"));
    test_teardown(test_pool);
}

static void test_response_bucket_chunked_read(CuTest *tc)
{
    serf_bucket_t *bkt, *tmp, *hdrs;

    apr_pool_t *test_pool = test_setup();
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);

    tmp = SERF_BUCKET_SIMPLE_STRING(
        "HTTP/1.1 200 OK" CRLF
        "Transfer-Encoding: chunked" CRLF
        CRLF
        "3" CRLF
        "abc" CRLF
        "4" CRLF
        "1234" CRLF
        "0" CRLF
        "Footer: value" CRLF
        CRLF,
        alloc);

    bkt = serf_bucket_response_create(tmp, alloc);

    /* Read all bucket and check it content. */
    read_and_check_bucket(tc, bkt, "abc1234");

    hdrs = serf_bucket_response_get_headers(bkt);
    CuAssertTrue(tc, hdrs != NULL);

    /* Check that trailing headers parsed correctly. */
    CuAssertStrEquals(tc, "value", serf_bucket_headers_get(hdrs, "Footer"));
    test_teardown(test_pool);
}

static void test_bucket_header_set(CuTest *tc)
{
    apr_pool_t *test_pool = test_setup();
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);
    serf_bucket_t *hdrs = serf_bucket_headers_create(alloc);

    CuAssertTrue(tc, hdrs != NULL);

    serf_bucket_headers_set(hdrs, "Foo", "bar");

    CuAssertStrEquals(tc, "bar", serf_bucket_headers_get(hdrs, "Foo"));

    serf_bucket_headers_set(hdrs, "Foo", "baz");

    CuAssertStrEquals(tc, "bar,baz", serf_bucket_headers_get(hdrs, "Foo"));

    serf_bucket_headers_set(hdrs, "Foo", "test");

    CuAssertStrEquals(tc, "bar,baz,test", serf_bucket_headers_get(hdrs, "Foo"));

    /* headers are case insensitive. */
    CuAssertStrEquals(tc, "bar,baz,test", serf_bucket_headers_get(hdrs, "fOo"));
    test_teardown(test_pool);
}

static apr_status_t read_requested_bytes(serf_bucket_t *bkt,
                                         apr_size_t requested,
                                         const char **buf,
                                         apr_size_t *len,
                                         apr_pool_t *pool)
{
    apr_size_t current = 0;
    const char *tmp;
    const char *data;
    apr_status_t status = APR_SUCCESS;

    tmp = apr_pcalloc(pool, requested);
    while (current < requested) {
        status = serf_bucket_read(bkt, requested, &data, len);
        memcpy((void*)(tmp + current), (void*)data, *len);
        current += *len;
        if (APR_STATUS_IS_EOF(status))
            break;
    }

    *buf = tmp;
    *len = current;
    return status;
}


static void test_iovec_buckets(CuTest *tc)
{
    apr_status_t status;
    serf_bucket_t *bkt, *iobkt;
    const char *data;
    apr_size_t len;
    struct iovec vecs[32];
    struct iovec tgt_vecs[32];
    int i;
    int vecs_used;

    apr_pool_t *test_pool = test_setup();
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);

    /* Test 1: Read a single string in an iovec, store it in a iovec_bucket
       and then read it back. */
    bkt = SERF_BUCKET_SIMPLE_STRING(
        "line1" CRLF
        "line2",
        alloc);

    status = serf_bucket_read_iovec(bkt, SERF_READ_ALL_AVAIL, 32, vecs,
                                    &vecs_used);

    iobkt = serf_bucket_iovec_create(vecs, vecs_used, alloc);

    /* Check available data */
    status = serf_bucket_peek(iobkt, &data, &len);
    CuAssertIntEquals(tc, APR_EOF, status);
    CuAssertIntEquals(tc, strlen("line1" CRLF "line2"), len);

    /* Try to read only a few bytes (less than what's in the first buffer). */
    status = serf_bucket_read_iovec(iobkt, 3, 32, tgt_vecs, &vecs_used);
    CuAssertIntEquals(tc, APR_SUCCESS, status);
    CuAssertIntEquals(tc, 1, vecs_used);
    CuAssertIntEquals(tc, 3, tgt_vecs[0].iov_len);
    CuAssert(tc, tgt_vecs[0].iov_base,
             strncmp("lin", tgt_vecs[0].iov_base, tgt_vecs[0].iov_len) == 0);

    /* Read the rest of the data. */
    status = serf_bucket_read_iovec(iobkt, SERF_READ_ALL_AVAIL, 32, tgt_vecs,
                                    &vecs_used);
    CuAssertIntEquals(tc, APR_EOF, status);
    CuAssertIntEquals(tc, 1, vecs_used);
    CuAssertIntEquals(tc, strlen("e1" CRLF "line2"), tgt_vecs[0].iov_len);
    CuAssert(tc, tgt_vecs[0].iov_base,
             strncmp("e1" CRLF "line2", tgt_vecs[0].iov_base, tgt_vecs[0].iov_len - 3) == 0);

    /* Bucket should now be empty */
    status = serf_bucket_peek(iobkt, &data, &len);
    CuAssertIntEquals(tc, APR_EOF, status);
    CuAssertIntEquals(tc, 0, len);

    /* Test 2: Read multiple character bufs in an iovec, then read them back
       in bursts. */
    for (i = 0; i < 32 ; i++) {
        vecs[i].iov_base = apr_psprintf(test_pool, "data %02d 901234567890", i);
        vecs[i].iov_len = strlen(vecs[i].iov_base);
    }

    iobkt = serf_bucket_iovec_create(vecs, 32, alloc);

    /* Check that some data is in the buffer. Don't verify the actual data, the
       amount of data returned is not guaranteed to be the full buffer. */
    status = serf_bucket_peek(iobkt, &data, &len);
    CuAssertTrue(tc, len > 0);
    CuAssertIntEquals(tc, APR_SUCCESS, status); /* this assumes not all data is
                                                   returned at once,
                                                   not guaranteed! */

    /* Read 1 buf.   20 = sizeof("data %2d 901234567890") */
    status = serf_bucket_read_iovec(iobkt, 1 * 20, 32,
                                    tgt_vecs, &vecs_used);
    CuAssertIntEquals(tc, APR_SUCCESS, status);
    CuAssertIntEquals(tc, 1, vecs_used);
    CuAssert(tc, tgt_vecs[0].iov_base,
             strncmp("data 00 901234567890", tgt_vecs[0].iov_base, tgt_vecs[0].iov_len) == 0);

    /* Read 2 bufs. */
    status = serf_bucket_read_iovec(iobkt, 2 * 20, 32,
                                    tgt_vecs, &vecs_used);
    CuAssertIntEquals(tc, APR_SUCCESS, status);
    CuAssertIntEquals(tc, 2, vecs_used);

    /* Read the remaining 29 bufs. */
    vecs_used = 400;  /* test if iovec code correctly resets vecs_used */
    status = serf_bucket_read_iovec(iobkt, SERF_READ_ALL_AVAIL, 32,
                                    tgt_vecs, &vecs_used);
    CuAssertIntEquals(tc, APR_EOF, status);
    CuAssertIntEquals(tc, 29, vecs_used);

    /* Test 3: use serf_bucket_read */
    for (i = 0; i < 32 ; i++) {
        vecs[i].iov_base = apr_psprintf(test_pool, "DATA %02d 901234567890", i);
        vecs[i].iov_len = strlen(vecs[i].iov_base);
    }

    iobkt = serf_bucket_iovec_create(vecs, 32, alloc);

    status = serf_bucket_read(iobkt, 10, &data, &len);
    CuAssertIntEquals(tc, APR_SUCCESS, status);
    CuAssertIntEquals(tc, 10, len);
    CuAssert(tc, data,
             strncmp("DATA 00 90", data, len) == 0);

    status = serf_bucket_read(iobkt, 10, &data, &len);
    CuAssertIntEquals(tc, APR_SUCCESS, status);
    CuAssertIntEquals(tc, 10, len);
    CuAssert(tc, tgt_vecs[0].iov_base,
             strncmp("1234567890", data, len) == 0);

    for (i = 1; i < 31 ; i++) {
        const char *exp = apr_psprintf(test_pool, "DATA %02d 901234567890", i);
        status = serf_bucket_read(iobkt, SERF_READ_ALL_AVAIL, &data, &len);
        CuAssertIntEquals(tc, APR_SUCCESS, status);
        CuAssertIntEquals(tc, 20, len);
        CuAssert(tc, data,
                 strncmp(exp, data, len) == 0);

    }

    status = serf_bucket_read(iobkt, 20, &data, &len);
    CuAssertIntEquals(tc, APR_EOF, status);
    CuAssertIntEquals(tc, 20, len);
    CuAssert(tc, data,
             strncmp("DATA 31 901234567890", data, len) == 0);

    /* Test 3: read an empty iovec */
    iobkt = serf_bucket_iovec_create(vecs, 0, alloc);
    status = serf_bucket_read_iovec(iobkt, SERF_READ_ALL_AVAIL, 32,
                                    tgt_vecs, &vecs_used);
    CuAssertIntEquals(tc, APR_EOF, status);
    CuAssertIntEquals(tc, 0, vecs_used);

    status = serf_bucket_read(iobkt, SERF_READ_ALL_AVAIL, &data, &len);
    CuAssertIntEquals(tc, APR_EOF, status);
    CuAssertIntEquals(tc, 0, len);

    /* Test 4: read 0 bytes from an iovec */
    bkt = SERF_BUCKET_SIMPLE_STRING("line1" CRLF, alloc);
    status = serf_bucket_read_iovec(bkt, SERF_READ_ALL_AVAIL, 32, vecs,
                                    &vecs_used);
    iobkt = serf_bucket_iovec_create(vecs, vecs_used, alloc);
    status = serf_bucket_read_iovec(iobkt, 0, 32,
                                    tgt_vecs, &vecs_used);
    CuAssertIntEquals(tc, APR_SUCCESS, status);
    CuAssertIntEquals(tc, 0, vecs_used);

    test_teardown(test_pool);
}

/* Construct a header bucket with some headers, and then read from it. */
static void test_header_buckets(CuTest *tc)
{
    apr_status_t status;
    apr_pool_t *test_pool = test_setup();
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);
    const char *cur;

    serf_bucket_t *hdrs = serf_bucket_headers_create(alloc);
    CuAssertTrue(tc, hdrs != NULL);

    serf_bucket_headers_set(hdrs, "Content-Type", "text/plain");
    serf_bucket_headers_set(hdrs, "Content-Length", "100");

    /* Note: order not guaranteed, assume here that it's fifo. */
    cur = "Content-Type: text/plain" CRLF
          "Content-Length: 100" CRLF
          CRLF
          CRLF;
    while (1) {
        const char *data;
        apr_size_t len;

        status = serf_bucket_read(hdrs, SERF_READ_ALL_AVAIL, &data, &len);
        CuAssert(tc, "Unexpected error when waiting for response headers",
                 !SERF_BUCKET_READ_ERROR(status));
        if (SERF_BUCKET_READ_ERROR(status) ||
            APR_STATUS_IS_EOF(status))
            break;

        /* Check that the bytes read match with expected at current position. */
        CuAssertStrnEquals(tc, cur, len, data);
        cur += len;
    }
    CuAssertIntEquals(tc, APR_EOF, status);
}

static void test_aggregate_buckets(CuTest *tc)
{
    apr_status_t status;
    serf_bucket_t *bkt, *aggbkt;
    struct iovec tgt_vecs[32];
    int vecs_used;

    apr_pool_t *test_pool = test_setup();
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);

    /* Test 1: read 0 bytes from an aggregate */
    aggbkt = serf_bucket_aggregate_create(alloc);

    bkt = SERF_BUCKET_SIMPLE_STRING("line1" CRLF, alloc);
    serf_bucket_aggregate_append(aggbkt, bkt);

    status = serf_bucket_read_iovec(aggbkt, 0, 32,
                                    tgt_vecs, &vecs_used);
    CuAssertIntEquals(tc, APR_SUCCESS, status);
    CuAssertIntEquals(tc, 0, vecs_used);

    test_teardown(test_pool);
}

/* Test for issue: the server aborts the connection in the middle of
   streaming the body of the response, where the length was set with the
   Content-Length header. Test that we get a decent error code from the
   response bucket instead of APR_EOF. */
static void test_response_body_too_small_cl(CuTest *tc)
{
    serf_bucket_t *bkt, *tmp;
    apr_pool_t *test_pool = test_setup();
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);

    /* Make a response of 60 bytes, but set the Content-Length to 100. */
#define BODY "12345678901234567890"\
             "12345678901234567890"\
             "12345678901234567890"

    tmp = SERF_BUCKET_SIMPLE_STRING("HTTP/1.1 200 OK" CRLF
                                    "Content-Type: text/plain" CRLF
                                    "Content-Length: 100" CRLF
                                    CRLF
                                    BODY,
                                    alloc);
    
    bkt = serf_bucket_response_create(tmp, alloc);

    {
        const char *data;
        apr_size_t len;
        apr_status_t status;

        status = serf_bucket_read(bkt, SERF_READ_ALL_AVAIL, &data, &len);

        CuAssert(tc, "Read more data than expected.",
                 strlen(BODY) >= len);
        CuAssert(tc, "Read data is not equal to expected.",
                 strncmp(BODY, data, len) == 0);
        CuAssert(tc, "Error expected due to response body too short!",
                 SERF_BUCKET_READ_ERROR(status));
        CuAssertIntEquals(tc, SERF_ERROR_TRUNCATED_HTTP_RESPONSE, status);
    }
}
#undef BODY

/* Test for issue: the server aborts the connection in the middle of
   streaming the body of the response, using chunked encoding. Test that we get
   a decent error code from the response bucket instead of APR_EOF. */
static void test_response_body_too_small_chunked(CuTest *tc)
{
    serf_bucket_t *bkt, *tmp;
    apr_pool_t *test_pool = test_setup();
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);

    /* Make a response of 60 bytes, but set the chunk size to 60 and don't end
       with chunk of length 0. */
#define BODY "12345678901234567890"\
"12345678901234567890"\
"12345678901234567890"

    tmp = SERF_BUCKET_SIMPLE_STRING("HTTP/1.1 200 OK" CRLF
                                    "Content-Type: text/plain" CRLF
                                    "Transfer-Encoding: chunked" CRLF
                                    CRLF
                                    "64" CRLF BODY,
                                    alloc);

    bkt = serf_bucket_response_create(tmp, alloc);

    {
        const char *data;
        apr_size_t len;
        apr_status_t status;

        status = serf_bucket_read(bkt, SERF_READ_ALL_AVAIL, &data, &len);

        CuAssert(tc, "Read more data than expected.",
                 strlen(BODY) >= len);
        CuAssert(tc, "Read data is not equal to expected.",
                 strncmp(BODY, data, len) == 0);
        CuAssert(tc, "Error expected due to response body too short!",
                 SERF_BUCKET_READ_ERROR(status));
        CuAssertIntEquals(tc, SERF_ERROR_TRUNCATED_HTTP_RESPONSE, status);
    }
}
#undef BODY

/* Test for issue: the server aborts the connection in the middle of
   streaming trailing CRLF after body chunk. Test that we get
   a decent error code from the response bucket instead of APR_EOF. */
static void test_response_body_chunked_no_crlf(CuTest *tc)
{
    serf_bucket_t *bkt, *tmp;
    apr_pool_t *test_pool = test_setup();
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);

    tmp = SERF_BUCKET_SIMPLE_STRING("HTTP/1.1 200 OK" CRLF
                                    "Content-Type: text/plain" CRLF
                                    "Transfer-Encoding: chunked" CRLF
                                    CRLF
                                    "2" CRLF
                                    "AB",
                                    alloc);

    bkt = serf_bucket_response_create(tmp, alloc);

    {
        char buf[1024];
        apr_size_t len;
        apr_status_t status;

        status = read_all(bkt, buf, sizeof(buf), &len);

        CuAssertIntEquals(tc, SERF_ERROR_TRUNCATED_HTTP_RESPONSE, status);
    }
    test_teardown(test_pool);
}

/* Test for issue: the server aborts the connection in the middle of
   streaming trailing CRLF after body chunk. Test that we get
   a decent error code from the response bucket instead of APR_EOF. */
static void test_response_body_chunked_incomplete_crlf(CuTest *tc)
{
    serf_bucket_t *bkt, *tmp;
    apr_pool_t *test_pool = test_setup();
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);

    tmp = SERF_BUCKET_SIMPLE_STRING("HTTP/1.1 200 OK" CRLF
                                    "Content-Type: text/plain" CRLF
                                    "Transfer-Encoding: chunked" CRLF
                                    CRLF
                                    "2" CRLF
                                    "AB"
                                    "\r",
                                    alloc);

    bkt = serf_bucket_response_create(tmp, alloc);

    {
        char buf[1024];
        apr_size_t len;
        apr_status_t status;

        status = read_all(bkt, buf, sizeof(buf), &len);

        CuAssertIntEquals(tc, SERF_ERROR_TRUNCATED_HTTP_RESPONSE, status);
    }
    test_teardown(test_pool);
}

static void test_response_body_chunked_gzip_small(CuTest *tc)
{
    serf_bucket_t *bkt, *tmp;
    apr_pool_t *test_pool = test_setup();
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);

    tmp = SERF_BUCKET_SIMPLE_STRING("HTTP/1.1 200 OK" CRLF
                                    "Content-Type: text/plain" CRLF
                                    "Transfer-Encoding: chunked" CRLF
                                    "Content-Encoding: gzip" CRLF
                                    CRLF
                                    "2" CRLF
                                    "A",
                                    alloc);

    bkt = serf_bucket_response_create(tmp, alloc);

    {
        char buf[1024];
        apr_size_t len;
        apr_status_t status;

        status = read_all(bkt, buf, sizeof(buf), &len);

        CuAssertIntEquals(tc, SERF_ERROR_TRUNCATED_HTTP_RESPONSE, status);
    }
    test_teardown(test_pool);
}

static void test_response_bucket_peek_at_headers(CuTest *tc)
{
    apr_pool_t *test_pool = test_setup();
    serf_bucket_t *resp_bkt1, *tmp, *hdrs;
    serf_status_line sl;
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);
    const char *hdr_val, *cur;
    apr_status_t status;

#define EXP_RESPONSE "HTTP/1.1 200 OK" CRLF\
                     "Content-Type: text/plain" CRLF\
                     "Content-Length: 100" CRLF\
                     CRLF\
                     "12345678901234567890"\
                     "12345678901234567890"\
                     "12345678901234567890"

    tmp = SERF_BUCKET_SIMPLE_STRING(EXP_RESPONSE,
                                    alloc);

    resp_bkt1 = serf_bucket_response_create(tmp, alloc);

    status = serf_bucket_response_status(resp_bkt1, &sl);
    CuAssertIntEquals(tc, 200, sl.code);
    CuAssertStrEquals(tc, "OK", sl.reason);
    CuAssertIntEquals(tc, SERF_HTTP_11, sl.version);
    
    /* Ensure that the status line & headers are read in the response_bucket. */
    status = serf_bucket_response_wait_for_headers(resp_bkt1);
    CuAssert(tc, "Unexpected error when waiting for response headers",
             !SERF_BUCKET_READ_ERROR(status));

    hdrs = serf_bucket_response_get_headers(resp_bkt1);
    CuAssertPtrNotNull(tc, hdrs);

    hdr_val = serf_bucket_headers_get(hdrs, "Content-Type");
    CuAssertStrEquals(tc, "text/plain", hdr_val);
    hdr_val = serf_bucket_headers_get(hdrs, "Content-Length");
    CuAssertStrEquals(tc, "100", hdr_val);

    /* Create a new bucket for the response which still has the original
       status line & headers. */

    status = serf_response_full_become_aggregate(resp_bkt1);
    CuAssertIntEquals(tc, APR_SUCCESS, status);
    cur = EXP_RESPONSE;

    while (1) {
        const char *data;
        apr_size_t len;
        apr_status_t status;

        status = serf_bucket_read(resp_bkt1, SERF_READ_ALL_AVAIL, &data, &len);
        CuAssert(tc, "Unexpected error when waiting for response headers",
                 !SERF_BUCKET_READ_ERROR(status));
        if (SERF_BUCKET_READ_ERROR(status) ||
            APR_STATUS_IS_EOF(status))
            break;

        /* Check that the bytes read match with expected at current position. */
        CuAssertStrnEquals(tc, cur, len, data);
        cur += len;
    }

}
#undef EXP_RESPONSE

CuSuite *test_buckets(void)
{
    CuSuite *suite = CuSuiteNew();

    SUITE_ADD_TEST(suite, test_simple_bucket_readline);
    SUITE_ADD_TEST(suite, test_response_bucket_read);
    SUITE_ADD_TEST(suite, test_response_bucket_headers);
    SUITE_ADD_TEST(suite, test_response_bucket_chunked_read);
    SUITE_ADD_TEST(suite, test_response_body_too_small_cl);
    SUITE_ADD_TEST(suite, test_response_body_too_small_chunked);
    SUITE_ADD_TEST(suite, test_response_body_chunked_no_crlf);
    SUITE_ADD_TEST(suite, test_response_body_chunked_incomplete_crlf);
    SUITE_ADD_TEST(suite, test_response_body_chunked_gzip_small);
    SUITE_ADD_TEST(suite, test_response_bucket_peek_at_headers);
    SUITE_ADD_TEST(suite, test_bucket_header_set);
    SUITE_ADD_TEST(suite, test_iovec_buckets);
    SUITE_ADD_TEST(suite, test_aggregate_buckets);
    SUITE_ADD_TEST(suite, test_header_buckets);

    return suite;
}
