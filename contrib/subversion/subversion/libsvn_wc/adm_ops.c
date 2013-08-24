/*
 * adm_ops.c: routines for affecting working copy administrative
 *            information.  NOTE: this code doesn't know where the adm
 *            info is actually stored.  Instead, generic handles to
 *            adm data are requested via a reference to some PATH
 *            (PATH being a regular, non-administrative directory or
 *            file in the working copy).
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



#include <string.h>
#include <stdlib.h>

#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_time.h>
#include <apr_errno.h>

#include "svn_types.h"
#include "svn_pools.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_hash.h"
#include "svn_wc.h"
#include "svn_io.h"
#include "svn_time.h"
#include "svn_sorts.h"

#include "wc.h"
#include "adm_files.h"
#include "conflicts.h"
#include "workqueue.h"

#include "svn_private_config.h"
#include "private/svn_subr_private.h"
#include "private/svn_dep_compat.h"


struct svn_wc_committed_queue_t
{
  /* The pool in which ->queue is allocated. */
  apr_pool_t *pool;
  /* Mapping (const char *) local_abspath to (committed_queue_item_t *). */
  apr_hash_t *queue;
  /* Is any item in the queue marked as 'recursive'? */
  svn_boolean_t have_recursive;
};

typedef struct committed_queue_item_t
{
  const char *local_abspath;
  svn_boolean_t recurse;
  svn_boolean_t no_unlock;
  svn_boolean_t keep_changelist;

  /* The pristine text checksum. */
  const svn_checksum_t *sha1_checksum;

  apr_hash_t *new_dav_cache;
} committed_queue_item_t;


apr_pool_t *
svn_wc__get_committed_queue_pool(const struct svn_wc_committed_queue_t *queue)
{
  return queue->pool;
}



/*** Finishing updates and commits. ***/

/* Queue work items that will finish a commit of the file or directory
 * LOCAL_ABSPATH in DB:
 *   - queue the removal of any "revert-base" props and text files;
 *   - queue an update of the DB entry for this node
 *
 * ### The Pristine Store equivalent should be:
 *   - remember the old BASE_NODE and WORKING_NODE pristine text c'sums;
 *   - queue an update of the DB entry for this node (incl. updating the
 *       BASE_NODE c'sum and setting the WORKING_NODE c'sum to NULL);
 *   - queue deletion of the old pristine texts by the remembered checksums.
 *
 * CHECKSUM is the checksum of the new text base for LOCAL_ABSPATH, and must
 * be provided if there is one, else NULL.
 *
 * STATUS, KIND, PROP_MODS and OLD_CHECKSUM are the current in-db values of
 * the node LOCAL_ABSPATH.
 */
