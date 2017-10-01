/* fs-pack-test.c --- tests for the filesystem
 *
 * ====================================================================
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
#include <string.h>
#include <apr_pools.h>

#include "../svn_test.h"
#include "../../libsvn_fs/fs-loader.h"
#include "../../libsvn_fs_fs/fs.h"
#include "../../libsvn_fs_fs/fs_fs.h"

#include "svn_hash.h"
#include "svn_pools.h"
#include "svn_props.h"
#include "svn_fs.h"
#include "private/svn_string_private.h"

#include "../svn_test_fs.h"



/*** Helper Functions ***/

static void
ignore_fs_warnings(void *baton, svn_error_t *err)
{
#ifdef SVN_DEBUG
  SVN_DBG(("Ignoring FS warning %s\n",
           svn_error_symbolic_name(err ? err->apr_err : 0)));
#endif
  return;
}

/* Write the format number and maximum number of files per directory
   to a new format file in PATH, overwriting a previously existing
   file.  Use POOL for temporary allocation.

   (This implementation is largely stolen from libsvn_fs_fs/fs_fs.c.) */
static svn_error_t *
write_format(const char *path,
             int format,
             int max_files_per_dir,
             apr_pool_t *pool)
{
  const char *contents;

  path = svn_dirent_join(path, "format", pool);

  if (format >= SVN_FS_FS__MIN_LAYOUT_FORMAT_OPTION_FORMAT)
    {
      if (max_files_per_dir)
        contents = apr_psprintf(pool,
                                "%d\n"
                                "layout sharded %d\n",
                                format, max_files_per_dir);
      else
        contents = apr_psprintf(pool,
                                "%d\n"
                                "layout linear",
                                format);
    }
  else
    {
      contents = apr_psprintf(pool, "%d\n", format);
    }

    {
      const char *path_tmp;

      SVN_ERR(svn_io_write_unique(&path_tmp,
                                  svn_dirent_dirname(path, pool),
                                  contents, strlen(contents),
                                  svn_io_file_del_none, pool));

      /* rename the temp file as the real destination */
      SVN_ERR(svn_io_file_rename(path_tmp, path, pool));
    }

  /* And set the perms to make it read only */
  return svn_io_set_file_read_only(path, FALSE, pool);
}

/* Return the expected contents of "iota" in revision REV. */
static const char *
get_rev_contents(svn_revnum_t rev, apr_pool_t *pool)
{
  /* Toss in a bunch of magic numbers for spice. */
  apr_int64_t num = ((rev * 1234353 + 4358) * 4583 + ((rev % 4) << 1)) / 42;
  return apr_psprintf(pool, "%" APR_INT64_T_FMT "\n", num);
}

struct pack_notify_baton
{
  apr_int64_t expected_shard;
  svn_fs_pack_notify_action_t expected_action;
};

static svn_error_t *
pack_notify(void *baton,
            apr_int64_t shard,
            svn_fs_pack_notify_action_t action,
            apr_pool_t *pool)
{
  struct pack_notify_baton *pnb = baton;

  SVN_TEST_ASSERT(shard == pnb->expected_shard);
  SVN_TEST_ASSERT(action == pnb->expected_action);

  /* Update expectations. */
  switch (action)
    {
      case svn_fs_pack_notify_start:
        pnb->expected_action = svn_fs_pack_notify_end;
        break;

      case svn_fs_pack_notify_end:
        pnb->expected_action = svn_fs_pack_notify_start;
        pnb->expected_shard++;
        break;

      default:
        return svn_error_create(SVN_ERR_TEST_FAILED, NULL,
                                "Unknown notification action when packing");
    }

  return SVN_NO_ERROR;
}

/* Create a packed filesystem in DIR.  Set the shard size to
   SHARD_SIZE and create NUM_REVS number of revisions (in addition to
   r0).  Use POOL for allocations.  After this function successfully
   completes, the filesystem's youngest revision number will be the
   same as NUM_REVS.  */
static svn_error_t *
create_packed_filesystem(const char *dir,
                         const svn_test_opts_t *opts,
                         int num_revs,
                         int shard_size,
                         apr_pool_t *pool)
{
  svn_fs_t *fs;
  svn_fs_txn_t *txn;
  svn_fs_root_t *txn_root;
  const char *conflict;
  svn_revnum_t after_rev;
  apr_pool_t *subpool = svn_pool_create(pool);
  struct pack_notify_baton pnb;
  apr_pool_t *iterpool;
  int version;

  /* Create a filesystem, then close it */
  SVN_ERR(svn_test__create_fs(&fs, dir, opts, subpool));
  svn_pool_destroy(subpool);

  subpool = svn_pool_create(pool);

  /* Rewrite the format file */
  SVN_ERR(svn_io_read_version_file(&version,
                                   svn_dirent_join(dir, "format", subpool),
                                   subpool));
  SVN_ERR(write_format(dir, version, shard_size, subpool));

  /* Reopen the filesystem */
  SVN_ERR(svn_fs_open(&fs, dir, NULL, subpool));

  /* Revision 1: the Greek tree */
  SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, subpool));
  SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
  SVN_ERR(svn_test__create_greek_tree(txn_root, subpool));
  SVN_ERR(svn_fs_commit_txn(&conflict, &after_rev, txn, subpool));
  SVN_TEST_ASSERT(SVN_IS_VALID_REVNUM(after_rev));

  /* Revisions 2 thru NUM_REVS-1: content tweaks to "iota". */
  iterpool = svn_pool_create(subpool);
  while (after_rev < num_revs)
    {
      svn_pool_clear(iterpool);
      SVN_ERR(svn_fs_begin_txn(&txn, fs, after_rev, iterpool));
      SVN_ERR(svn_fs_txn_root(&txn_root, txn, iterpool));
      SVN_ERR(svn_test__set_file_contents(txn_root, "iota",
                                          get_rev_contents(after_rev + 1,
                                                           iterpool),
                                          iterpool));
      SVN_ERR(svn_fs_commit_txn(&conflict, &after_rev, txn, iterpool));
      SVN_TEST_ASSERT(SVN_IS_VALID_REVNUM(after_rev));
    }
  svn_pool_destroy(iterpool);
  svn_pool_destroy(subpool);

  /* Now pack the FS */
  pnb.expected_shard = 0;
  pnb.expected_action = svn_fs_pack_notify_start;
  return svn_fs_pack(dir, pack_notify, &pnb, NULL, NULL, pool);
}

