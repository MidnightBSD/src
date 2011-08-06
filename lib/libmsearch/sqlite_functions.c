#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "sqlite3ext.h"
#include "zlib.h"
SQLITE_EXTENSION_INIT1

int sqlite3_extension_init(sqlite3 *, char **, const sqlite3_api_routines *); 

/** 
 * Rank results from matchinfo with default pcx parameters
 * @see http://www.sqlite.org/fts3.html#matchinfo 
 */
static void 
rankfunc(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const unsigned int *match_info;
    unsigned int p, c, iPhrase, iCol;
    double score = 0.0;
    
    assert(sizeof(int) == 4);
    assert(argc > 0);

    match_info = (const unsigned int *) sqlite3_value_blob(argv[0]);
    p = match_info[0]; /* query had p phrases */
    c = match_info[1]; /* columns in table */
     
    /* Iterate through each phrase in the users query. */
    for(iPhrase = 0; iPhrase < p; iPhrase++){
        /* Now iterate through each column in the users query. For each column,
         ** increment the relevancy score by:
         **
         **   (<hit count> / <global hit count>) * <column weight>
         **
         ** aPhraseinfo[] points to the start of the data for phrase iPhrase. So
         ** the hit count and global hit counts for each column are found in
         ** aPhraseinfo[iCol*3] and aPhraseinfo[iCol*3+1], respectively.
         */
        const unsigned int *aPhraseinfo = &match_info[2 + iPhrase*c*3];
        for (iCol = 0; iCol < c; iCol++) {
            int hits_this_row = aPhraseinfo[3*iCol];
            int hits_all_rows = aPhraseinfo[3*iCol+1];
            double weight = (double) (aPhraseinfo[3*iCol+2]);
            if (hits_this_row > 0) {
                score += ((double)hits_this_row / (double)hits_all_rows) * weight;
            }
        }
    }
    sqlite3_result_double(context, score);
}

static void 
compressFunc(sqlite3_context *context, int argc, sqlite3_value **argv) {
    int in, out;
    long int out2;
    const unsigned char *inBuf;
    unsigned char *outBuf;
  
    assert(argc == 1);
    
    in = sqlite3_value_bytes(argv[0]);
    inBuf = sqlite3_value_blob(argv[0]);
    out = 13 + in + (in + 999) / 1000;
    
    outBuf = malloc((out + 4) * sizeof(char));
    outBuf[0] = in>>24 & 0xff;
    outBuf[1] = in>>16 & 0xff;
    outBuf[2] = in>>8 & 0xff;
    outBuf[3] = in & 0xff;
    
    out2 = (long int)out;
    compress(&outBuf[4], &out2, inBuf, in);
    sqlite3_result_blob(context, outBuf, out2+4, free);
}

static void 
uncompressFunc(sqlite3_context *context, int argc, sqlite3_value **argv) {
    unsigned int in, out, rc;
    const unsigned char *inBuf;
    unsigned char *outBuf;
    long int out2;

    assert(argc == 1);
  
    in = sqlite3_value_bytes(argv[0]);
  
    if (in <= 4) {
        return;
    }
    
    inBuf = sqlite3_value_blob(argv[0]);
    out = (inBuf[0]<<24) + (inBuf[1]<<16) + (inBuf[2]<<8) + inBuf[3];
    outBuf = malloc(out * sizeof(char));
    out2 = (long int)out;
    rc = uncompress(outBuf, &out2, &inBuf[4], in);
  
    if (rc != Z_OK) {
        free(outBuf);
    } else {
        sqlite3_result_blob(context, outBuf, out2, free);
    }
}

int 
sqlite3_extension_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi) {
    SQLITE_EXTENSION_INIT2(pApi)
    
    sqlite3_create_function(db, "msearch_compress", 1, SQLITE_UTF8, 0, &compressFunc, 0, 0);
    sqlite3_create_function(db, "msearch_uncompress", 1, SQLITE_UTF8, 0, uncompressFunc, 0, 0);
    sqlite3_create_function(db, "msearch_rank", 1, SQLITE_ANY, 0, rankfunc, 0, 0);

    return 0;
}