static svn_error_t *
process_committed_leaf(svn_wc__db_t *db,
                       const char *local_abspath,
                       svn_boolean_t via_recurse,
                       svn_wc__db_status_t status,
                       svn_node_kind_t kind,
                       svn_boolean_t prop_mods,
                       const svn_checksum_t *old_checksum,
                       svn_revnum_t new_revnum,
                       apr_time_t new_changed_date,
                       const char *new_changed_author,
                       apr_hash_t *new_dav_cache,
                       svn_boolean_t no_unlock,
                       svn_boolean_t keep_changelist,
                       const svn_checksum_t *checksum,
                       apr_pool_t *scratch_pool)
{
  svn_revnum_t new_changed_rev = new_revnum;
  svn_skel_t *work_item = NULL;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(local_abspath));

  {
    const char *adm_abspath;

    if (kind == svn_node_dir)
      adm_abspath = local_abspath;
    else
      adm_abspath = svn_dirent_dirname(local_abspath, scratch_pool);
    SVN_ERR(svn_wc__write_check(db, adm_abspath, scratch_pool));
  }

  if (status == svn_wc__db_status_deleted)
    {
      return svn_error_trace(
                svn_wc__db_base_remove(
                                db, local_abspath,
                                FALSE /* keep_as_working */,
                                FALSE /* queue_deletes */,
                                TRUE  /* remove_locks */,
                                (! via_recurse)
                                    ? new_revnum : SVN_INVALID_REVNUM,
                                NULL, NULL,
                                scratch_pool));
    }
  else if (status == svn_wc__db_status_not_present)
    {
      /* We are committing the leaf of a copy operation.
         We leave the not-present marker to allow pulling in excluded
         children of a copy.

         The next update will remove the not-present marker. */

      return SVN_NO_ERROR;
    }

  SVN_ERR_ASSERT(status == svn_wc__db_status_normal
                 || status == svn_wc__db_status_incomplete
                 || status == svn_wc__db_status_added);

  if (kind != svn_node_dir)
    {
      /* If we sent a delta (meaning: post-copy modification),
         then this file will appear in the queue and so we should have
         its checksum already. */
      if (checksum == NULL)
        {
          /* It was copied and not modified. We must have a text
             base for it. And the node should have a checksum. */
          SVN_ERR_ASSERT(old_checksum != NULL);

          checksum = old_checksum;

          /* Is the node completely unmodified and are we recursing? */
          if (via_recurse && !prop_mods)
            {
              /* If a copied node itself is not modified, but the op_root of
                 the copy is committed we have to make sure that changed_rev,
                 changed_date and changed_author don't change or the working
                 copy used for committing will show different last modified
                 information then a clean checkout of exactly the same
                 revisions. (Issue #3676) */

              SVN_ERR(svn_wc__db_read_info(NULL, NULL, NULL, NULL, NULL,
                                           NULL, &new_changed_rev,
                                           &new_changed_date,
                                           &new_changed_author, NULL, NULL,
                                           NULL, NULL, NULL, NULL, NULL,
                                           NULL, NULL, NULL, NULL,
                                           NULL, NULL, NULL, NULL,
                                           NULL, NULL, NULL,
                                           db, local_abspath,
                                           scratch_pool, scratch_pool));
            }
        }

      SVN_ERR(svn_wc__wq_build_file_commit(&work_item,
                                           db, local_abspath,
                                           prop_mods,
                                           scratch_pool, scratch_pool));
    }

  /* The new text base will be found in the pristine store by its checksum. */
  SVN_ERR(svn_wc__db_global_commit(db, local_abspath,
                                   new_revnum, new_changed_rev,
                                   new_changed_date, new_changed_author,
                                   checksum,
                                   NULL /* new_children */,
                                   new_dav_cache,
                                   keep_changelist,
                                   no_unlock,
                                   work_item,
                                   scratch_pool));

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc__process_committed_internal(svn_wc__db_t *db,
                                   const char *local_abspath,
                                   svn_boolean_t recurse,
                                   svn_boolean_t top_of_recurse,
                                   svn_revnum_t new_revnum,
                                   apr_time_t new_date,
                                   const char *rev_author,
                                   apr_hash_t *new_dav_cache,
                                   svn_boolean_t no_unlock,
                                   svn_boolean_t keep_changelist,
                                   const svn_checksum_t *sha1_checksum,
                                   const svn_wc_committed_queue_t *queue,
                                   apr_pool_t *scratch_pool)
{
  svn_wc__db_status_t status;
  svn_node_kind_t kind;
  const svn_checksum_t *old_checksum;
  svn_boolean_t prop_mods;

  SVN_ERR(svn_wc__db_read_info(&status, &kind, NULL, NULL, NULL, NULL, NULL,
                               NULL, NULL, NULL, &old_checksum, NULL, NULL,
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                               NULL, NULL, &prop_mods, NULL, NULL, NULL,
                               db, local_abspath,
                               scratch_pool, scratch_pool));

  /* NOTE: be wary of making crazy semantic changes in this function, since
     svn_wc_process_committed4() calls this.  */

  SVN_ERR(process_committed_leaf(db, local_abspath, !top_of_recurse,
                                 status, kind, prop_mods, old_checksum,
                                 new_revnum, new_date, rev_author,
                                 new_dav_cache,
                                 no_unlock, keep_changelist,
                                 sha1_checksum,
                                 scratch_pool));

  /* Only check for recursion on nodes that have children */
  if (kind != svn_node_file
      || status == svn_wc__db_status_not_present
      || status == svn_wc__db_status_excluded
      || status == svn_wc__db_status_server_excluded
      /* Node deleted -> then no longer a directory */
      || status == svn_wc__db_status_deleted)
    {
      return SVN_NO_ERROR;
    }

  if (recurse)
    {
      const apr_array_header_t *children;
      apr_pool_t *iterpool = svn_pool_create(scratch_pool);
      int i;

      /* Read PATH's entries;  this is the absolute path. */
      SVN_ERR(svn_wc__db_read_children(&children, db, local_abspath,
                                       scratch_pool, iterpool));

      /* Recursively loop over all children. */
      for (i = 0; i < children->nelts; i++)
        {
          const char *name = APR_ARRAY_IDX(children, i, const char *);
          const char *this_abspath;
          const committed_queue_item_t *cqi;

          svn_pool_clear(iterpool);

          this_abspath = svn_dirent_join(local_abspath, name, iterpool);

          sha1_checksum = NULL;
          cqi = svn_hash_gets(queue->queue, this_abspath);

          if (cqi != NULL)
            sha1_checksum = cqi->sha1_checksum;

          /* Recurse.  Pass NULL for NEW_DAV_CACHE, because the
             ones present in the current call are only applicable to
             this one committed item. */
          SVN_ERR(svn_wc__process_committed_internal(
                    db, this_abspath,
                    TRUE /* recurse */,
                    FALSE /* top_of_recurse */,
                    new_revnum, new_date,
                    rev_author,
                    NULL /* new_dav_cache */,
                    TRUE /* no_unlock */,
                    keep_changelist,
                    sha1_checksum,
                    queue,
                    iterpool));
        }

      svn_pool_destroy(iterpool);
    }

  return SVN_NO_ERROR;
}


apr_hash_t *
svn_wc__prop_array_to_hash(const apr_array_header_t *props,
                           apr_pool_t *result_pool)
{
  int i;
  apr_hash_t *prophash;

  if (props == NULL || props->nelts == 0)
    return NULL;

  prophash = apr_hash_make(result_pool);

  for (i = 0; i < props->nelts; i++)
    {
      const svn_prop_t *prop = APR_ARRAY_IDX(props, i, const svn_prop_t *);
      if (prop->value != NULL)
        svn_hash_sets(prophash, prop->name, prop->value);
    }

  return prophash;
}


svn_wc_committed_queue_t *
svn_wc_committed_queue_create(apr_pool_t *pool)
{
  svn_wc_committed_queue_t *q;

  q = apr_palloc(pool, sizeof(*q));
  q->pool = pool;
  q->queue = apr_hash_make(pool);
  q->have_recursive = FALSE;

  return q;
}


svn_error_t *
svn_wc_queue_committed3(svn_wc_committed_queue_t *queue,
                        svn_wc_context_t *wc_ctx,
                        const char *local_abspath,
                        svn_boolean_t recurse,
                        const apr_array_header_t *wcprop_changes,
                        svn_boolean_t remove_lock,
                        svn_boolean_t remove_changelist,
                        const svn_checksum_t *sha1_checksum,
                        apr_pool_t *scratch_pool)
{
  committed_queue_item_t *cqi;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(local_abspath));

  queue->have_recursive |= recurse;

  /* Use the same pool as the one QUEUE was allocated in,
     to prevent lifetime issues.  Intermediate operations
     should use SCRATCH_POOL. */

  /* Add to the array with paths and options */
  cqi = apr_palloc(queue->pool, sizeof(*cqi));
  cqi->local_abspath = local_abspath;
  cqi->recurse = recurse;
  cqi->no_unlock = !remove_lock;
  cqi->keep_changelist = !remove_changelist;
  cqi->sha1_checksum = sha1_checksum;
  cqi->new_dav_cache = svn_wc__prop_array_to_hash(wcprop_changes, queue->pool);

  svn_hash_sets(queue->queue, local_abspath, cqi);

  return SVN_NO_ERROR;
}