/* Create a packed FSFS filesystem for revprop tests at REPO_NAME with
 * MAX_REV revisions and the given SHARD_SIZE and OPTS.  Return it in *FS.
 * Use POOL for allocations.
 */
static svn_error_t *
prepare_revprop_repo(svn_fs_t **fs,
                     const char *repo_name,
                     int max_rev,
                     int shard_size,
                     const svn_test_opts_t *opts,
                     apr_pool_t *pool)
{
  svn_fs_txn_t *txn;
  svn_fs_root_t *txn_root;
  const char *conflict;
  svn_revnum_t after_rev;
  apr_pool_t *subpool;

  /* Create the packed FS and open it. */
  SVN_ERR(create_packed_filesystem(repo_name, opts, max_rev, shard_size, pool));
  SVN_ERR(svn_fs_open(fs, repo_name, NULL, pool));

  subpool = svn_pool_create(pool);
  /* Do a commit to trigger packing. */
  SVN_ERR(svn_fs_begin_txn(&txn, *fs, max_rev, subpool));
  SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
  SVN_ERR(svn_test__set_file_contents(txn_root, "iota", "new-iota",  subpool));
  SVN_ERR(svn_fs_commit_txn(&conflict, &after_rev, txn, subpool));
  SVN_TEST_ASSERT(SVN_IS_VALID_REVNUM(after_rev));
  svn_pool_destroy(subpool);

  /* Pack the repository. */
  SVN_ERR(svn_fs_pack(repo_name, NULL, NULL, NULL, NULL, pool));

  return SVN_NO_ERROR;
}

/* For revision REV, return a short log message allocated in POOL.
 */
static svn_string_t *
default_log(svn_revnum_t rev, apr_pool_t *pool)
{
  return svn_string_createf(pool, "Default message for rev %ld", rev);
}

/* For revision REV, return a long log message allocated in POOL.
 */
static svn_string_t *
large_log(svn_revnum_t rev, apr_size_t length, apr_pool_t *pool)
{
  svn_stringbuf_t *temp = svn_stringbuf_create_ensure(100000, pool);
  int i, count = (int)(length - 50) / 6;

  svn_stringbuf_appendcstr(temp, "A ");
  for (i = 0; i < count; ++i)
    svn_stringbuf_appendcstr(temp, "very, ");

  svn_stringbuf_appendcstr(temp,
    apr_psprintf(pool, "very long message for rev %ld, indeed", rev));

  return svn_stringbuf__morph_into_string(temp);
}

/* For revision REV, return a long log message allocated in POOL.
 */
static svn_string_t *
huge_log(svn_revnum_t rev, apr_pool_t *pool)
{
  return large_log(rev, 90000, pool);
}


/*** Tests ***/

/* ------------------------------------------------------------------------ */
#define REPO_NAME "test-repo-fsfs-pack"
#define SHARD_SIZE 7
#define MAX_REV 53
static svn_error_t *
pack_filesystem(const svn_test_opts_t *opts,
                apr_pool_t *pool)
{
  int i;
  svn_node_kind_t kind;
  const char *path;
  char buf[80];
  apr_file_t *file;
  apr_size_t len;

  /* Bail (with success) on known-untestable scenarios */
  if ((strcmp(opts->fs_type, "fsfs") != 0)
      || (opts->server_minor_version && (opts->server_minor_version < 6)))
    return SVN_NO_ERROR;

  SVN_ERR(create_packed_filesystem(REPO_NAME, opts, MAX_REV, SHARD_SIZE,
                                   pool));

  /* Check to see that the pack files exist, and that the rev directories
     don't. */
  for (i = 0; i < (MAX_REV + 1) / SHARD_SIZE; i++)
    {
      path = svn_dirent_join_many(pool, REPO_NAME, "revs",
                                  apr_psprintf(pool, "%d.pack", i / SHARD_SIZE),
                                  "pack", NULL);

      /* These files should exist. */
      SVN_ERR(svn_io_check_path(path, &kind, pool));
      if (kind != svn_node_file)
        return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
                                 "Expected pack file '%s' not found", path);

      path = svn_dirent_join_many(pool, REPO_NAME, "revs",
                                  apr_psprintf(pool, "%d.pack", i / SHARD_SIZE),
                                  "manifest", NULL);
      SVN_ERR(svn_io_check_path(path, &kind, pool));
      if (kind != svn_node_file)
        return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
                                 "Expected manifest file '%s' not found",
                                 path);

      /* This directory should not exist. */
      path = svn_dirent_join_many(pool, REPO_NAME, "revs",
                                  apr_psprintf(pool, "%d", i / SHARD_SIZE),
                                  NULL);
      SVN_ERR(svn_io_check_path(path, &kind, pool));
      if (kind != svn_node_none)
        return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
                                 "Unexpected directory '%s' found", path);
    }

  /* Ensure the min-unpacked-rev jives with the above operations. */
  SVN_ERR(svn_io_file_open(&file,
                           svn_dirent_join(REPO_NAME, PATH_MIN_UNPACKED_REV,
                                           pool),
                           APR_READ | APR_BUFFERED, APR_OS_DEFAULT, pool));
  len = sizeof(buf);
  SVN_ERR(svn_io_read_length_line(file, buf, &len, pool));
  SVN_ERR(svn_io_file_close(file, pool));
  if (SVN_STR_TO_REV(buf) != (MAX_REV / SHARD_SIZE) * SHARD_SIZE)
    return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
                             "Bad '%s' contents", PATH_MIN_UNPACKED_REV);

  /* Finally, make sure the final revision directory does exist. */
  path = svn_dirent_join_many(pool, REPO_NAME, "revs",
                              apr_psprintf(pool, "%d", (i / SHARD_SIZE) + 1),
                              NULL);
  SVN_ERR(svn_io_check_path(path, &kind, pool));
  if (kind != svn_node_none)
    return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
                             "Expected directory '%s' not found", path);


  return SVN_NO_ERROR;
}
#undef REPO_NAME
#undef SHARD_SIZE
#undef MAX_REV

