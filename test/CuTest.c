/* 
 * Copyright (c) 2003 Asim Jalis
 * 
 *  This software is provided 'as-is', without any express or implied
 *  warranty. In no event will the authors be held liable for any
 *  damages arising from the use of this software.
 * 
 *  Permission is granted to anyone to use this software for any
 *  purpose, including commercial applications, and to alter it and
 *  redistribute it freely, subject to the following restrictions:
 * 
 *  1. The origin of this software must not be misrepresented; you
 *  must not claim that you wrote the original software. If you use
 *  this software in a product, an acknowledgment in the product
 *  documentation would be appreciated but is not required.
 * 
 *  2. Altered source versions must be plainly marked as such, and
 *  must not be misrepresented as being the original software.
 * 
 *  3. This notice may not be removed or altered from any source
 *  distribution.
 *-------------------------------------------------------------------------*
 *
 * Originally obtained from "http://cutest.sourceforge.net/" version 1.4.
 *
 * See CuTest.h for a list of serf-specific modifications.
 */
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "CuTest.h"

/*-------------------------------------------------------------------------*
 * CuStr
 *-------------------------------------------------------------------------*/

char* CuStrAlloc(int size)
{
    char* newStr = (char*) malloc( sizeof(char) * (size) );
    return newStr;
}

char* CuStrCopy(const char* old)
{
    int len = strlen(old);
    char* newStr = CuStrAlloc(len + 1);
    strcpy(newStr, old);
    return newStr;
}

/*-------------------------------------------------------------------------*
 * CuString
 *-------------------------------------------------------------------------*/

void CuStringInit(CuString* str)
{
    str->length = 0;
    str->size = STRING_MAX;
    str->buffer = (char*) malloc(sizeof(char) * str->size);
    str->buffer[0] = '\0';
}

CuString* CuStringNew(void)
{
    CuString* str = (CuString*) malloc(sizeof(CuString));
    str->length = 0;
    str->size = STRING_MAX;
    str->buffer = (char*) malloc(sizeof(char) * str->size);
    str->buffer[0] = '\0';
    return str;
}

void CuStringFree(CuString *str)
{
    free(str->buffer);
    free(str);
}

void CuStringResize(CuString* str, int newSize)
{
    str->buffer = (char*) realloc(str->buffer, sizeof(char) * newSize);
    str->size = newSize;
}

void CuStringAppend(CuString* str, const char* text)
{
    int length;

    if (text == NULL) {
        text = "NULL";
    }

    length = strlen(text);
    if (str->length + length + 1 >= str->size)
        CuStringResize(str, str->length + length + 1 + STRING_INC);
    str->length += length;
    strcat(str->buffer, text);
}

void CuStringAppendChar(CuString* str, char ch)
{
    char text[2];
    text[0] = ch;
    text[1] = '\0';
    CuStringAppend(str, text);
}

void CuStringAppendFormat(CuString* str, const char* format, ...)
{
    va_list argp;
    char buf[HUGE_STRING_LEN];
    va_start(argp, format);
    vsprintf(buf, format, argp);
    va_end(argp);
    CuStringAppend(str, buf);
}

void CuStringInsert(CuString* str, const char* text, int pos)
{
    int length = strlen(text);
    if (pos > str->length)
        pos = str->length;
    if (str->length + length + 1 >= str->size)
        CuStringResize(str, str->length + length + 1 + STRING_INC);
    memmove(str->buffer + pos + length, str->buffer + pos, (str->length - pos) + 1);
    str->length += length;
    memcpy(str->buffer + pos, text, length);
}

/*-------------------------------------------------------------------------*
 * CuTest
 *-------------------------------------------------------------------------*/

void CuTestInit(CuTest* t, const char* name, TestFunction function)
{
    t->name = CuStrCopy(name);
    t->failed = 0;
    t->ran = 0;
    t->message = NULL;
    t->function = function;
    t->jumpBuf = NULL;
    t->setup = NULL;
    t->teardown = NULL;
    t->testBaton = NULL;
}