/* Return TRUE if any item of QUEUE is a parent of ITEM and will be
   processed recursively, return FALSE otherwise.

   The algorithmic complexity of this search implementation is O(queue
   length), but it's quite quick.
*/
static svn_boolean_t
have_recursive_parent(apr_hash_t *queue,
                      const committed_queue_item_t *item,
                      apr_pool_t *scratch_pool)
{
  apr_hash_index_t *hi;
  const char *local_abspath = item->local_abspath;

  for (hi = apr_hash_first(scratch_pool, queue); hi; hi = apr_hash_next(hi))
    {
      const committed_queue_item_t *qi = svn__apr_hash_index_val(hi);

      if (qi == item)
        continue;

      if (qi->recurse && svn_dirent_is_child(qi->local_abspath, local_abspath,
                                             NULL))
        return TRUE;
    }

  return FALSE;
}


svn_error_t *
svn_wc_process_committed_queue2(svn_wc_committed_queue_t *queue,
                                svn_wc_context_t *wc_ctx,
                                svn_revnum_t new_revnum,
                                const char *rev_date,
                                const char *rev_author,
                                svn_cancel_func_t cancel_func,
                                void *cancel_baton,
                                apr_pool_t *scratch_pool)
{
  apr_array_header_t *sorted_queue;
  int i;
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);
  apr_time_t new_date;
  apr_hash_t *run_wqs = apr_hash_make(scratch_pool);
  apr_hash_index_t *hi;

  if (rev_date)
    SVN_ERR(svn_time_from_cstring(&new_date, rev_date, iterpool));
  else
    new_date = 0;

  /* Process the queued items in order of their paths.  (The requirement is
   * probably just that a directory must be processed before its children.) */
  sorted_queue = svn_sort__hash(queue->queue, svn_sort_compare_items_as_paths,
                                scratch_pool);
  for (i = 0; i < sorted_queue->nelts; i++)
    {
      const svn_sort__item_t *sort_item
        = &APR_ARRAY_IDX(sorted_queue, i, svn_sort__item_t);
      const committed_queue_item_t *cqi = sort_item->value;
      const char *wcroot_abspath;

      svn_pool_clear(iterpool);

      /* Skip this item if it is a child of a recursive item, because it has
         been (or will be) accounted for when that recursive item was (or
         will be) processed. */
      if (queue->have_recursive && have_recursive_parent(queue->queue, cqi,
                                                         iterpool))
        continue;

      SVN_ERR(svn_wc__process_committed_internal(
                wc_ctx->db, cqi->local_abspath,
                cqi->recurse,
                TRUE /* top_of_recurse */,
                new_revnum, new_date, rev_author,
                cqi->new_dav_cache,
                cqi->no_unlock,
                cqi->keep_changelist,
                cqi->sha1_checksum, queue,
                iterpool));

      /* Don't run the wq now, but remember that we must call it for this
         working copy */
      SVN_ERR(svn_wc__db_get_wcroot(&wcroot_abspath,
                                    wc_ctx->db, cqi->local_abspath,
                                    iterpool, iterpool));

      if (! svn_hash_gets(run_wqs, wcroot_abspath))
        {
          wcroot_abspath = apr_pstrdup(scratch_pool, wcroot_abspath);
          svn_hash_sets(run_wqs, wcroot_abspath, wcroot_abspath);
        }
    }

  /* Make sure nothing happens if this function is called again.  */
  apr_hash_clear(queue->queue);

  /* Ok; everything is committed now. Now we can start calling callbacks */

  if (cancel_func)
    SVN_ERR(cancel_func(cancel_baton));

  for (hi = apr_hash_first(scratch_pool, run_wqs);
       hi;
       hi = apr_hash_next(hi))
    {
      const char *wcroot_abspath = svn__apr_hash_index_key(hi);

      svn_pool_clear(iterpool);

      SVN_ERR(svn_wc__wq_run(wc_ctx->db, wcroot_abspath,
                             cancel_func, cancel_baton,
                             iterpool));
    }

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

/* Schedule the single node at LOCAL_ABSPATH, of kind KIND, for addition in
 * its parent directory in the WC.  It will have the regular properties
 * provided in PROPS, or none if that is NULL.
 *
 * If the node is a file, set its on-disk executable and read-only bits to
 * match its properties and lock state,
 * ### only if it has an svn:executable or svn:needs-lock property.
 * ### This is to match the previous behaviour of setting its props
 *     afterwards by calling svn_wc_prop_set4(), but is not very clean.
 *
 * Sync the on-disk executable and read-only bits accordingly.
 */
static svn_error_t *
add_from_disk(svn_wc__db_t *db,
              const char *local_abspath,
              svn_node_kind_t kind,
              const apr_hash_t *props,
              apr_pool_t *scratch_pool)
{
  if (kind == svn_node_file)
    {
      svn_skel_t *work_item = NULL;

      if (props && (svn_prop_get_value(props, SVN_PROP_EXECUTABLE)
                    || svn_prop_get_value(props, SVN_PROP_NEEDS_LOCK)))
        SVN_ERR(svn_wc__wq_build_sync_file_flags(&work_item, db, local_abspath,
                                                 scratch_pool, scratch_pool));

      SVN_ERR(svn_wc__db_op_add_file(db, local_abspath, props, work_item,
                                     scratch_pool));
      if (work_item)
        SVN_ERR(svn_wc__wq_run(db, local_abspath, NULL, NULL, scratch_pool));
    }
  else
    {
      SVN_ERR(svn_wc__db_op_add_directory(db, local_abspath, props, NULL,
                                          scratch_pool));
    }

  return SVN_NO_ERROR;
}