/* ------------------------------------------------------------------------ */
#define REPO_NAME "test-repo-fsfs-pack-even"
#define SHARD_SIZE 4
#define MAX_REV 11
static svn_error_t *
pack_even_filesystem(const svn_test_opts_t *opts,
                     apr_pool_t *pool)
{
  svn_node_kind_t kind;
  const char *path;

  /* Bail (with success) on known-untestable scenarios */
  if ((strcmp(opts->fs_type, "fsfs") != 0)
      || (opts->server_minor_version && (opts->server_minor_version < 6)))
    return SVN_NO_ERROR;

  SVN_ERR(create_packed_filesystem(REPO_NAME, opts, MAX_REV, SHARD_SIZE,
                                   pool));

  path = svn_dirent_join_many(pool, REPO_NAME, "revs", "2.pack", NULL);
  SVN_ERR(svn_io_check_path(path, &kind, pool));
  if (kind != svn_node_dir)
    return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
                             "Packing did not complete as expected");

  return SVN_NO_ERROR;
}
#undef REPO_NAME
#undef SHARD_SIZE
#undef MAX_REV

/* ------------------------------------------------------------------------ */
#define REPO_NAME "test-repo-read-packed-fs"
#define SHARD_SIZE 5
#define MAX_REV 11
static svn_error_t *
read_packed_fs(const svn_test_opts_t *opts,
               apr_pool_t *pool)
{
  svn_fs_t *fs;
  svn_stream_t *rstream;
  svn_stringbuf_t *rstring;
  svn_revnum_t i;

  /* Bail (with success) on known-untestable scenarios */
  if ((strcmp(opts->fs_type, "fsfs") != 0)
      || (opts->server_minor_version && (opts->server_minor_version < 6)))
    return SVN_NO_ERROR;

  SVN_ERR(create_packed_filesystem(REPO_NAME, opts, MAX_REV, SHARD_SIZE, pool));
  SVN_ERR(svn_fs_open(&fs, REPO_NAME, NULL, pool));

  for (i = 1; i < (MAX_REV + 1); i++)
    {
      svn_fs_root_t *rev_root;
      svn_stringbuf_t *sb;

      SVN_ERR(svn_fs_revision_root(&rev_root, fs, i, pool));
      SVN_ERR(svn_fs_file_contents(&rstream, rev_root, "iota", pool));
      SVN_ERR(svn_test__stream_to_string(&rstring, rstream, pool));

      if (i == 1)
        sb = svn_stringbuf_create("This is the file 'iota'.\n", pool);
      else
        sb = svn_stringbuf_create(get_rev_contents(i, pool), pool);

      if (! svn_stringbuf_compare(rstring, sb))
        return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
                                 "Bad data in revision %ld.", i);
    }

  return SVN_NO_ERROR;
}
#undef REPO_NAME
#undef SHARD_SIZE
#undef MAX_REV

/* ------------------------------------------------------------------------ */
#define REPO_NAME "test-repo-commit-packed-fs"
#define SHARD_SIZE 5
#define MAX_REV 10
static svn_error_t *
commit_packed_fs(const svn_test_opts_t *opts,
                 apr_pool_t *pool)
{
  svn_fs_t *fs;
  svn_fs_txn_t *txn;
  svn_fs_root_t *txn_root;
  const char *conflict;
  svn_revnum_t after_rev;

  /* Bail (with success) on known-untestable scenarios */
  if ((strcmp(opts->fs_type, "fsfs") != 0)
      || (opts->server_minor_version && (opts->server_minor_version < 6)))
    return SVN_NO_ERROR;

  /* Create the packed FS and open it. */
  SVN_ERR(create_packed_filesystem(REPO_NAME, opts, MAX_REV, 5, pool));
  SVN_ERR(svn_fs_open(&fs, REPO_NAME, NULL, pool));

  /* Now do a commit. */
  SVN_ERR(svn_fs_begin_txn(&txn, fs, MAX_REV, pool));
  SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
  SVN_ERR(svn_test__set_file_contents(txn_root, "iota",
          "How much better is it to get wisdom than gold! and to get "
          "understanding rather to be chosen than silver!", pool));
  SVN_ERR(svn_fs_commit_txn(&conflict, &after_rev, txn, pool));
  SVN_TEST_ASSERT(SVN_IS_VALID_REVNUM(after_rev));

  return SVN_NO_ERROR;
}
#undef REPO_NAME
#undef MAX_REV
#undef SHARD_SIZE

/* ------------------------------------------------------------------------ */
#define REPO_NAME "test-repo-get-set-revprop-packed-fs"
#define SHARD_SIZE 4
#define MAX_REV 10
static svn_error_t *
get_set_revprop_packed_fs(const svn_test_opts_t *opts,
                          apr_pool_t *pool)
{
  svn_fs_t *fs;
  svn_string_t *prop_value;

  /* Bail (with success) on known-untestable scenarios */
  if ((strcmp(opts->fs_type, "fsfs") != 0)
      || (opts->server_minor_version && (opts->server_minor_version < 7)))
    return SVN_NO_ERROR;

  /* Create the packed FS and open it. */
  SVN_ERR(prepare_revprop_repo(&fs, REPO_NAME, MAX_REV, SHARD_SIZE, opts,
                               pool));

  /* Try to get revprop for revision 0
   * (non-packed due to special handling). */
  SVN_ERR(svn_fs_revision_prop(&prop_value, fs, 0, SVN_PROP_REVISION_AUTHOR,
                               pool));

  /* Try to change revprop for revision 0
   * (non-packed due to special handling). */
  SVN_ERR(svn_fs_change_rev_prop(fs, 0, SVN_PROP_REVISION_AUTHOR,
                                 svn_string_create("tweaked-author", pool),
                                 pool));

  /* verify */
  SVN_ERR(svn_fs_revision_prop(&prop_value, fs, 0, SVN_PROP_REVISION_AUTHOR,
                               pool));
  SVN_TEST_STRING_ASSERT(prop_value->data, "tweaked-author");

  /* Try to get packed revprop for revision 5. */
  SVN_ERR(svn_fs_revision_prop(&prop_value, fs, 5, SVN_PROP_REVISION_AUTHOR,
                               pool));

  /* Try to change packed revprop for revision 5. */
  SVN_ERR(svn_fs_change_rev_prop(fs, 5, SVN_PROP_REVISION_AUTHOR,
                                 svn_string_create("tweaked-author2", pool),
                                 pool));

  /* verify */
  SVN_ERR(svn_fs_revision_prop(&prop_value, fs, 5, SVN_PROP_REVISION_AUTHOR,
                               pool));
  SVN_TEST_STRING_ASSERT(prop_value->data, "tweaked-author2");

  return SVN_NO_ERROR;
}
#undef REPO_NAME
#undef MAX_REV
#undef SHARD_SIZE

