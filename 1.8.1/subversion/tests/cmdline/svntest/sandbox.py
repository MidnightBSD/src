#
# sandbox.py :  tools for manipulating a test's working area ("a sandbox")
#
# ====================================================================
#    Licensed to the Apache Software Foundation (ASF) under one
#    or more contributor license agreements.  See the NOTICE file
#    distributed with this work for additional information
#    regarding copyright ownership.  The ASF licenses this file
#    to you under the Apache License, Version 2.0 (the
#    "License"); you may not use this file except in compliance
#    with the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing,
#    software distributed under the License is distributed on an
#    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#    KIND, either express or implied.  See the License for the
#    specific language governing permissions and limitations
#    under the License.
# ====================================================================
#

import os
import shutil
import copy
import urllib
import logging

import svntest

logger = logging.getLogger()


class Sandbox:
  """Manages a sandbox (one or more repository/working copy pairs) for
  a test to operate within."""

  dependents = None
  tmp_dir = None

  def __init__(self, module, idx):
    self.test_paths = []

    self._set_name("%s-%d" % (module, idx))
    # This flag is set to True by build() and returned by is_built()
    self._is_built = False

  def _set_name(self, name, read_only=False):
    """A convenience method for renaming a sandbox, useful when
    working with multiple repositories in the same unit test."""
    if not name is None:
      self.name = name
    self.read_only = read_only
    self.wc_dir = os.path.join(svntest.main.general_wc_dir, self.name)
    self.add_test_path(self.wc_dir)
    if not read_only:
      self.repo_dir = os.path.join(svntest.main.general_repo_dir, self.name)
      self.repo_url = (svntest.main.options.test_area_url + '/'
                       + urllib.pathname2url(self.repo_dir))
      self.add_test_path(self.repo_dir)
    else:
      self.repo_dir = svntest.main.pristine_greek_repos_dir
      self.repo_url = svntest.main.pristine_greek_repos_url

    ### TODO: Move this into to the build() method
    # For dav tests we need a single authz file which must be present,
    # so we recreate it each time a sandbox is created with some default
    # contents, making sure that an empty file is never present
    if self.repo_url.startswith("http"):
      # this dir doesn't exist out of the box, so we may have to make it
      if not os.path.exists(svntest.main.work_dir):
        os.makedirs(svntest.main.work_dir)
      self.authz_file = os.path.join(svntest.main.work_dir, "authz")
      tmp_authz_file = os.path.join(svntest.main.work_dir, "authz-" + self.name)
      open(tmp_authz_file, 'w').write("[/]\n* = rw\n")
      shutil.move(tmp_authz_file, self.authz_file)
      self.groups_file = os.path.join(svntest.main.work_dir, "groups")

    # For svnserve tests we have a per-repository authz file, and it
    # doesn't need to be there in order for things to work, so we don't
    # have any default contents.
    elif self.repo_url.startswith("svn"):
      self.authz_file = os.path.join(self.repo_dir, "conf", "authz")
      self.groups_file = os.path.join(self.repo_dir, "conf", "groups")

  def clone_dependent(self, copy_wc=False):
    """A convenience method for creating a near-duplicate of this
    sandbox, useful when working with multiple repositories in the
    same unit test.  If COPY_WC is true, make an exact copy of this
    sandbox's working copy at the new sandbox's working copy
    directory.  Any necessary cleanup operations are triggered by
    cleanup of the original sandbox."""

    if not self.dependents:
      self.dependents = []
    clone = copy.deepcopy(self)
    self.dependents.append(clone)
    clone._set_name("%s-%d" % (self.name, len(self.dependents)))
    if copy_wc:
      self.add_test_path(clone.wc_dir)
      shutil.copytree(self.wc_dir, clone.wc_dir, symlinks=True)
    return clone

  def build(self, name=None, create_wc=True, read_only=False,
            minor_version=None):
    """Make a 'Greek Tree' repo (or refer to the central one if READ_ONLY),
       and check out a WC from it (unless CREATE_WC is false). Change the
       sandbox's name to NAME. See actions.make_repo_and_wc() for details."""
    self._set_name(name, read_only)
    svntest.actions.make_repo_and_wc(self, create_wc, read_only, minor_version)
    self._is_built = True

  def authz_name(self, repo_dir=None):
    "return this sandbox's name for use in an authz file"
    repo_dir = repo_dir or self.repo_dir
    if self.repo_url.startswith("http"):
      return os.path.basename(repo_dir)
    else:
      return repo_dir.replace('\\', '/')

  def add_test_path(self, path, remove=True):
    self.test_paths.append(path)
    if remove:
      svntest.main.safe_rmtree(path)

  def add_repo_path(self, suffix, remove=True):
    """Generate a path, under the general repositories directory, with
       a name that ends in SUFFIX, e.g. suffix="2" -> ".../basic_tests.2".
       If REMOVE is true, remove anything currently on disk at that path.
       Remember that path so that the automatic clean-up mechanism can
       delete it at the end of the test. Generate a repository URL to
       refer to a repository at that path. Do not create a repository.
       Return (REPOS-PATH, REPOS-URL)."""
    path = (os.path.join(svntest.main.general_repo_dir, self.name)
            + '.' + suffix)
    url = svntest.main.options.test_area_url + \
                                        '/' + urllib.pathname2url(path)
    self.add_test_path(path, remove)
    return path, url

  def add_wc_path(self, suffix, remove=True):
    """Generate a path, under the general working copies directory, with
       a name that ends in SUFFIX, e.g. suffix="2" -> ".../basic_tests.2".
       If REMOVE is true, remove anything currently on disk at that path.
       Remember that path so that the automatic clean-up mechanism can
       delete it at the end of the test. Do not create a working copy.
       Return the generated WC-PATH."""
    path = self.wc_dir + '.' + suffix
    self.add_test_path(path, remove)
    return path

  tempname_offs = 0 # Counter for get_tempname

  def get_tempname(self, prefix='tmp'):
    """Get a stable name for a temporary file that will be removed after
       running the test"""

    if not self.tmp_dir:
      # Create an empty directory for temporary files
      self.tmp_dir = self.add_wc_path('tmp', remove=True)
      os.mkdir(self.tmp_dir)

    self.tempname_offs = self.tempname_offs + 1

    return os.path.join(self.tmp_dir, '%s-%s' % (prefix, self.tempname_offs))

  def cleanup_test_paths(self):
    "Clean up detritus from this sandbox, and any dependents."
    if self.dependents:
      # Recursively cleanup any dependent sandboxes.
      for sbox in self.dependents:
        sbox.cleanup_test_paths()
    # cleanup all test specific working copies and repositories
    for path in self.test_paths:
      if not path is svntest.main.pristine_greek_repos_dir:
        _cleanup_test_path(path)

  def is_built(self):
    "Returns True when build() has been called on this instance."
    return self._is_built

  def ospath(self, relpath, wc_dir=None):
    """Return RELPATH converted to an OS-style path relative to the WC dir
       of this sbox, or relative to OS-style path WC_DIR if supplied."""
    if wc_dir is None:
      wc_dir = self.wc_dir
    return os.path.join(wc_dir, svntest.wc.to_ospath(relpath))

  def ospaths(self, relpaths, wc_dir=None):
    """Return a list of RELPATHS but with each path converted to an OS-style
       path relative to the WC dir of this sbox, or relative to OS-style
       path WC_DIR if supplied."""
    return [self.ospath(rp, wc_dir) for rp in relpaths]

  def path(self, relpath, wc_dir=None):
    """Return RELPATH converted to an path relative to the WC dir
       of this sbox, or relative to WC_DIR if supplied, but always
       using '/' as directory separator."""
    return self.ospath(relpath, wc_dir=wc_dir).replace(os.path.sep, '/')

  def redirected_root_url(self, temporary=False):
    """If TEMPORARY is set, return the URL which should be configured
       to temporarily redirect to the root of this repository;
       otherwise, return the URL which should be configured to
       permanent redirect there.  (Assumes that the sandbox is not
       read-only.)"""
    assert not self.read_only
    assert self.repo_url.startswith("http")
    parts = self.repo_url.rsplit('/', 1)
    return '%s/REDIRECT-%s-%s' % (parts[0],
                                  temporary and 'TEMP' or 'PERM',
                                  parts[1])

  def simple_update(self, target=None, revision='HEAD'):
    """Update the WC or TARGET.
       TARGET is a relpath relative to the WC."""
    if target is None:
      target = self.wc_dir
    else:
      target = self.ospath(target)
    svntest.main.run_svn(False, 'update', target, '-r', revision)

  def simple_switch(self, url, target=None):
    """Switch the WC or TARGET to URL.
       TARGET is a relpath relative to the WC."""
    if target is None:
      target = self.wc_dir
    else:
      target = self.ospath(target)
    svntest.main.run_svn(False, 'switch', url, target, '--ignore-ancestry')

  def simple_commit(self, target=None, message=None):
    """Commit the WC or TARGET, with a default or supplied log message.
       Raise if the exit code is non-zero or there is output on stderr.
       TARGET is a relpath relative to the WC."""
    assert not self.read_only
    if target is None:
      target = self.wc_dir
    else:
      target = self.ospath(target)
    if message is None:
      message = svntest.main.make_log_msg()
    svntest.actions.run_and_verify_commit(self.wc_dir, None, None, [],
                                          '-m', message, target)

  def simple_rm(self, *targets):
    """Schedule TARGETS for deletion.
       TARGETS are relpaths relative to the WC."""
    assert len(targets) > 0
    targets = self.ospaths(targets)
    svntest.main.run_svn(False, 'rm', *targets)

  def simple_mkdir(self, *targets):
    """Create TARGETS as directories scheduled for addition.
       TARGETS are relpaths relative to the WC."""
    assert len(targets) > 0
    targets = self.ospaths(targets)
    svntest.main.run_svn(False, 'mkdir', *targets)

  def simple_add(self, *targets):
    """Schedule TARGETS for addition.
       TARGETS are relpaths relative to the WC."""
    assert len(targets) > 0
    targets = self.ospaths(targets)
    svntest.main.run_svn(False, 'add', *targets)

  def simple_revert(self, *targets):
    """Revert TARGETS.
       TARGETS are relpaths relative to the WC."""
    assert len(targets) > 0
    targets = self.ospaths(targets)
    svntest.main.run_svn(False, 'revert', *targets)

  def simple_propset(self, name, value, *targets):
    """Set property NAME to VALUE on TARGETS.
       TARGETS are relpaths relative to the WC."""
    assert len(targets) > 0
    targets = self.ospaths(targets)
    svntest.main.run_svn(False, 'propset', name, value, *targets)

  def simple_propdel(self, name, *targets):
    """Delete property NAME from TARGETS.
       TARGETS are relpaths relative to the WC."""
    assert len(targets) > 0
    targets = self.ospaths(targets)
    svntest.main.run_svn(False, 'propdel', name, *targets)

  def simple_propget(self, name, target):
    """Return the value of the property NAME on TARGET.
       TARGET is a relpath relative to the WC."""
    target = self.ospath(target)
    exit, out, err = svntest.main.run_svn(False, 'propget',
                                          '--strict', name, target)
    return ''.join(out)

  def simple_proplist(self, target):
    """Return a dictionary mapping property name to property value, of the
       properties on TARGET.
       TARGET is a relpath relative to the WC."""
    target = self.ospath(target)
    exit, out, err = svntest.main.run_svn(False, 'proplist',
                                          '--verbose', '--quiet', target)
    props = {}
    for line in out:
      line = line.rstrip('\r\n')
      if line[2] != ' ':  # property name
        name = line[2:]
        val = None
      elif line.startswith('    '):  # property value
        if val is None:
          val = line[4:]
        else:
          val += '\n' + line[4:]
        props[name] = val
      else:
        raise Exception("Unexpected line '" + line + "' in proplist output" + str(out))
    return props

  def simple_add_symlink(self, dest, target):
    """Create a symlink TARGET pointing to DEST and add it to subversion"""
    if svntest.main.is_posix_os():
      os.symlink(dest, self.ospath(target))
    else:
      svntest.main.file_write(self.ospath(target), "link %s" % dest)
    self.simple_add(target)
    if not svntest.main.is_posix_os():
      # '*' is evaluated on Windows
      self.simple_propset('svn:special', 'X', target)

  def simple_add_text(self, text, *targets):
    """Create files containing TEXT as TARGETS"""
    assert len(targets) > 0
    for target in targets:
       svntest.main.file_write(self.ospath(target), text, mode='wb')
    self.simple_add(*targets)

  def simple_copy(self, source, dest):
    """Copy SOURCE to DEST in the WC.
       SOURCE and DEST are relpaths relative to the WC."""
    source = self.ospath(source)
    dest = self.ospath(dest)
    svntest.main.run_svn(False, 'copy', source, dest)

  def simple_move(self, source, dest):
    """Move SOURCE to DEST in the WC.
       SOURCE and DEST are relpaths relative to the WC."""
    source = self.ospath(source)
    dest = self.ospath(dest)
    svntest.main.run_svn(False, 'move', source, dest)

  def simple_repo_copy(self, source, dest):
    """Copy SOURCE to DEST in the repository, committing the result with a
       default log message.
       SOURCE and DEST are relpaths relative to the repo root."""
    svntest.main.run_svn(False, 'copy', '-m', svntest.main.make_log_msg(),
                         self.repo_url + '/' + source,
                         self.repo_url + '/' + dest)

  def simple_append(self, dest, contents, truncate=False):
    """Append CONTENTS to file DEST, optionally truncating it first.
       DEST is a relpath relative to the WC."""
    open(self.ospath(dest), truncate and 'w' or 'a').write(contents)

  def simple_lock(self, *targets):
    """Lock TARGETS in the WC.
       TARGETS are relpaths relative to the WC."""
    assert len(targets) > 0
    targets = self.ospaths(targets)
    svntest.main.run_svn(False, 'lock', *targets)


def is_url(target):
  return (target.startswith('^/')
          or target.startswith('file://')
          or target.startswith('http://')
          or target.startswith('https://')
          or target.startswith('svn://')
          or target.startswith('svn+ssh://'))


_deferred_test_paths = []

def cleanup_deferred_test_paths():
  global _deferred_test_paths
  test_paths = _deferred_test_paths
  _deferred_test_paths = []
  for path in test_paths:
    _cleanup_test_path(path, True)


def _cleanup_test_path(path, retrying=False):
  if retrying:
    logger.info("CLEANUP: RETRY: %s", path)
  else:
    logger.info("CLEANUP: %s", path)

  try:
    svntest.main.safe_rmtree(path, retrying)
  except:
    logger.info("WARNING: cleanup failed, will try again later")
    _deferred_test_paths.append(path)
