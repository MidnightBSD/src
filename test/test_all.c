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
#include "test_serf.h"
#include <stdlib.h>

static const struct testlist {
    const char *testname;
    CuSuite *(*func)(void);
} tests[] = {
    {"context", test_context},
    {"buckets", test_buckets},
    {"ssl",     test_ssl},
    {"auth",    test_auth},
#if 0
    /* internal test for the mock bucket. */
    {"mock",    test_mock_bucket},
#endif
    {"LastTest", NULL}
};

int main(int argc, char *argv[])
{
    CuSuite *alltests = NULL;
    CuString *output = CuStringNew();
    int i;
    int list_provided = 0;
    int exit_code;

    apr_initialize();
    atexit(apr_terminate);

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) {
            continue;
        }
        if (!strcmp(argv[i], "-l")) {
            for (i = 0; tests[i].func != NULL; i++) {
                printf("%s\n", tests[i].testname);
            }
            exit(0);
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "invalid option: `%s'\n", argv[i]);
            exit(1);
        }
        list_provided = 1;
    }

    alltests = CuSuiteNew();
    if (!list_provided) {
        /* add everything */
        for (i = 0; tests[i].func != NULL; i++) {
            CuSuite *st = tests[i].func();
            CuSuiteAddSuite(alltests, st);
            CuSuiteFree(st);
        }
    }
    else {
        /* add only the tests listed */
        for (i = 1; i < argc; i++) {
            int j;
            int found = 0;

            if (argv[i][0] == '-') {
                continue;
            }
            for (j = 0; tests[j].func != NULL; j++) {
                if (!strcmp(argv[i], tests[j].testname)) {
                    CuSuiteAddSuite(alltests, tests[j].func());
                    found = 1;
                }
            }
            if (!found) {
                fprintf(stderr, "invalid test name: `%s'\n", argv[i]);
                exit(1);
            }
        }
    }

    CuSuiteRun(alltests);
    CuSuiteSummary(alltests, output);
    CuSuiteDetails(alltests, output);
    printf("%s\n", output->buffer);

    exit_code = alltests->failCount > 0 ? 1 : 0;

    CuSuiteFreeDeep(alltests);
    CuStringFree(output);

    return exit_code;
}