/* Set *REPOS_ROOT_URL and *REPOS_UUID to the repository of the parent of
   LOCAL_ABSPATH.  REPOS_ROOT_URL and/or REPOS_UUID may be NULL if not
   wanted.  Check that the parent of LOCAL_ABSPATH is a versioned directory
   in a state in which a new child node can be scheduled for addition;
   return an error if not. */
static svn_error_t *
check_can_add_to_parent(const char **repos_root_url,
                        const char **repos_uuid,
                        svn_wc__db_t *db,
                        const char *local_abspath,
                        apr_pool_t *result_pool,
                        apr_pool_t *scratch_pool)
{
  const char *parent_abspath = svn_dirent_dirname(local_abspath, scratch_pool);
  svn_wc__db_status_t parent_status;
  svn_node_kind_t parent_kind;
  svn_error_t *err;

  SVN_ERR(svn_wc__write_check(db, parent_abspath, scratch_pool));

  err = svn_wc__db_read_info(&parent_status, &parent_kind, NULL,
                             NULL, repos_root_url, repos_uuid, NULL, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                             NULL, NULL, NULL,
                             db, parent_abspath, result_pool, scratch_pool);

  if (err
      || parent_status == svn_wc__db_status_not_present
      || parent_status == svn_wc__db_status_excluded
      || parent_status == svn_wc__db_status_server_excluded)
    {
      return
        svn_error_createf(SVN_ERR_ENTRY_NOT_FOUND, err,
                          _("Can't find parent directory's node while"
                            " trying to add '%s'"),
                          svn_dirent_local_style(local_abspath,
                                                 scratch_pool));
    }
  else if (parent_status == svn_wc__db_status_deleted)
    {
      return
        svn_error_createf(SVN_ERR_WC_SCHEDULE_CONFLICT, NULL,
                          _("Can't add '%s' to a parent directory"
                            " scheduled for deletion"),
                          svn_dirent_local_style(local_abspath,
                                                 scratch_pool));
    }
  else if (parent_kind != svn_node_dir)
    return svn_error_createf(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
                             _("Can't schedule an addition of '%s'"
                               " below a not-directory node"),
                             svn_dirent_local_style(local_abspath,
                                                    scratch_pool));

  /* If we haven't found the repository info yet, find it now. */
  if ((repos_root_url && ! *repos_root_url)
      || (repos_uuid && ! *repos_uuid))
    {
      if (parent_status == svn_wc__db_status_added)
        SVN_ERR(svn_wc__db_scan_addition(NULL, NULL, NULL,
                                         repos_root_url, repos_uuid, NULL,
                                         NULL, NULL, NULL,
                                         db, parent_abspath,
                                         result_pool, scratch_pool));
      else
        SVN_ERR(svn_wc__db_scan_base_repos(NULL,
                                           repos_root_url, repos_uuid,
                                           db, parent_abspath,
                                           result_pool, scratch_pool));
    }

  return SVN_NO_ERROR;
}


/* Check that the on-disk item at LOCAL_ABSPATH can be scheduled for
 * addition to its WC parent directory.
 *
 * Set *KIND_P to the kind of node to be added, *DB_ROW_EXISTS_P to whether
 * it is already a versioned path, and if so, *IS_WC_ROOT_P to whether it's
 * a WC root.
 *
 * ### The checks here, and the outputs, are geared towards svn_wc_add4().
 */
static svn_error_t *
check_can_add_node(svn_node_kind_t *kind_p,
                   svn_boolean_t *db_row_exists_p,
                   svn_boolean_t *is_wc_root_p,
                   svn_wc__db_t *db,
                   const char *local_abspath,
                   const char *copyfrom_url,
                   svn_revnum_t copyfrom_rev,
                   apr_pool_t *scratch_pool)
{
  const char *base_name = svn_dirent_basename(local_abspath, scratch_pool);
  svn_boolean_t is_wc_root;
  svn_node_kind_t kind;
  svn_boolean_t is_special;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(local_abspath));
  SVN_ERR_ASSERT(!copyfrom_url || (svn_uri_is_canonical(copyfrom_url,
                                                        scratch_pool)
                                   && SVN_IS_VALID_REVNUM(copyfrom_rev)));

  /* Check that the proposed node has an acceptable name. */
  if (svn_wc_is_adm_dir(base_name, scratch_pool))
    return svn_error_createf
      (SVN_ERR_ENTRY_FORBIDDEN, NULL,
       _("Can't create an entry with a reserved name while trying to add '%s'"),
       svn_dirent_local_style(local_abspath, scratch_pool));

  SVN_ERR(svn_path_check_valid(local_abspath, scratch_pool));

  /* Make sure something's there; set KIND and *KIND_P. */
  SVN_ERR(svn_io_check_special_path(local_abspath, &kind, &is_special,
                                    scratch_pool));
  if (kind == svn_node_none)
    return svn_error_createf(SVN_ERR_WC_PATH_NOT_FOUND, NULL,
                             _("'%s' not found"),
                             svn_dirent_local_style(local_abspath,
                                                    scratch_pool));
  if (kind == svn_node_unknown)
    return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                             _("Unsupported node kind for path '%s'"),
                             svn_dirent_local_style(local_abspath,
                                                    scratch_pool));
  if (kind_p)
    *kind_p = kind;

  /* Determine whether a DB row for this node EXISTS, and whether it
     IS_WC_ROOT.  If it exists, check that it is in an acceptable state for
     adding the new node; if not, return an error. */
  {
    svn_wc__db_status_t status;
    svn_boolean_t conflicted;
    svn_boolean_t exists;
    svn_error_t *err
      = svn_wc__db_read_info(&status, NULL, NULL, NULL, NULL, NULL, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL,
                             &conflicted,
                             NULL, NULL, NULL, NULL, NULL, NULL,
                             db, local_abspath,
                             scratch_pool, scratch_pool);

    if (err)
      {
        if (err->apr_err != SVN_ERR_WC_PATH_NOT_FOUND)
          return svn_error_trace(err);

        svn_error_clear(err);
        exists = FALSE;
        is_wc_root = FALSE;
      }
    else
      {
        is_wc_root = FALSE;
        exists = TRUE;

        /* Note that the node may be in conflict even if it does not
         * exist on disk (certain tree conflict scenarios). */
        if (conflicted)
          return svn_error_createf(SVN_ERR_WC_FOUND_CONFLICT, NULL,
                                   _("'%s' is an existing item in conflict; "
                                   "please mark the conflict as resolved "
                                   "before adding a new item here"),
                                   svn_dirent_local_style(local_abspath,
                                                          scratch_pool));
        switch (status)
          {
            case svn_wc__db_status_not_present:
              break;
            case svn_wc__db_status_deleted:
              /* A working copy root should never have a WORKING_NODE */
              SVN_ERR_ASSERT(!is_wc_root);
              break;
            case svn_wc__db_status_normal:
              SVN_ERR(svn_wc__db_is_wcroot(&is_wc_root, db, local_abspath,
                                           scratch_pool));

              if (is_wc_root && copyfrom_url)
                {
                  /* Integrate a sub working copy in a parent working copy
                     (legacy behavior) */
                  break;
                }
              else if (is_wc_root && is_special)
                {
                  /* Adding a symlink to a working copy root.
                     (special_tests.py 23: externals as symlink targets) */
                  break;
                }
              /* else: Fall through in default error */

            default:
              return svn_error_createf(
                               SVN_ERR_ENTRY_EXISTS, NULL,
                               _("'%s' is already under version control"),
                               svn_dirent_local_style(local_abspath,
                                                      scratch_pool));
          }
      } /* err */

    if (db_row_exists_p)
      *db_row_exists_p = exists;
    if (is_wc_root_p)
      *is_wc_root_p = is_wc_root;
  }

  return SVN_NO_ERROR;
}