/* ------------------------------------------------------------------------ */
#define REPO_NAME "test-repo-get-set-large-revprop-packed-fs"
#define SHARD_SIZE 4
#define MAX_REV 11
static svn_error_t *
get_set_large_revprop_packed_fs(const svn_test_opts_t *opts,
                                apr_pool_t *pool)
{
  svn_fs_t *fs;
  svn_string_t *prop_value;
  svn_revnum_t rev;

  /* Bail (with success) on known-untestable scenarios */
  if ((strcmp(opts->fs_type, "fsfs") != 0)
      || (opts->server_minor_version && (opts->server_minor_version < 7)))
    return SVN_NO_ERROR;

  /* Create the packed FS and open it. */
  SVN_ERR(prepare_revprop_repo(&fs, REPO_NAME, MAX_REV, SHARD_SIZE, opts,
                               pool));

  /* Set commit messages to different, large values that fill the pack
   * files but do not exceed the pack size limit. */
  for (rev = 0; rev <= MAX_REV; ++rev)
    SVN_ERR(svn_fs_change_rev_prop(fs, rev, SVN_PROP_REVISION_LOG,
                                   large_log(rev, 15000, pool),
                                   pool));

  /* verify */
  for (rev = 0; rev <= MAX_REV; ++rev)
    {
      SVN_ERR(svn_fs_revision_prop(&prop_value, fs, rev,
                                   SVN_PROP_REVISION_LOG, pool));
      SVN_TEST_STRING_ASSERT(prop_value->data,
                             large_log(rev, 15000, pool)->data);
    }

  /* Put a larger revprop into the last, some middle and the first revision
   * of a pack.  This should cause the packs to split in the middle. */
  SVN_ERR(svn_fs_change_rev_prop(fs, 3, SVN_PROP_REVISION_LOG,
                                 /* rev 0 is not packed */
                                 large_log(3, 37000, pool),
                                 pool));
  SVN_ERR(svn_fs_change_rev_prop(fs, 5, SVN_PROP_REVISION_LOG,
                                 large_log(5, 25000, pool),
                                 pool));
  SVN_ERR(svn_fs_change_rev_prop(fs, 8, SVN_PROP_REVISION_LOG,
                                 large_log(8, 25000, pool),
                                 pool));

  /* verify */
  for (rev = 0; rev <= MAX_REV; ++rev)
    {
      SVN_ERR(svn_fs_revision_prop(&prop_value, fs, rev,
                                   SVN_PROP_REVISION_LOG, pool));

      if (rev == 3)
        SVN_TEST_STRING_ASSERT(prop_value->data,
                               large_log(rev, 37000, pool)->data);
      else if (rev == 5 || rev == 8)
        SVN_TEST_STRING_ASSERT(prop_value->data,
                               large_log(rev, 25000, pool)->data);
      else
        SVN_TEST_STRING_ASSERT(prop_value->data,
                               large_log(rev, 15000, pool)->data);
    }

  return SVN_NO_ERROR;
}
#undef REPO_NAME
#undef MAX_REV
#undef SHARD_SIZE

/* ------------------------------------------------------------------------ */
#define REPO_NAME "test-repo-get-set-huge-revprop-packed-fs"
#define SHARD_SIZE 4
#define MAX_REV 10
static svn_error_t *
get_set_huge_revprop_packed_fs(const svn_test_opts_t *opts,
                               apr_pool_t *pool)
{
  svn_fs_t *fs;
  svn_string_t *prop_value;
  svn_revnum_t rev;

  /* Bail (with success) on known-untestable scenarios */
  if ((strcmp(opts->fs_type, "fsfs") != 0)
      || (opts->server_minor_version && (opts->server_minor_version < 7)))
    return SVN_NO_ERROR;

  /* Create the packed FS and open it. */
  SVN_ERR(prepare_revprop_repo(&fs, REPO_NAME, MAX_REV, SHARD_SIZE, opts,
                               pool));

  /* Set commit messages to different values */
  for (rev = 0; rev <= MAX_REV; ++rev)
    SVN_ERR(svn_fs_change_rev_prop(fs, rev, SVN_PROP_REVISION_LOG,
                                   default_log(rev, pool),
                                   pool));

  /* verify */
  for (rev = 0; rev <= MAX_REV; ++rev)
    {
      SVN_ERR(svn_fs_revision_prop(&prop_value, fs, rev,
                                   SVN_PROP_REVISION_LOG, pool));
      SVN_TEST_STRING_ASSERT(prop_value->data, default_log(rev, pool)->data);
    }

  /* Put a huge revprop into the last, some middle and the first revision
   * of a pack.  They will cause the pack files to split accordingly. */
  SVN_ERR(svn_fs_change_rev_prop(fs, 3, SVN_PROP_REVISION_LOG,
                                 huge_log(3, pool),
                                 pool));
  SVN_ERR(svn_fs_change_rev_prop(fs, 5, SVN_PROP_REVISION_LOG,
                                 huge_log(5, pool),
                                 pool));
  SVN_ERR(svn_fs_change_rev_prop(fs, 8, SVN_PROP_REVISION_LOG,
                                 huge_log(8, pool),
                                 pool));

  /* verify */
  for (rev = 0; rev <= MAX_REV; ++rev)
    {
      SVN_ERR(svn_fs_revision_prop(&prop_value, fs, rev,
                                   SVN_PROP_REVISION_LOG, pool));

      if (rev == 3 || rev == 5 || rev == 8)
        SVN_TEST_STRING_ASSERT(prop_value->data,
                               huge_log(rev, pool)->data);
      else
        SVN_TEST_STRING_ASSERT(prop_value->data,
                               default_log(rev, pool)->data);
    }

  return SVN_NO_ERROR;
}
#undef REPO_NAME
#undef MAX_REV
#undef SHARD_SIZE

