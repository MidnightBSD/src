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

#include <apr.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include <apr_env.h>

#include "serf.h"
#include "serf_bucket_types.h"

#include "test_serf.h"

#if defined(WIN32) && defined(_DEBUG)
/* Include this file to allow running a Debug build of serf with a Release
   build of OpenSSL. */
#include <openssl/applink.c>
#endif

/* Test setting up the openssl library. */
static void test_ssl_init(CuTest *tc)
{
    serf_bucket_t *bkt, *stream;
    serf_ssl_context_t *ssl_context;
    apr_status_t status;

    apr_pool_t *test_pool = tc->testBaton;
    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(test_pool, NULL,
                                                              NULL);

    stream = SERF_BUCKET_SIMPLE_STRING("", alloc);

    bkt = serf_bucket_ssl_decrypt_create(stream, NULL,
                                         alloc);
    ssl_context = serf_bucket_ssl_decrypt_context_get(bkt);

    bkt = serf_bucket_ssl_encrypt_create(stream, ssl_context,
                                         alloc);

    status = serf_ssl_use_default_certificates(ssl_context);

    CuAssertIntEquals(tc, APR_SUCCESS, status);
}

#define get_ca_file(pool, file)  get_srcdir_file(pool, file) 
/* Test that loading a custom CA certificate file works. */
static void test_ssl_load_cert_file(CuTest *tc)
{
    serf_ssl_certificate_t *cert = NULL;

    apr_pool_t *test_pool = tc->testBaton;
    apr_status_t status = serf_ssl_load_cert_file(
        &cert, get_ca_file(test_pool, "test/serftestca.pem"), test_pool);

    CuAssertIntEquals(tc, APR_SUCCESS, status);
    CuAssertPtrNotNull(tc, cert);
}

/* Test that reading the subject from a custom CA certificate file works. */
static void test_ssl_cert_subject(CuTest *tc)
{
    apr_hash_t *subject;
    serf_ssl_certificate_t *cert = NULL;
    apr_status_t status;

    apr_pool_t *test_pool = tc->testBaton;

    status = serf_ssl_load_cert_file(&cert, get_ca_file(test_pool,
                                                        "test/serftestca.pem"),
                                     test_pool);

    CuAssertIntEquals(tc, APR_SUCCESS, status);
    CuAssertPtrNotNull(tc, cert);

    subject = serf_ssl_cert_subject(cert, test_pool);
    CuAssertPtrNotNull(tc, subject);

    CuAssertStrEquals(tc, "Serf",
                      apr_hash_get(subject, "CN", APR_HASH_KEY_STRING));
    CuAssertStrEquals(tc, "Test Suite",
                      apr_hash_get(subject, "OU", APR_HASH_KEY_STRING));
    CuAssertStrEquals(tc, "In Serf we trust, Inc.", 
                      apr_hash_get(subject, "O", APR_HASH_KEY_STRING));
    CuAssertStrEquals(tc, "Mechelen", 
                      apr_hash_get(subject, "L", APR_HASH_KEY_STRING));
    CuAssertStrEquals(tc, "Antwerp", 
                      apr_hash_get(subject, "ST", APR_HASH_KEY_STRING));
    CuAssertStrEquals(tc, "BE", 
                      apr_hash_get(subject, "C", APR_HASH_KEY_STRING));
    CuAssertStrEquals(tc, "serf@example.com", 
                      apr_hash_get(subject, "E", APR_HASH_KEY_STRING));
}

/* Test that reading the issuer from a custom CA certificate file works. */
static void test_ssl_cert_issuer(CuTest *tc)
{
    apr_hash_t *issuer;
    serf_ssl_certificate_t *cert = NULL;
    apr_status_t status;

    apr_pool_t *test_pool = tc->testBaton;

    status = serf_ssl_load_cert_file(&cert, get_ca_file(test_pool,
                                                        "test/serftestca.pem"),
                                     test_pool);

    CuAssertIntEquals(tc, APR_SUCCESS, status);
    CuAssertPtrNotNull(tc, cert);

    issuer = serf_ssl_cert_issuer(cert, test_pool);
    CuAssertPtrNotNull(tc, issuer);

    /* TODO: create a new test certificate with different issuer and subject. */
    CuAssertStrEquals(tc, "Serf",
                      apr_hash_get(issuer, "CN", APR_HASH_KEY_STRING));
    CuAssertStrEquals(tc, "Test Suite",
                      apr_hash_get(issuer, "OU", APR_HASH_KEY_STRING));
    CuAssertStrEquals(tc, "In Serf we trust, Inc.",
                      apr_hash_get(issuer, "O", APR_HASH_KEY_STRING));
    CuAssertStrEquals(tc, "Mechelen",
                      apr_hash_get(issuer, "L", APR_HASH_KEY_STRING));
    CuAssertStrEquals(tc, "Antwerp",
                      apr_hash_get(issuer, "ST", APR_HASH_KEY_STRING));
    CuAssertStrEquals(tc, "BE",
                      apr_hash_get(issuer, "C", APR_HASH_KEY_STRING));
    CuAssertStrEquals(tc, "serf@example.com",
                      apr_hash_get(issuer, "E", APR_HASH_KEY_STRING));
}

/* Test that reading the notBefore,notAfter,sha1 fingerprint and subjectAltNames
   from a custom CA certificate file works. */