/* Convert the nested pristine working copy rooted at LOCAL_ABSPATH into
 * a copied subtree in the outer working copy.
 *
 * LOCAL_ABSPATH must be the root of a nested working copy that has no
 * local modifications.  The parent directory of LOCAL_ABSPATH must be a
 * versioned directory in the outer WC, and must belong to the same
 * repository as the nested WC.  The nested WC will be integrated into the
 * parent's WC, and will no longer be a separate WC. */
static svn_error_t *
integrate_nested_wc_as_copy(svn_wc_context_t *wc_ctx,
                            const char *local_abspath,
                            apr_pool_t *scratch_pool)
{
  svn_wc__db_t *db = wc_ctx->db;
  const char *moved_abspath;

  /* Drop any references to the wc that is to be rewritten */
  SVN_ERR(svn_wc__db_drop_root(db, local_abspath, scratch_pool));

  /* Move the admin dir from the wc to a temporary location: MOVED_ABSPATH */
  {
    const char *tmpdir_abspath;
    const char *moved_adm_abspath;
    const char *adm_abspath;

    SVN_ERR(svn_wc__db_temp_wcroot_tempdir(&tmpdir_abspath, db,
                                           svn_dirent_dirname(local_abspath,
                                                              scratch_pool),
                                           scratch_pool, scratch_pool));
    SVN_ERR(svn_io_open_unique_file3(NULL, &moved_abspath, tmpdir_abspath,
                                     svn_io_file_del_on_close,
                                     scratch_pool, scratch_pool));
    SVN_ERR(svn_io_dir_make(moved_abspath, APR_OS_DEFAULT, scratch_pool));

    adm_abspath = svn_wc__adm_child(local_abspath, "", scratch_pool);
    moved_adm_abspath = svn_wc__adm_child(moved_abspath, "", scratch_pool);
    SVN_ERR(svn_io_file_move(adm_abspath, moved_adm_abspath, scratch_pool));
  }

  /* Copy entries from temporary location into the main db */
  SVN_ERR(svn_wc_copy3(wc_ctx, moved_abspath, local_abspath,
                       TRUE /* metadata_only */,
                       NULL, NULL, NULL, NULL, scratch_pool));

  /* Cleanup the temporary admin dir */
  SVN_ERR(svn_wc__db_drop_root(db, moved_abspath, scratch_pool));
  SVN_ERR(svn_io_remove_dir2(moved_abspath, FALSE, NULL, NULL,
                             scratch_pool));

  /* The subdir is now part of our parent working copy. Our caller assumes
     that we return the new node locked, so obtain a lock if we didn't
     receive the lock via our depth infinity lock */
  {
    svn_boolean_t owns_lock;

    SVN_ERR(svn_wc__db_wclock_owns_lock(&owns_lock, db, local_abspath,
                                        FALSE, scratch_pool));
    if (!owns_lock)
      SVN_ERR(svn_wc__db_wclock_obtain(db, local_abspath, 0, FALSE,
                                       scratch_pool));
  }

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc_add4(svn_wc_context_t *wc_ctx,
            const char *local_abspath,
            svn_depth_t depth,
            const char *copyfrom_url,
            svn_revnum_t copyfrom_rev,
            svn_cancel_func_t cancel_func,
            void *cancel_baton,
            svn_wc_notify_func2_t notify_func,
            void *notify_baton,
            apr_pool_t *scratch_pool)
{
  svn_wc__db_t *db = wc_ctx->db;
  svn_node_kind_t kind;
  svn_boolean_t db_row_exists;
  svn_boolean_t is_wc_root;
  const char *repos_root_url;
  const char *repos_uuid;

  SVN_ERR(check_can_add_node(&kind, &db_row_exists, &is_wc_root,
                             db, local_abspath, copyfrom_url, copyfrom_rev,
                             scratch_pool));

  /* Get REPOS_ROOT_URL and REPOS_UUID.  Check that the
     parent is a versioned directory in an acceptable state. */
  SVN_ERR(check_can_add_to_parent(&repos_root_url, &repos_uuid,
                                  db, local_abspath, scratch_pool,
                                  scratch_pool));

  /* If we're performing a repos-to-WC copy, check that the copyfrom
     repository is the same as the parent dir's repository. */
  if (copyfrom_url && !svn_uri__is_ancestor(repos_root_url, copyfrom_url))
    return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                             _("The URL '%s' has a different repository "
                               "root than its parent"), copyfrom_url);

  /* Verify that we can actually integrate the inner working copy */
  if (is_wc_root)
    {
      const char *repos_relpath, *inner_repos_root_url, *inner_repos_uuid;
      const char *inner_url;

      SVN_ERR(svn_wc__db_scan_base_repos(&repos_relpath,
                                         &inner_repos_root_url,
                                         &inner_repos_uuid,
                                         db, local_abspath,
                                         scratch_pool, scratch_pool));

      if (strcmp(inner_repos_uuid, repos_uuid)
          || strcmp(repos_root_url, inner_repos_root_url))
        return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                                 _("Can't schedule the working copy at '%s' "
                                   "from repository '%s' with uuid '%s' "
                                   "for addition under a working copy from "
                                   "repository '%s' with uuid '%s'."),
                                 svn_dirent_local_style(local_abspath,
                                                        scratch_pool),
                                 inner_repos_root_url, inner_repos_uuid,
                                 repos_root_url, repos_uuid);

      inner_url = svn_path_url_add_component2(repos_root_url, repos_relpath,
                                              scratch_pool);

      if (strcmp(copyfrom_url, inner_url))
        return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                                 _("Can't add '%s' with URL '%s', but with "
                                   "the data from '%s'"),
                                 svn_dirent_local_style(local_abspath,
                                                        scratch_pool),
                                 copyfrom_url, inner_url);
    }

  if (!copyfrom_url)  /* Case 2a: It's a simple add */
    {
      SVN_ERR(add_from_disk(db, local_abspath, kind, NULL,
                            scratch_pool));
      if (kind == svn_node_dir && !db_row_exists)
        {
          /* If using the legacy 1.6 interface the parent lock may not
             be recursive and add is expected to lock the new dir.

             ### Perhaps the lock should be created in the same
             transaction that adds the node? */
          svn_boolean_t owns_lock;

          SVN_ERR(svn_wc__db_wclock_owns_lock(&owns_lock, db, local_abspath,
                                              FALSE, scratch_pool));
          if (!owns_lock)
            SVN_ERR(svn_wc__db_wclock_obtain(db, local_abspath, 0, FALSE,
                                             scratch_pool));
        }
    }
  else if (!is_wc_root)  /* Case 2b: It's a copy from the repository */
    {
      if (kind == svn_node_file)
        {
          /* This code should never be used, as it doesn't install proper
             pristine and/or properties. But it was not an error in the old
             version of this function.

             ===> Use svn_wc_add_repos_file4() directly! */
          svn_stream_t *content = svn_stream_empty(scratch_pool);

          SVN_ERR(svn_wc_add_repos_file4(wc_ctx, local_abspath,
                                         content, NULL, NULL, NULL,
                                         copyfrom_url, copyfrom_rev,
                                         cancel_func, cancel_baton,
                                         scratch_pool));
        }
      else
        {
          const char *repos_relpath =
            svn_uri_skip_ancestor(repos_root_url, copyfrom_url, scratch_pool);

          SVN_ERR(svn_wc__db_op_copy_dir(db, local_abspath,
                                         apr_hash_make(scratch_pool),
                                         copyfrom_rev, 0, NULL,
                                         repos_relpath,
                                         repos_root_url, repos_uuid,
                                         copyfrom_rev,
                                         NULL /* children */, FALSE, depth,
                                         NULL /* conflicts */,
                                         NULL /* work items */,
                                         scratch_pool));
        }
    }
  else  /* Case 1: Integrating a separate WC into this one, in place */
    {
      SVN_ERR(integrate_nested_wc_as_copy(wc_ctx, local_abspath,
                                          scratch_pool));
    }

  /* Report the addition to the caller. */
  if (notify_func != NULL)
    {
      svn_wc_notify_t *notify = svn_wc_create_notify(local_abspath,
                                                     svn_wc_notify_add,
                                                     scratch_pool);
      notify->kind = kind;
      (*notify_func)(notify_baton, notify, scratch_pool);
    }

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc_add_from_disk2(svn_wc_context_t *wc_ctx,
                      const char *local_abspath,
                      const apr_hash_t *props,
                      svn_wc_notify_func2_t notify_func,
                      void *notify_baton,
                      apr_pool_t *scratch_pool)
{
  svn_node_kind_t kind;

  SVN_ERR(check_can_add_node(&kind, NULL, NULL, wc_ctx->db, local_abspath,
                             NULL, SVN_INVALID_REVNUM, scratch_pool));
  SVN_ERR(check_can_add_to_parent(NULL, NULL, wc_ctx->db, local_abspath,
                                  scratch_pool, scratch_pool));

  /* Canonicalize and check the props */
  if (props)
    {
      apr_hash_t *new_props;

      SVN_ERR(svn_wc__canonicalize_props(
                &new_props,
                local_abspath, kind, props, FALSE /* skip_some_checks */,
                scratch_pool, scratch_pool));
      props = new_props;
    }

  /* Add to the DB and maybe update on-disk executable read-only bits */
  SVN_ERR(add_from_disk(wc_ctx->db, local_abspath, kind, props,
                        scratch_pool));

  /* Report the addition to the caller. */
  if (notify_func != NULL)
    {
      svn_wc_notify_t *notify = svn_wc_create_notify(local_abspath,
                                                     svn_wc_notify_add,
                                                     scratch_pool);
      notify->kind = kind;
      notify->mime_type = svn_prop_get_value(props, SVN_PROP_MIME_TYPE);
      (*notify_func)(notify_baton, notify, scratch_pool);
    }

  return SVN_NO_ERROR;
}