/* ------------------------------------------------------------------------ */
/* Regression test for issue #3571 (fsfs 'svnadmin recover' expects
   youngest revprop to be outside revprops.db). */
#define REPO_NAME "test-repo-recover-fully-packed"
#define SHARD_SIZE 4
#define MAX_REV 7
static svn_error_t *
recover_fully_packed(const svn_test_opts_t *opts,
                     apr_pool_t *pool)
{
  apr_pool_t *subpool;
  svn_fs_t *fs;
  svn_fs_txn_t *txn;
  svn_fs_root_t *txn_root;
  const char *conflict;
  svn_revnum_t after_rev;
  svn_error_t *err;

  /* Bail (with success) on known-untestable scenarios */
  if ((strcmp(opts->fs_type, "fsfs") != 0)
      || (opts->server_minor_version && (opts->server_minor_version < 7)))
    return SVN_NO_ERROR;

  /* Create a packed FS for which every revision will live in a pack
     digest file, and then recover it. */
  SVN_ERR(create_packed_filesystem(REPO_NAME, opts, MAX_REV, SHARD_SIZE, pool));
  SVN_ERR(svn_fs_recover(REPO_NAME, NULL, NULL, pool));

  /* Add another revision, re-pack, re-recover. */
  subpool = svn_pool_create(pool);
  SVN_ERR(svn_fs_open(&fs, REPO_NAME, NULL, subpool));
  SVN_ERR(svn_fs_begin_txn(&txn, fs, MAX_REV, subpool));
  SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
  SVN_ERR(svn_test__set_file_contents(txn_root, "A/mu", "new-mu", subpool));
  SVN_ERR(svn_fs_commit_txn(&conflict, &after_rev, txn, subpool));
  SVN_TEST_ASSERT(SVN_IS_VALID_REVNUM(after_rev));
  svn_pool_destroy(subpool);
  SVN_ERR(svn_fs_pack(REPO_NAME, NULL, NULL, NULL, NULL, pool));
  SVN_ERR(svn_fs_recover(REPO_NAME, NULL, NULL, pool));

  /* Now, delete the youngest revprop file, and recover again.  This
     time we want to see an error! */
  SVN_ERR(svn_io_remove_file2(
              svn_dirent_join_many(pool, REPO_NAME, PATH_REVPROPS_DIR,
                                   apr_psprintf(pool, "%ld/%ld",
                                                after_rev / SHARD_SIZE,
                                                after_rev),
                                   NULL),
              FALSE, pool));
  err = svn_fs_recover(REPO_NAME, NULL, NULL, pool);
  if (! err)
    return svn_error_create(SVN_ERR_TEST_FAILED, NULL,
                            "Expected SVN_ERR_FS_CORRUPT error; got none");
  if (err->apr_err != SVN_ERR_FS_CORRUPT)
    return svn_error_create(SVN_ERR_TEST_FAILED, err,
                            "Expected SVN_ERR_FS_CORRUPT error; got:");
  svn_error_clear(err);
  return SVN_NO_ERROR;
}
#undef REPO_NAME
#undef MAX_REV
#undef SHARD_SIZE

/* ------------------------------------------------------------------------ */
/* Regression test for issue #4320 (fsfs file-hinting fails when reading a rep
   from the transaction that is commiting rev = SHARD_SIZE). */
#define REPO_NAME "test-repo-file-hint-at-shard-boundary"
#define SHARD_SIZE 4
#define MAX_REV (SHARD_SIZE - 1)
static svn_error_t *
file_hint_at_shard_boundary(const svn_test_opts_t *opts,
                            apr_pool_t *pool)
{
  apr_pool_t *subpool;
  svn_fs_t *fs;
  svn_fs_txn_t *txn;
  svn_fs_root_t *txn_root;
  const char *file_contents;
  svn_stringbuf_t *retrieved_contents;
  svn_error_t *err = SVN_NO_ERROR;

  /* Bail (with success) on known-untestable scenarios */
  if ((strcmp(opts->fs_type, "fsfs") != 0)
      || (opts->server_minor_version && (opts->server_minor_version < 8)))
    return SVN_NO_ERROR;

  /* Create a packed FS and MAX_REV revisions */
  SVN_ERR(create_packed_filesystem(REPO_NAME, opts, MAX_REV, SHARD_SIZE, pool));

  /* Reopen the filesystem */
  subpool = svn_pool_create(pool);
  SVN_ERR(svn_fs_open(&fs, REPO_NAME, NULL, subpool));

  /* Revision = SHARD_SIZE */
  file_contents = get_rev_contents(SHARD_SIZE, subpool);
  SVN_ERR(svn_fs_begin_txn(&txn, fs, MAX_REV, subpool));
  SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
  SVN_ERR(svn_test__set_file_contents(txn_root, "iota", file_contents,
                                      subpool));

  /* Retrieve the file. */
  SVN_ERR(svn_test__get_file_contents(txn_root, "iota", &retrieved_contents,
                                      subpool));
  if (strcmp(retrieved_contents->data, file_contents))
    {
      err = svn_error_create(SVN_ERR_TEST_FAILED, err,
                              "Retrieved incorrect contents from iota.");
    }

  /* Close the repo. */
  svn_pool_destroy(subpool);

  return err;
}
#undef REPO_NAME
#undef MAX_REV
#undef SHARD_SIZE

