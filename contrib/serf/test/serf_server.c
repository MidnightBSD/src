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
#include <apr_base64.h>
#include <apr_getopt.h>
#include <apr_version.h>

#include "serf.h"

typedef struct {
    int foo;
} app_baton_t;


static apr_status_t incoming_request(serf_context_t *ctx,
                                        serf_incoming_request_t *req,
                                        void *request_baton,
                                        apr_pool_t *pool)
{
    printf("INCOMING REQUEST\n");
    return APR_SUCCESS;
}


static apr_status_t accept_fn(serf_context_t *ctx, 
                                        serf_listener_t *l,
                                        void *baton,
                                        apr_socket_t *insock,
                                        apr_pool_t *pool)
{
    serf_incoming_t *client = NULL;
    printf("new connection from \n");
    return serf_incoming_create(&client, ctx, insock, baton, incoming_request, pool);
}
            
static void print_usage(apr_pool_t *pool)
{
    puts("serf_server [options] listen_address:listen_port");
    puts("-h\tDisplay this help");
    puts("-v\tDisplay version");
}

int main(int argc, const char **argv)
{
    apr_status_t rv;
    apr_pool_t *pool;
    serf_context_t *context;
    serf_listener_t *listener;
    app_baton_t app_ctx;
    const char *listen_spec;
    char *addr = NULL;
    char *scope_id = NULL;
    apr_port_t port;
    apr_getopt_t *opt;
    char opt_c;
    const char *opt_arg;

    apr_initialize();
    atexit(apr_terminate);

    apr_pool_create(&pool, NULL);

    apr_getopt_init(&opt, pool, argc, argv);

    while ((rv = apr_getopt(opt, "hv", &opt_c, &opt_arg)) ==
           APR_SUCCESS) {
        switch (opt_c) {
        case 'h':
            print_usage(pool);
            exit(0);
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

    listen_spec = argv[opt->ind];


    rv = apr_parse_addr_port(&addr, &scope_id, &port, listen_spec, pool);
    if (rv) {
        printf("Error parsing listen address: %d\n", rv);
        exit(1);
    }

    if (!addr) {
        addr = "0.0.0.0";
    }

    if (port == 0) {
        port = 8080;
    }

    context = serf_context_create(pool);

    /* TODO.... stuff */
    app_ctx.foo = 1;
    rv = serf_listener_create(&listener, context, addr, port, 
                              &app_ctx, accept_fn, pool);
    if (rv) {
        printf("Error parsing listener: %d\n", rv);
        exit(1);
    }

    while (1) {
        rv = serf_context_run(context, SERF_DURATION_FOREVER, pool);
        if (APR_STATUS_IS_TIMEUP(rv))
            continue;
        if (rv) {
            char buf[200];

            printf("Error running context: (%d) %s\n", rv,
                   apr_strerror(rv, buf, sizeof(buf)));
            apr_pool_destroy(pool);
            exit(1);
        }
    }

    apr_pool_destroy(pool);
    return 0;
}