CuTest* CuTestNew(const char* name, TestFunction function)
{
    CuTest* tc = CU_ALLOC(CuTest);
    CuTestInit(tc, name, function);
    return tc;
}

void CuTestFree(CuTest* tc)
{
    free(tc->name);
    free(tc);
}

void CuTestRun(CuTest* tc)
{
    jmp_buf buf;
    tc->jumpBuf = &buf;
    if (tc->setup)
        tc->testBaton = tc->setup(tc);
    if (setjmp(buf) == 0)
    {
        tc->ran = 1;
        (tc->function)(tc);
    }
    if (tc->teardown)
        tc->teardown(tc->testBaton);

    tc->jumpBuf = 0;
}

static void CuFailInternal(CuTest* tc, const char* file, int line, CuString* string)
{
    char buf[HUGE_STRING_LEN];

    sprintf(buf, "%s:%d: ", file, line);
    CuStringInsert(string, buf, 0);

    tc->failed = 1;
    tc->message = string->buffer;
    if (tc->jumpBuf != 0) longjmp(*(tc->jumpBuf), 0);
}

void CuFail_Line(CuTest* tc, const char* file, int line, const char* message2, const char* message)
{
    CuString string;

    CuStringInit(&string);
    if (message2 != NULL)
    {
        CuStringAppend(&string, message2);
        CuStringAppend(&string, ": ");
    }
    CuStringAppend(&string, message);
    CuFailInternal(tc, file, line, &string);
}

void CuAssert_Line(CuTest* tc, const char* file, int line, const char* message, int condition)
{
    if (condition) return;
    CuFail_Line(tc, file, line, NULL, message);
}

void CuAssertStrnEquals_LineMsg(CuTest* tc, const char* file, int line, const char* message,
                                const char* expected, size_t explen,
                                const char* actual)
{
    CuString string;
    if ((explen == 0) ||
        (expected == NULL && actual == NULL) ||
        (expected != NULL && actual != NULL &&
         strncmp(expected, actual, explen) == 0))
    {
        return;
    }

    CuStringInit(&string);
    if (message != NULL)
    {
        CuStringAppend(&string, message);
        CuStringAppend(&string, ": ");
    }
    CuStringAppend(&string, "expected <");
    CuStringAppend(&string, expected);
    CuStringAppend(&string, "> but was <");
    CuStringAppend(&string, actual);
    CuStringAppend(&string, ">");
    CuFailInternal(tc, file, line, &string);
}

void CuAssertStrEquals_LineMsg(CuTest* tc, const char* file, int line, const char* message,
    const char* expected, const char* actual)
{
    CuString string;
    if ((expected == NULL && actual == NULL) ||
        (expected != NULL && actual != NULL &&
         strcmp(expected, actual) == 0))
    {
        return;
    }

    CuStringInit(&string);
    if (message != NULL)
    {
        CuStringAppend(&string, message);
        CuStringAppend(&string, ": ");
    }
    CuStringAppend(&string, "expected <");
    CuStringAppend(&string, expected);
    CuStringAppend(&string, "> but was <");
    CuStringAppend(&string, actual);
    CuStringAppend(&string, ">");
    CuFailInternal(tc, file, line, &string);}

void CuAssertIntEquals_LineMsg(CuTest* tc, const char* file, int line, const char* message,
    int expected, int actual)
{
    char buf[STRING_MAX];
    if (expected == actual) return;
    sprintf(buf, "expected <%d> but was <%d>", expected, actual);
    CuFail_Line(tc, file, line, message, buf);
}

void CuAssertDblEquals_LineMsg(CuTest* tc, const char* file, int line, const char* message,
    double expected, double actual, double delta)
{
    char buf[STRING_MAX];
    if (fabs(expected - actual) <= delta) return;
    sprintf(buf, "expected <%lf> but was <%lf>", expected, actual);
    CuFail_Line(tc, file, line, message, buf);
}