/* ------------------------------------------------------------------------ */
#define REPO_NAME "get_set_multiple_huge_revprops_packed_fs"
#define SHARD_SIZE 4
#define MAX_REV 9
static svn_error_t *
get_set_multiple_huge_revprops_packed_fs(const svn_test_opts_t *opts,
                                         apr_pool_t *pool)
{
  svn_fs_t *fs;
  svn_string_t *prop_value;
  svn_revnum_t rev;

  /* Bail (with success) on known-untestable scenarios */
  if ((strcmp(opts->fs_type, "fsfs") != 0)
      || (opts->server_minor_version && (opts->server_minor_version < 7)))
    return SVN_NO_ERROR;

  /* Create the packed FS and open it. */
  SVN_ERR(prepare_revprop_repo(&fs, REPO_NAME, MAX_REV, SHARD_SIZE, opts,
                               pool));

  /* Set commit messages to different values */
  for (rev = 0; rev <= MAX_REV; ++rev)
    SVN_ERR(svn_fs_change_rev_prop(fs, rev, SVN_PROP_REVISION_LOG,
                                   default_log(rev, pool),
                                   pool));

  /* verify */
  for (rev = 0; rev <= MAX_REV; ++rev)
    {
      SVN_ERR(svn_fs_revision_prop(&prop_value, fs, rev,
                                   SVN_PROP_REVISION_LOG, pool));
      SVN_TEST_STRING_ASSERT(prop_value->data, default_log(rev, pool)->data);
    }

  /* Put a huge revprop into revision 1 and 2. */
  SVN_ERR(svn_fs_change_rev_prop(fs, 1, SVN_PROP_REVISION_LOG,
                                 huge_log(1, pool),
                                 pool));
  SVN_ERR(svn_fs_change_rev_prop(fs, 2, SVN_PROP_REVISION_LOG,
                                 huge_log(2, pool),
                                 pool));
  SVN_ERR(svn_fs_change_rev_prop(fs, 5, SVN_PROP_REVISION_LOG,
                                 huge_log(5, pool),
                                 pool));
  SVN_ERR(svn_fs_change_rev_prop(fs, 6, SVN_PROP_REVISION_LOG,
                                 huge_log(6, pool),
                                 pool));

  /* verify */
  for (rev = 0; rev <= MAX_REV; ++rev)
    {
      SVN_ERR(svn_fs_revision_prop(&prop_value, fs, rev,
                                   SVN_PROP_REVISION_LOG, pool));

      if (rev == 1 || rev == 2 || rev == 5 || rev == 6)
        SVN_TEST_STRING_ASSERT(prop_value->data,
                               huge_log(rev, pool)->data);
      else
        SVN_TEST_STRING_ASSERT(prop_value->data,
                               default_log(rev, pool)->data);
    }

  return SVN_NO_ERROR;
}
#undef REPO_NAME
#undef MAX_REV
#undef SHARD_SIZE

/* ------------------------------------------------------------------------ */

#define REPO_NAME "revprop_caching_on_off"
static svn_error_t *
revprop_caching_on_off(const svn_test_opts_t *opts,
                       apr_pool_t *pool)
{
  svn_fs_t *fs1;
  svn_fs_t *fs2;
  apr_hash_t *fs_config;
  svn_string_t *value;
  const svn_string_t *another_value_for_avoiding_warnings_from_a_broken_api;
  const svn_string_t *new_value = svn_string_create("new", pool);

  if (strcmp(opts->fs_type, "fsfs") != 0)
    return svn_error_create(SVN_ERR_TEST_SKIPPED, NULL, NULL);

  /* Open two filesystem objects, enable revision property caching
   * in one of them. */
  SVN_ERR(svn_test__create_fs(&fs1, REPO_NAME, opts, pool));

  fs_config = apr_hash_make(pool);
  apr_hash_set(fs_config, SVN_FS_CONFIG_FSFS_CACHE_REVPROPS,
               APR_HASH_KEY_STRING, "1");

  SVN_ERR(svn_fs_open(&fs2, svn_fs_path(fs1, pool), fs_config, pool));

  /* With inefficient named atomics, the filesystem will output a warning
     and disable the revprop caching, but we still would like to test
     these cases.  Ignore the warning(s). */
  svn_fs_set_warning_func(fs2, ignore_fs_warnings, NULL);

  SVN_ERR(svn_fs_revision_prop(&value, fs2, 0, "svn:date", pool));
  another_value_for_avoiding_warnings_from_a_broken_api = value;
  SVN_ERR(svn_fs_change_rev_prop2(
              fs1, 0, "svn:date",
              &another_value_for_avoiding_warnings_from_a_broken_api,
              new_value, pool));

  /* Expect the change to be visible through both objects.*/
  SVN_ERR(svn_fs_revision_prop(&value, fs1, 0, "svn:date", pool));
  SVN_TEST_STRING_ASSERT(value->data, "new");

  SVN_ERR(svn_fs_revision_prop(&value, fs2, 0, "svn:date", pool));
  SVN_TEST_STRING_ASSERT(value->data, "new");

  return SVN_NO_ERROR;
}

#undef REPO_NAME

/* ------------------------------------------------------------------------ */

#define REPO_NAME "test-repo-delta_chain_with_plain"