/* Return a path where nothing exists on disk, within the admin directory
   belonging to the WCROOT_ABSPATH directory.  */
static const char *
nonexistent_path(const char *wcroot_abspath, apr_pool_t *scratch_pool)
{
  return svn_wc__adm_child(wcroot_abspath, SVN_WC__ADM_NONEXISTENT_PATH,
                           scratch_pool);
}


svn_error_t *
svn_wc_get_pristine_copy_path(const char *path,
                              const char **pristine_path,
                              apr_pool_t *pool)
{
  svn_wc__db_t *db;
  const char *local_abspath;
  svn_error_t *err;

  SVN_ERR(svn_dirent_get_absolute(&local_abspath, path, pool));

  SVN_ERR(svn_wc__db_open(&db, NULL, FALSE, TRUE, pool, pool));
  /* DB is now open. This is seemingly a "light" function that a caller
     may use repeatedly despite error return values. The rest of this
     function should aggressively close DB, even in the error case.  */

  err = svn_wc__text_base_path_to_read(pristine_path, db, local_abspath,
                                       pool, pool);
  if (err && err->apr_err == SVN_ERR_WC_PATH_UNEXPECTED_STATUS)
    {
      /* The node doesn't exist, so return a non-existent path located
         in WCROOT/.svn/  */
      const char *wcroot_abspath;

      svn_error_clear(err);

      err = svn_wc__db_get_wcroot(&wcroot_abspath, db, local_abspath,
                                  pool, pool);
      if (err == NULL)
        *pristine_path = nonexistent_path(wcroot_abspath, pool);
    }

   return svn_error_compose_create(err, svn_wc__db_close(db));
}


