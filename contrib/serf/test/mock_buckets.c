/* Copyright 2013 Justin Erenkrantz and Greg Stein
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

#include <apr_pools.h>

#include "serf.h"
#include "serf_bucket_util.h"
#include "test_serf.h"

/* This bucket uses a list of count - data/len - status actions (provided by the
   test case), to control the read / read_iovec operations. */
typedef struct {
    mockbkt_action *actions;
    int len;
    const char *current_data;
    int remaining_data;
    int current_action;
    int remaining_times;
} mockbkt_context_t;

serf_bucket_t *serf_bucket_mock_create(mockbkt_action *actions,
                                       int len,
                                       serf_bucket_alloc_t *allocator)
{
    mockbkt_context_t *ctx;

    ctx = serf_bucket_mem_alloc(allocator, sizeof(*ctx));
    ctx->actions = actions;
    ctx->len = len;
    ctx->current_data = 0l;
    ctx->remaining_data = -1;
    ctx->current_action = 0;
    ctx->remaining_times = -1;

    return serf_bucket_create(&serf_bucket_type_mock, allocator, ctx);
}

static apr_status_t next_action(mockbkt_context_t *ctx)
{
    mockbkt_action *action;

    while (1)
    {
        if (ctx->current_action >= ctx->len)
            return APR_EOF;

        action = &ctx->actions[ctx->current_action];

        if (ctx->remaining_times == 0) {
            ctx->current_action++;
            ctx->remaining_times = -1;
            ctx->remaining_data = -1;
            continue;
        }

        if (ctx->remaining_data <= 0) {
            ctx->current_data = action->data;
            ctx->remaining_times = action->times;
            ctx->remaining_data = strlen(action->data);
        }

        return APR_SUCCESS;
    }
}

static apr_status_t serf_mock_readline(serf_bucket_t *bucket,
                                       int acceptable, int *found,
                                       const char **data, apr_size_t *len)
{
    mockbkt_context_t *ctx = bucket->data;
    mockbkt_action *action;
    apr_status_t status;
    const char *start_line;

    status = next_action(ctx);
    if (status) {
        *len = 0;
        return status;
    }

    action = &ctx->actions[ctx->current_action];
    start_line = *data = ctx->current_data;
    *len = ctx->remaining_data;

    serf_util_readline(&start_line, len, acceptable, found);

    /* See how much ctx->current moved forward. */
    *len = start_line - ctx->current_data;
    ctx->remaining_data -= *len;
    ctx->current_data += *len;
    if (ctx->remaining_data == 0)
        ctx->remaining_times--;

    return ctx->remaining_data ? APR_SUCCESS : action->status;
}

static apr_status_t serf_mock_read(serf_bucket_t *bucket,
                                   apr_size_t requested,
                                   const char **data, apr_size_t *len)
{
    mockbkt_context_t *ctx = bucket->data;
    mockbkt_action *action;
    apr_status_t status;

    status = next_action(ctx);
    if (status) {
        *len = 0;
        return status;
    }

    action = &ctx->actions[ctx->current_action];
    *len = requested < ctx->remaining_data ? requested : ctx->remaining_data;
    *data = ctx->current_data;

    ctx->remaining_data -= *len;
    ctx->current_data += *len;

    if (ctx->remaining_data == 0)
        ctx->remaining_times--;

    return ctx->remaining_data ? APR_SUCCESS : action->status;
}

static apr_status_t serf_mock_peek(serf_bucket_t *bucket,
                                   const char **data,
                                   apr_size_t *len)
{
    mockbkt_context_t *ctx = bucket->data;
    mockbkt_action *action;
    apr_status_t status;

    status = next_action(ctx);
    if (status)
        return status;

    action = &ctx->actions[ctx->current_action];
    *len = ctx->remaining_data;
    *data = ctx->current_data;

    /* peek only returns an error, APR_EOF or APR_SUCCESS.
       APR_EAGAIN is returned as APR_SUCCESS. */
    if (SERF_BUCKET_READ_ERROR(action->status))
        return status;

    return action->status == APR_EOF ? APR_EOF : APR_SUCCESS;
}

/* An action { "", 0, APR_EAGAIN } means that serf should exit serf_context_run
   and pass the buck back to the application. As long as no new data arrives,
   this action remains active.
 
   This function allows the 'application' to trigger the arrival of more data.
   If the current action is { "", 0, APR_EAGAIN }, reduce the number of times
   the action should run by one, and proceed with the next action if needed.
 */
apr_status_t serf_bucket_mock_more_data_arrived(serf_bucket_t *bucket)
{
    mockbkt_context_t *ctx = bucket->data;
    mockbkt_action *action;
    apr_status_t status;

    status = next_action(ctx);
    if (status)
        return status;

    action = &ctx->actions[ctx->current_action];
    if (ctx->remaining_data == 0 && action->status == APR_EAGAIN) {
        ctx->remaining_times--;
        action->times--;
    }

    return APR_SUCCESS;
}

const serf_bucket_type_t serf_bucket_type_mock = {
    "MOCK",
    serf_mock_read,
    serf_mock_readline,
    serf_default_read_iovec,
    serf_default_read_for_sendfile,
    serf_default_read_bucket,
    serf_mock_peek,
    serf_default_destroy_and_data,
};