static svn_error_t *
delta_chain_with_plain(const svn_test_opts_t *opts,
                       apr_pool_t *pool)
{
  svn_fs_t *fs;
  fs_fs_data_t *ffd;
  svn_fs_txn_t *txn;
  svn_fs_root_t *root;
  svn_revnum_t rev;
  svn_stringbuf_t *prop_value, *contents, *contents2, *hash_rep;
  int i;
  apr_hash_t *fs_config, *props;

  if (strcmp(opts->fs_type, "fsfs") != 0)
    return svn_error_create(SVN_ERR_TEST_SKIPPED, NULL, NULL);

  /* Reproducing issue #4577 without the r1676667 fix is much harder in 1.9+
   * than it was in 1.8.  The reason is that 1.9+ won't deltify small reps
   * nor against small reps.  So, we must construct relatively large PLAIN
   * and DELTA reps.
   *
   * The idea is to construct a PLAIN prop rep, make a file share that as
   * its text rep, grow the file considerably (to make the PLAIN rep later
   * read beyond EOF) and then replace it entirely with another longish
   * contents.
   */

  /* Create a repo that and explicitly enable rep sharing. */
  SVN_ERR(svn_test__create_fs(&fs, REPO_NAME, opts, pool));

  ffd = fs->fsap_data;
  if (ffd->format < SVN_FS_FS__MIN_REP_SHARING_FORMAT)
    return svn_error_create(SVN_ERR_TEST_SKIPPED, NULL, NULL);

  ffd->rep_sharing_allowed = TRUE;

  /* Make sure all props are stored as PLAIN reps. */
  ffd->deltify_properties = FALSE;

  /* Construct various content strings.
   * Note that props need to be shorter than the file contents. */
  prop_value = svn_stringbuf_create("prop", pool);
  for (i = 0; i < 10; ++i)
    svn_stringbuf_appendstr(prop_value, prop_value);

  contents = svn_stringbuf_create("Some text.", pool);
  for (i = 0; i < 10; ++i)
    svn_stringbuf_appendstr(contents, contents);

  contents2 = svn_stringbuf_create("Totally new!", pool);
  for (i = 0; i < 10; ++i)
    svn_stringbuf_appendstr(contents2, contents2);

  /* Revision 1: create a property rep. */
  SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
  SVN_ERR(svn_fs_txn_root(&root, txn, pool));
  SVN_ERR(svn_fs_change_node_prop(root, "/", "p",
                                  svn_string_create(prop_value->data, pool),
                                  pool));
  SVN_ERR(svn_fs_commit_txn(NULL, &rev, txn, pool));

  /* Revision 2: create a file that shares the text rep with the PLAIN
   * property rep from r1. */
  props = apr_hash_make(pool);
  apr_hash_set(props, "p", APR_HASH_KEY_STRING,
               svn_string_create(prop_value->data, pool));

  hash_rep = svn_stringbuf_create_empty(pool);
  svn_hash_write2(props, svn_stream_from_stringbuf(hash_rep, pool), "END",
                  pool);

  SVN_ERR(svn_fs_begin_txn(&txn, fs, rev, pool));
  SVN_ERR(svn_fs_txn_root(&root, txn, pool));
  SVN_ERR(svn_fs_make_file(root, "foo", pool));
  SVN_ERR(svn_test__set_file_contents(root, "foo", hash_rep->data, pool));
  SVN_ERR(svn_fs_commit_txn(NULL, &rev, txn, pool));

  /* Revision 3: modify the file contents to a long-ish full text
   * (~10kByte, longer than the r1 revision file). */
  SVN_ERR(svn_fs_begin_txn(&txn, fs, rev, pool));
  SVN_ERR(svn_fs_txn_root(&root, txn, pool));
  SVN_ERR(svn_test__set_file_contents(root, "foo", contents->data, pool));
  SVN_ERR(svn_fs_commit_txn(NULL, &rev, txn, pool));

  /* Revision 4: replace file contents to something disjoint from r3. */
  SVN_ERR(svn_fs_begin_txn(&txn, fs, rev, pool));
  SVN_ERR(svn_fs_txn_root(&root, txn, pool));
  SVN_ERR(svn_test__set_file_contents(root, "foo", contents2->data, pool));
  SVN_ERR(svn_fs_commit_txn(NULL, &rev, txn, pool));

  /* Getting foo@4 must work.  To make sure we actually read from disk,
   * use a new FS instance with disjoint caches. */
  fs_config = apr_hash_make(pool);
  apr_hash_set(fs_config, SVN_FS_CONFIG_FSFS_CACHE_NS, APR_HASH_KEY_STRING,
                          svn_uuid_generate(pool));
  SVN_ERR(svn_fs_open(&fs, REPO_NAME, fs_config, pool));

  SVN_ERR(svn_fs_revision_root(&root, fs, rev, pool));
  SVN_ERR(svn_test__get_file_contents(root, "foo", &contents, pool));
  SVN_TEST_STRING_ASSERT(contents->data, contents2->data);

  return SVN_NO_ERROR;
}

#undef REPO_NAME

/* ------------------------------------------------------------------------ */
#define REPO_NAME "test-repo-plain_0_length"

static char *
stringbuf_find(svn_stringbuf_t *rev_contents,
               const char *substring)
{
  apr_size_t i;
  apr_size_t len = strlen(substring);

  for (i = 0; i < rev_contents->len - len + 1; ++i)
      if (!memcmp(rev_contents->data + i, substring, len))
        return rev_contents->data + i;

  return NULL;
}


static svn_stringbuf_t *
get_line(svn_stringbuf_t *rev_contents,
         const char *prefix,
         apr_pool_t *pool)
{
  char *end, *start = stringbuf_find(rev_contents, prefix);
  if (start == NULL)
    return svn_stringbuf_create_empty(pool);

  end = strchr(start, '\n');
  if (end == NULL)
    return svn_stringbuf_create_empty(pool);

  return svn_stringbuf_ncreate(start, end - start, pool);
}

static svn_error_t *
plain_0_length(const svn_test_opts_t *opts,
               apr_pool_t *pool)
{
  svn_fs_t *fs;
  fs_fs_data_t *ffd;
  svn_fs_txn_t *txn;
  svn_fs_root_t *root;
  svn_revnum_t rev;
  const char *rev_path;
  svn_stringbuf_t *rev_contents, *props_line;
  char *text;
  apr_hash_t *fs_config;
  svn_filesize_t file_length;
  apr_file_t *file;

  if (strcmp(opts->fs_type, "fsfs") != 0)
    return svn_error_create(SVN_ERR_TEST_SKIPPED, NULL, NULL);

  /* Create a repo that does not deltify properties and does not share reps
     on its own - makes it easier to do that later by hand. */
  SVN_ERR(svn_test__create_fs(&fs, REPO_NAME, opts, pool));
  ffd = fs->fsap_data;
  ffd->deltify_properties = FALSE;
  ffd->rep_sharing_allowed = FALSE;

  /* Create one file node with matching contents and property reps. */
  SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
  SVN_ERR(svn_fs_txn_root(&root, txn, pool));
  SVN_ERR(svn_fs_make_file(root, "foo", pool));
  SVN_ERR(svn_test__set_file_contents(root, "foo", "END\n", pool));
  SVN_ERR(svn_fs_change_node_prop(root, "foo", "x", NULL, pool));
  SVN_ERR(svn_fs_commit_txn(NULL, &rev, txn, pool));

  /* Redirect text rep to props rep. */
  SVN_ERR(svn_fs_fs__path_rev_absolute(&rev_path, fs, rev, pool));
  SVN_ERR(svn_stringbuf_from_file2(&rev_contents, rev_path, pool));

  props_line = get_line(rev_contents, "props: ", pool);
  text = stringbuf_find(rev_contents, "text: ");

  if (text)
    {
      /* Explicitly set the last number before the MD5 to 0 */
      strstr(props_line->data, " 2d29")[-1] = '0';

      /* Add a padding space - in case we shorten the number in TEXT */
      svn_stringbuf_appendbyte(props_line, ' ');

      /* Make text point to the PLAIN data rep with no expanded size info. */
      memcpy(text + 6, props_line->data + 7, props_line->len - 7);
    }

  SVN_ERR(svn_io_set_file_read_write(rev_path, FALSE, pool));
  SVN_ERR(svn_io_file_open(&file, rev_path, APR_WRITE | APR_TRUNCATE,
                           APR_OS_DEFAULT, pool));
  SVN_ERR(svn_io_file_write_full(file, rev_contents->data,
                                 rev_contents->len, NULL, pool));
  SVN_ERR(svn_io_file_close(file, pool));

  /* Create an independent FS instances with separate caches etc. */
  fs_config = apr_hash_make(pool);
  apr_hash_set(fs_config, SVN_FS_CONFIG_FSFS_CACHE_NS, APR_HASH_KEY_STRING,
               svn_uuid_generate(pool));
  SVN_ERR(svn_fs_open(&fs, REPO_NAME, fs_config, pool));

  /* Now, check that we get the correct file length. */
  SVN_ERR(svn_fs_revision_root(&root, fs, rev, pool));
  SVN_ERR(svn_fs_file_length(&file_length, root, "foo", pool));

  SVN_TEST_ASSERT(file_length == 4);

  return SVN_NO_ERROR;
}