svn_error_t *
svn_wc_get_pristine_contents2(svn_stream_t **contents,
                              svn_wc_context_t *wc_ctx,
                              const char *local_abspath,
                              apr_pool_t *result_pool,
                              apr_pool_t *scratch_pool)
{
  return svn_error_trace(svn_wc__get_pristine_contents(contents, NULL,
                                                       wc_ctx->db,
                                                       local_abspath,
                                                       result_pool,
                                                       scratch_pool));
}


typedef struct get_pristine_lazyopen_baton_t
{
  svn_wc_context_t *wc_ctx;
  const char *wri_abspath;
  const svn_checksum_t *checksum;

} get_pristine_lazyopen_baton_t;


/* Implements svn_stream_lazyopen_func_t */
static svn_error_t *
get_pristine_lazyopen_func(svn_stream_t **stream,
                           void *baton,
                           apr_pool_t *result_pool,
                           apr_pool_t *scratch_pool)
{
  get_pristine_lazyopen_baton_t *b = baton;
  const svn_checksum_t *sha1_checksum;

  /* svn_wc__db_pristine_read() wants a SHA1, so if we have an MD5,
     we'll use it to lookup the SHA1. */
  if (b->checksum->kind == svn_checksum_sha1)
    sha1_checksum = b->checksum;
  else
    SVN_ERR(svn_wc__db_pristine_get_sha1(&sha1_checksum, b->wc_ctx->db,
                                         b->wri_abspath, b->checksum,
                                         scratch_pool, scratch_pool));

  SVN_ERR(svn_wc__db_pristine_read(stream, NULL, b->wc_ctx->db,
                                   b->wri_abspath, sha1_checksum,
                                   result_pool, scratch_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__get_pristine_contents_by_checksum(svn_stream_t **contents,
                                          svn_wc_context_t *wc_ctx,
                                          const char *wri_abspath,
                                          const svn_checksum_t *checksum,
                                          apr_pool_t *result_pool,
                                          apr_pool_t *scratch_pool)
{
  svn_boolean_t present;

  *contents = NULL;

  SVN_ERR(svn_wc__db_pristine_check(&present, wc_ctx->db, wri_abspath,
                                    checksum, scratch_pool));

  if (present)
    {
      get_pristine_lazyopen_baton_t *gpl_baton;

      gpl_baton = apr_pcalloc(result_pool, sizeof(*gpl_baton));
      gpl_baton->wc_ctx = wc_ctx;
      gpl_baton->wri_abspath = wri_abspath;
      gpl_baton->checksum = checksum;

      *contents = svn_stream_lazyopen_create(get_pristine_lazyopen_func,
                                             gpl_baton, FALSE, result_pool);
    }

  return SVN_NO_ERROR;
}



svn_error_t *
svn_wc_add_lock2(svn_wc_context_t *wc_ctx,
                 const char *local_abspath,
                 const svn_lock_t *lock,
                 apr_pool_t *scratch_pool)
{
  svn_wc__db_lock_t db_lock;
  svn_error_t *err;
  const svn_string_t *needs_lock;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(local_abspath));

  /* ### Enable after fixing callers */
  /*SVN_ERR(svn_wc__write_check(wc_ctx->db,
                              svn_dirent_dirname(local_abspath, scratch_pool),
                              scratch_pool));*/

  db_lock.token = lock->token;
  db_lock.owner = lock->owner;
  db_lock.comment = lock->comment;
  db_lock.date = lock->creation_date;
  err = svn_wc__db_lock_add(wc_ctx->db, local_abspath, &db_lock, scratch_pool);
  if (err)
    {
      if (err->apr_err != SVN_ERR_WC_PATH_NOT_FOUND)
        return svn_error_trace(err);

      /* Remap the error.  */
      svn_error_clear(err);
      return svn_error_createf(SVN_ERR_ENTRY_NOT_FOUND, NULL,
                               _("'%s' is not under version control"),
                               svn_dirent_local_style(local_abspath,
                                                      scratch_pool));
    }

  /* if svn:needs-lock is present, then make the file read-write. */
  err = svn_wc__internal_propget(&needs_lock, wc_ctx->db, local_abspath,
                                 SVN_PROP_NEEDS_LOCK, scratch_pool,
                                 scratch_pool);

  if (err && err->apr_err == SVN_ERR_WC_PATH_UNEXPECTED_STATUS)
    {
      /* The node has non wc representation (e.g. deleted), so
         we don't want to touch the in-wc file */
      svn_error_clear(err);
      return SVN_NO_ERROR;
    }
  SVN_ERR(err);

  if (needs_lock)
    SVN_ERR(svn_io_set_file_read_write(local_abspath, FALSE, scratch_pool));

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc_remove_lock2(svn_wc_context_t *wc_ctx,
                    const char *local_abspath,
                    apr_pool_t *scratch_pool)
{
  svn_error_t *err;
  const svn_string_t *needs_lock;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(local_abspath));

  /* ### Enable after fixing callers */
  /*SVN_ERR(svn_wc__write_check(wc_ctx->db,
                              svn_dirent_dirname(local_abspath, scratch_pool),
                              scratch_pool));*/

  err = svn_wc__db_lock_remove(wc_ctx->db, local_abspath, scratch_pool);
  if (err)
    {
      if (err->apr_err != SVN_ERR_WC_PATH_NOT_FOUND)
        return svn_error_trace(err);

      /* Remap the error.  */
      svn_error_clear(err);
      return svn_error_createf(SVN_ERR_ENTRY_NOT_FOUND, NULL,
                               _("'%s' is not under version control"),
                               svn_dirent_local_style(local_abspath,
                                                      scratch_pool));
    }

  /* if svn:needs-lock is present, then make the file read-only. */
  err = svn_wc__internal_propget(&needs_lock, wc_ctx->db, local_abspath,
                                 SVN_PROP_NEEDS_LOCK, scratch_pool,
                                 scratch_pool);
  if (err)
    {
      if (err->apr_err != SVN_ERR_WC_PATH_UNEXPECTED_STATUS)
        return svn_error_trace(err);

      svn_error_clear(err);
      return SVN_NO_ERROR; /* Node is shadowed and/or deleted,
                              so we shouldn't apply its lock */
    }

  if (needs_lock)
    SVN_ERR(svn_io_set_file_read_only(local_abspath, FALSE, scratch_pool));

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc_set_changelist2(svn_wc_context_t *wc_ctx,
                       const char *local_abspath,
                       const char *new_changelist,
                       svn_depth_t depth,
                       const apr_array_header_t *changelist_filter,
                       svn_cancel_func_t cancel_func,
                       void *cancel_baton,
                       svn_wc_notify_func2_t notify_func,
                       void *notify_baton,
                       apr_pool_t *scratch_pool)
{
  /* Assert that we aren't being asked to set an empty changelist. */
  SVN_ERR_ASSERT(! (new_changelist && new_changelist[0] == '\0'));

  SVN_ERR_ASSERT(svn_dirent_is_absolute(local_abspath));

  SVN_ERR(svn_wc__db_op_set_changelist(wc_ctx->db, local_abspath,
                                       new_changelist, changelist_filter,
                                       depth, notify_func, notify_baton,
                                       cancel_func, cancel_baton,
                                       scratch_pool));

  return SVN_NO_ERROR;
}

struct get_cl_fn_baton
{
  svn_wc__db_t *db;
  apr_hash_t *clhash;
  svn_changelist_receiver_t callback_func;
  void *callback_baton;
};


static svn_error_t *
get_node_changelist(const char *local_abspath,
                    svn_node_kind_t kind,
                    void *baton,
                    apr_pool_t *scratch_pool)
{
  struct get_cl_fn_baton *b = baton;
  const char *changelist;

  SVN_ERR(svn_wc__db_read_info(NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                               NULL, NULL, NULL, NULL, NULL, &changelist,
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                               b->db, local_abspath,
                               scratch_pool, scratch_pool));

  if (svn_wc__internal_changelist_match(b->db, local_abspath, b->clhash,
                                        scratch_pool))
    SVN_ERR(b->callback_func(b->callback_baton, local_abspath,
                             changelist, scratch_pool));

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc_get_changelists(svn_wc_context_t *wc_ctx,
                       const char *local_abspath,
                       svn_depth_t depth,
                       const apr_array_header_t *changelist_filter,
                       svn_changelist_receiver_t callback_func,
                       void *callback_baton,
                       svn_cancel_func_t cancel_func,
                       void *cancel_baton,
                       apr_pool_t *scratch_pool)
{
  struct get_cl_fn_baton gnb;

  gnb.db = wc_ctx->db;
  gnb.clhash = NULL;
  gnb.callback_func = callback_func;
  gnb.callback_baton = callback_baton;

  if (changelist_filter)
    SVN_ERR(svn_hash_from_cstring_keys(&gnb.clhash, changelist_filter,
                                       scratch_pool));

  return svn_error_trace(
    svn_wc__internal_walk_children(wc_ctx->db, local_abspath, FALSE,
                                   changelist_filter, get_node_changelist,
                                   &gnb, depth,
                                   cancel_func, cancel_baton,
                                   scratch_pool));

}


svn_boolean_t
svn_wc__internal_changelist_match(svn_wc__db_t *db,
                                  const char *local_abspath,
                                  const apr_hash_t *clhash,
                                  apr_pool_t *scratch_pool)
{
  svn_error_t *err;
  const char *changelist;

  if (clhash == NULL)
    return TRUE;

  err = svn_wc__db_read_info(NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                             NULL, NULL, NULL, NULL, NULL, &changelist,
                             NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                             db, local_abspath, scratch_pool, scratch_pool);
  if (err)
    {
      svn_error_clear(err);
      return FALSE;
    }

  return (changelist
            && svn_hash_gets((apr_hash_t *)clhash, changelist) != NULL);
}


svn_boolean_t
svn_wc__changelist_match(svn_wc_context_t *wc_ctx,
                         const char *local_abspath,
                         const apr_hash_t *clhash,
                         apr_pool_t *scratch_pool)
{
  return svn_wc__internal_changelist_match(wc_ctx->db, local_abspath, clhash,
                                           scratch_pool);
}