/* internal test for the mock buckets */
static void test_basic_mock_bucket(CuTest *tc)
{
    serf_bucket_t *mock_bkt;
    apr_pool_t *test_pool = tc->testBaton;
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);
    /* read one line */
    {
        mockbkt_action actions[]= {
            { 1, "HTTP/1.1 200 OK" CRLF, APR_EOF },
        };
        mock_bkt = serf_bucket_mock_create(actions, 1, alloc);
        read_and_check_bucket(tc, mock_bkt,
                              "HTTP/1.1 200 OK" CRLF);

        mock_bkt = serf_bucket_mock_create(actions, 1, alloc);
        readlines_and_check_bucket(tc, mock_bkt, SERF_NEWLINE_CRLF,
                                   "HTTP/1.1 200 OK" CRLF, 1);
    }
    /* read one line, character per character */
    {
        apr_status_t status;
        const char *expected = "HTTP/1.1 200 OK" CRLF;
        mockbkt_action actions[]= {
            { 1, "HTTP/1.1 200 OK" CRLF, APR_EOF },
        };
        mock_bkt = serf_bucket_mock_create(actions, 1, alloc);
        do
        {
            const char *data;
            apr_size_t len;

            status = serf_bucket_read(mock_bkt, 1, &data, &len);
            CuAssert(tc, "Got error during bucket reading.",
                     !SERF_BUCKET_READ_ERROR(status));
            CuAssert(tc, "Read more data than expected.",
                     strlen(expected) >= len);
            CuAssert(tc, "Read data is not equal to expected.",
                     strncmp(expected, data, len) == 0);
            CuAssert(tc, "Read more data than requested.",
                     len <= 1);

            expected += len;
        } while(!APR_STATUS_IS_EOF(status));
        
        CuAssert(tc, "Read less data than expected.", strlen(expected) == 0);
    }
    /* read multiple lines */
    {
        mockbkt_action actions[]= {
            { 1, "HTTP/1.1 200 OK" CRLF, APR_SUCCESS },
            { 1, "Content-Type: text/plain" CRLF, APR_EOF },
        };
        mock_bkt = serf_bucket_mock_create(actions, 2, alloc);
        readlines_and_check_bucket(tc, mock_bkt, SERF_NEWLINE_CRLF,
                                   "HTTP/1.1 200 OK" CRLF
                                   "Content-Type: text/plain" CRLF, 2);
    }
    /* read empty line */
    {
        mockbkt_action actions[]= {
            { 1, "HTTP/1.1 200 OK" CRLF, APR_SUCCESS },
            { 1, "", APR_EAGAIN },
            { 1, "Content-Type: text/plain" CRLF, APR_EOF },
        };
        mock_bkt = serf_bucket_mock_create(actions, 3, alloc);
        read_and_check_bucket(tc, mock_bkt,
                              "HTTP/1.1 200 OK" CRLF
                              "Content-Type: text/plain" CRLF);
        mock_bkt = serf_bucket_mock_create(actions, 3, alloc);
        readlines_and_check_bucket(tc, mock_bkt, SERF_NEWLINE_CRLF,
                                   "HTTP/1.1 200 OK" CRLF
                                   "Content-Type: text/plain" CRLF, 2);
    }
    /* read empty line */
    {
        mockbkt_action actions[]= {
            { 1, "HTTP/1.1 200 OK" CR, APR_SUCCESS },
            { 1, "", APR_EAGAIN },
            { 1, LF, APR_EOF },
        };
        mock_bkt = serf_bucket_mock_create(actions,
                                           sizeof(actions)/sizeof(actions[0]),
                                           alloc);
        read_and_check_bucket(tc, mock_bkt,
                              "HTTP/1.1 200 OK" CRLF);

        mock_bkt = serf_bucket_mock_create(actions,
                                           sizeof(actions)/sizeof(actions[0]),
                                           alloc);
        readlines_and_check_bucket(tc, mock_bkt, SERF_NEWLINE_CRLF,
                                   "HTTP/1.1 200 OK" CRLF, 1);
    }
    /* test more_data_arrived */
    {
        apr_status_t status;
        const char *data;
        apr_size_t len;
        int i;

        mockbkt_action actions[]= {
            { 1, "", APR_EAGAIN },
            { 1, "blabla", APR_EOF },
        };
        mock_bkt = serf_bucket_mock_create(actions,
                                           sizeof(actions)/sizeof(actions[0]),
                                           alloc);

        for (i = 0; i < 5; i++) {
            status = serf_bucket_peek(mock_bkt, &data, &len);
            CuAssertIntEquals(tc, APR_SUCCESS, status);
            CuAssertIntEquals(tc, 0, len);
            CuAssertIntEquals(tc, '\0', *data);
        }

        serf_bucket_mock_more_data_arrived(mock_bkt);

        status = serf_bucket_peek(mock_bkt, &data, &len);
        CuAssertIntEquals(tc, APR_EOF, status);
        CuAssertIntEquals(tc, 6, len);
        CuAssert(tc, "Read data is not equal to expected.",
                 strncmp("blabla", data, len) == 0);
    }
}

CuSuite *test_mock_bucket(void)
{
    CuSuite *suite = CuSuiteNew();

    CuSuiteSetSetupTeardownCallbacks(suite, test_setup, test_teardown);

    SUITE_ADD_TEST(suite, test_basic_mock_bucket);

    return suite;
}