#undef REPO_NAME

/* ------------------------------------------------------------------------ */

#define REPO_NAME "test-repo-large_delta_against_plain"

static svn_error_t *
large_delta_against_plain(const svn_test_opts_t *opts,
                          apr_pool_t *pool)
{
  svn_fs_t *fs;
  fs_fs_data_t *ffd;
  svn_fs_txn_t *txn;
  svn_fs_root_t *root;
  svn_revnum_t rev;
  svn_stringbuf_t *prop_value;
  svn_string_t *prop_read;
  int i;
  apr_hash_t *fs_config;

  if (strcmp(opts->fs_type, "fsfs") != 0)
    return svn_error_create(SVN_ERR_TEST_SKIPPED, NULL, NULL);

  /* Create a repo that and explicitly enable rep sharing. */
  SVN_ERR(svn_test__create_fs(&fs, REPO_NAME, opts, pool));
  ffd = fs->fsap_data;

  /* Make sure all props are stored as PLAIN reps. */
  ffd->deltify_properties = FALSE;

  /* Construct a property larger than 2 txdelta windows. */
  prop_value = svn_stringbuf_create("prop", pool);
  while (prop_value->len <= 2 * 102400)
    svn_stringbuf_appendstr(prop_value, prop_value);

  /* Revision 1: create a property rep. */
  SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
  SVN_ERR(svn_fs_txn_root(&root, txn, pool));
  SVN_ERR(svn_fs_change_node_prop(root, "/", "p",
                                  svn_string_create(prop_value->data, pool),
                                  pool));
  SVN_ERR(svn_fs_commit_txn(NULL, &rev, txn, pool));

  /* Now, store them as DELTA reps. */
  ffd->deltify_properties = TRUE;

  /* Construct a property larger than 2 txdelta windows, distinct from the
   * previous one but with a matching "tail". */
  prop_value = svn_stringbuf_create("blob", pool);
  while (prop_value->len <= 2 * 102400)
    svn_stringbuf_appendstr(prop_value, prop_value);
  for (i = 0; i < 100; ++i)
    svn_stringbuf_appendcstr(prop_value, "prop");

  /* Revision 2: modify the property. */
  SVN_ERR(svn_fs_begin_txn(&txn, fs, 1, pool));
  SVN_ERR(svn_fs_txn_root(&root, txn, pool));
  SVN_ERR(svn_fs_change_node_prop(root, "/", "p",
                                  svn_string_create(prop_value->data, pool),
                                  pool));
  SVN_ERR(svn_fs_commit_txn(NULL, &rev, txn, pool));

  /* Reconstructing the property deltified must work.  To make sure we
   * actually read from disk, use a new FS instance with disjoint caches. */
  fs_config = apr_hash_make(pool);
  svn_hash_sets(fs_config, SVN_FS_CONFIG_FSFS_CACHE_NS,
                           svn_uuid_generate(pool));
  SVN_ERR(svn_fs_open(&fs, REPO_NAME, fs_config, pool));

  SVN_ERR(svn_fs_revision_root(&root, fs, rev, pool));
  SVN_ERR(svn_fs_node_prop(&prop_read, root, "/", "p", pool));
  SVN_TEST_STRING_ASSERT(prop_read->data, prop_value->data);

  return SVN_NO_ERROR;
}

#undef REPO_NAME

/* ------------------------------------------------------------------------ */

/* The test table.  */

struct svn_test_descriptor_t test_funcs[] =
  {
    SVN_TEST_NULL,
    SVN_TEST_OPTS_PASS(pack_filesystem,
                       "pack a FSFS filesystem"),
    SVN_TEST_OPTS_PASS(pack_even_filesystem,
                       "pack FSFS where revs % shard = 0"),
    SVN_TEST_OPTS_PASS(read_packed_fs,
                       "read from a packed FSFS filesystem"),
    SVN_TEST_OPTS_PASS(commit_packed_fs,
                       "commit to a packed FSFS filesystem"),
    SVN_TEST_OPTS_PASS(get_set_revprop_packed_fs,
                       "get/set revprop while packing FSFS filesystem"),
    SVN_TEST_OPTS_PASS(get_set_large_revprop_packed_fs,
                       "get/set large packed revprops in FSFS"),
    SVN_TEST_OPTS_PASS(get_set_huge_revprop_packed_fs,
                       "get/set huge packed revprops in FSFS"),
    SVN_TEST_OPTS_PASS(recover_fully_packed,
                       "recover a fully packed filesystem"),
    SVN_TEST_OPTS_PASS(file_hint_at_shard_boundary,
                       "test file hint at shard boundary"),
    SVN_TEST_OPTS_PASS(get_set_multiple_huge_revprops_packed_fs,
                       "set multiple huge revprops in packed FSFS"),
    SVN_TEST_OPTS_PASS(revprop_caching_on_off,
                       "change revprops with enabled and disabled caching"),
    SVN_TEST_OPTS_PASS(delta_chain_with_plain,
                       "delta chains starting with PLAIN, issue #4577"),
    SVN_TEST_OPTS_PASS(plain_0_length,
                       "file with 0 expanded-length, issue #4554"),
    SVN_TEST_OPTS_PASS(large_delta_against_plain,
                       "large deltas against PLAIN, issue #4658"),
    SVN_TEST_NULL
  };