void CuAssertPtrEquals_LineMsg(CuTest* tc, const char* file, int line, const char* message,
    void* expected, void* actual)
{
    char buf[STRING_MAX];
    if (expected == actual) return;
    sprintf(buf, "expected pointer <0x%p> but was <0x%p>", expected, actual);
    CuFail_Line(tc, file, line, message, buf);
}


/*-------------------------------------------------------------------------*
 * CuSuite
 *-------------------------------------------------------------------------*/

void CuSuiteInit(CuSuite* testSuite)
{
    testSuite->count = 0;
    testSuite->failCount = 0;
    testSuite->setup = NULL;
    testSuite->teardown = NULL;
}

CuSuite* CuSuiteNew(void)
{
    CuSuite* testSuite = CU_ALLOC(CuSuite);
    CuSuiteInit(testSuite);
    return testSuite;
}

void CuSuiteFree(CuSuite *testSuite)
{
    free(testSuite);
}

void CuSuiteFreeDeep(CuSuite *testSuite)
{
    int i;
    for (i = 0 ; i < testSuite->count ; ++i)
    {
        CuTest* testCase = testSuite->list[i];
        CuTestFree(testCase);
    }
    free(testSuite);
}

void CuSuiteAdd(CuSuite* testSuite, CuTest *testCase)
{
    assert(testSuite->count < MAX_TEST_CASES);
    testSuite->list[testSuite->count] = testCase;
    testSuite->count++;

    /* CuSuiteAdd is called twice per test, don't reset the callbacks if
       already set. */
    if (!testCase->setup)
        testCase->setup = testSuite->setup;
    if (!testCase->teardown)
        testCase->teardown = testSuite->teardown;
}

void CuSuiteAddSuite(CuSuite* testSuite, CuSuite* testSuite2)
{
    int i;
    for (i = 0 ; i < testSuite2->count ; ++i)
    {
        CuTest* testCase = testSuite2->list[i];
        CuSuiteAdd(testSuite, testCase);
    }
}

void CuSuiteRun(CuSuite* testSuite)
{
    int i;
    for (i = 0 ; i < testSuite->count ; ++i)
    {
        CuTest* testCase = testSuite->list[i];
        CuTestRun(testCase);
        if (testCase->failed) { testSuite->failCount += 1; }
    }
}

void CuSuiteSummary(CuSuite* testSuite, CuString* summary)
{
    int i;
    for (i = 0 ; i < testSuite->count ; ++i)
    {
        CuTest* testCase = testSuite->list[i];
        CuStringAppend(summary, testCase->failed ? "F" : ".");
    }
    CuStringAppend(summary, "\n\n");
}

void CuSuiteDetails(CuSuite* testSuite, CuString* details)
{
    int i;
    int failCount = 0;

    if (testSuite->failCount == 0)
    {
        int passCount = testSuite->count - testSuite->failCount;
        const char* testWord = passCount == 1 ? "test" : "tests";
        CuStringAppendFormat(details, "OK (%d %s)\n", passCount, testWord);
    }
    else
    {
        if (testSuite->failCount == 1)
            CuStringAppend(details, "There was 1 failure:\n");
        else
            CuStringAppendFormat(details, "There were %d failures:\n", testSuite->failCount);

        for (i = 0 ; i < testSuite->count ; ++i)
        {
            CuTest* testCase = testSuite->list[i];
            if (testCase->failed)
            {
                failCount++;
                CuStringAppendFormat(details, "%d) %s: %s\n",
                    failCount, testCase->name, testCase->message);
            }
        }
        CuStringAppend(details, "\n!!!FAILURES!!!\n");

        CuStringAppendFormat(details, "Runs: %d ",   testSuite->count);
        CuStringAppendFormat(details, "Passes: %d ", testSuite->count - testSuite->failCount);
        CuStringAppendFormat(details, "Fails: %d\n",  testSuite->failCount);
    }
}

void CuSuiteSetSetupTeardownCallbacks(CuSuite* testSuite, TestCallback setup,
                                      TestCallback teardown)
{
    testSuite->setup = setup;
    testSuite->teardown = teardown;
}