static void test_ssl_cert_certificate(CuTest *tc)
{
    apr_hash_t *kv;
    serf_ssl_certificate_t *cert = NULL;
    apr_array_header_t *san_arr;
    apr_status_t status;

    apr_pool_t *test_pool = tc->testBaton;

    status = serf_ssl_load_cert_file(&cert, get_ca_file(test_pool,
                                                        "test/serftestca.pem"),
                                     test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);
    CuAssertPtrNotNull(tc, cert);

    kv = serf_ssl_cert_certificate(cert, test_pool);
    CuAssertPtrNotNull(tc, kv);

    CuAssertStrEquals(tc, "8A:4C:19:D5:F2:52:4E:35:49:5E:7A:14:80:B2:02:BD:B4:4D:22:18",
                      apr_hash_get(kv, "sha1", APR_HASH_KEY_STRING));
    CuAssertStrEquals(tc, "Mar 21 13:18:17 2008 GMT",
                      apr_hash_get(kv, "notBefore", APR_HASH_KEY_STRING));
    CuAssertStrEquals(tc, "Mar 21 13:18:17 2011 GMT",
                      apr_hash_get(kv, "notAfter", APR_HASH_KEY_STRING));

    /* TODO: create a new test certificate with a/some sAN's. */
    san_arr = apr_hash_get(kv, "subjectAltName", APR_HASH_KEY_STRING);
    CuAssertTrue(tc, san_arr == NULL);
}

static const char *extract_cert_from_pem(const char *pemdata,
                                         apr_pool_t *pool)
{
    enum { INIT, CERT_BEGIN, CERT_FOUND } state;
    serf_bucket_t *pembkt;
    const char *begincert = "-----BEGIN CERTIFICATE-----";
    const char *endcert = "-----END CERTIFICATE-----";
    char *certdata = "";
    apr_size_t certlen = 0;
    apr_status_t status = APR_SUCCESS;

    serf_bucket_alloc_t *alloc = serf_bucket_allocator_create(pool,
                                                              NULL, NULL);

    /* Extract the certificate from the .pem file, also remove newlines. */
    pembkt = SERF_BUCKET_SIMPLE_STRING(pemdata, alloc);
    state = INIT;
    while (state != CERT_FOUND && status != APR_EOF) {
        const char *data;
        apr_size_t len;
        int found;

        status = serf_bucket_readline(pembkt, SERF_NEWLINE_ANY, &found,
                                      &data, &len);
        if (SERF_BUCKET_READ_ERROR(status))
            return NULL;

        if (state == INIT) {
            if (strncmp(begincert, data, strlen(begincert)) == 0)
                state = CERT_BEGIN;
        } else if (state == CERT_BEGIN) {
            if (strncmp(endcert, data, strlen(endcert)) == 0)
                state = CERT_FOUND;
            else {
                certdata = apr_pstrcat(pool, certdata, data, NULL);
                certlen += len;
                switch (found) {
                    case SERF_NEWLINE_CR:
                    case SERF_NEWLINE_LF:
                        certdata[certlen-1] = '\0';
                        certlen --;
                        break;
                    case SERF_NEWLINE_CRLF:
                        certdata[certlen-2] = '\0';
                        certlen-=2;
                        break;
                }
            }
        }
    }

    if (state == CERT_FOUND)
        return certdata;
    else
        return NULL;
}

static void test_ssl_cert_export(CuTest *tc)
{
    serf_ssl_certificate_t *cert = NULL;
    apr_file_t *fp;
    apr_finfo_t file_info;
    const char *base64derbuf;
    char *pembuf;
    apr_size_t pemlen;
    apr_status_t status;

    apr_pool_t *test_pool = tc->testBaton;

    status = serf_ssl_load_cert_file(&cert, get_ca_file(test_pool,
                                                        "test/serftestca.pem"),
                                     test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);
    CuAssertPtrNotNull(tc, cert);

    /* A .pem file contains a Base64 encoded DER certificate, which is exactly
       what serf_ssl_cert_export is supposed to be returning. */
    status = apr_file_open(&fp,
                           get_srcdir_file(test_pool, "test/serftestca.pem"),
                           APR_FOPEN_READ | APR_FOPEN_BINARY,
                           APR_FPROT_OS_DEFAULT, test_pool);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    apr_file_info_get(&file_info, APR_FINFO_SIZE, fp);
    pembuf = apr_palloc(test_pool, file_info.size);

    status = apr_file_read_full(fp, pembuf, file_info.size, &pemlen);
    CuAssertIntEquals(tc, APR_SUCCESS, status);

    base64derbuf = serf_ssl_cert_export(cert, test_pool);

    CuAssertStrEquals(tc,
                      extract_cert_from_pem(pembuf, test_pool),
                      base64derbuf);
}

CuSuite *test_ssl(void)
{
    CuSuite *suite = CuSuiteNew();

    CuSuiteSetSetupTeardownCallbacks(suite, test_setup, test_teardown);

    SUITE_ADD_TEST(suite, test_ssl_init);
    SUITE_ADD_TEST(suite, test_ssl_load_cert_file);
    SUITE_ADD_TEST(suite, test_ssl_cert_subject);
    SUITE_ADD_TEST(suite, test_ssl_cert_issuer);
    SUITE_ADD_TEST(suite, test_ssl_cert_certificate);
    SUITE_ADD_TEST(suite, test_ssl_cert_export);

    return suite;
}
