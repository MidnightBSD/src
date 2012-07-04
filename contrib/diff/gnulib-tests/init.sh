# source this file; set up for tests

# Copyright (C) 2009, 2010 Free Software Foundation, Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Using this file in a test
# =========================
#
# The typical skeleton of a test looks like this:
#
#   #!/bin/sh
#   : ${srcdir=.}
#   . "$srcdir/init.sh"; path_prepend_ .
#   Execute some commands.
#   Note that these commands are executed in a subdirectory, therefore you
#   need to prepend "../" to relative filenames in the build directory.
#   Set the exit code 0 for success, 77 for skipped, or 1 or other for failure.
#   Use the skip_ and fail_ functions to print a diagnostic and then exit
#   with the corresponding exit code.
#   Exit $?

# Executing a test that uses this file
# ====================================
#
# Running a single test:
#   $ make check TESTS=test-foo.sh
#
# Running a single test, with verbose output:
#   $ make check TESTS=test-foo.sh VERBOSE=yes
#
# Running a single test, with single-stepping:
#   1. Go into a sub-shell:
#   $ bash
#   2. Set relevant environment variables from TESTS_ENVIRONMENT in the
#      Makefile:
#   $ export srcdir=../../tests # this is an example
#   3. Execute the commands from the test, copy&pasting them one by one:
#   $ . "$srcdir/init.sh"; path_prepend_ .
#   ...
#   4. Finally
#   $ exit

# We use a trap below for cleanup.  This requires us to go through
# hoops to get the right exit status transported through the handler.
# So use `Exit STATUS' instead of `exit STATUS' inside of the tests.
# Turn off errexit here so that we don't trip the bug with OSF1/Tru64
# sh inside this function.
Exit () { set +e; (exit $1); exit $1; }

fail_() { echo "$ME_: failed test: $@" 1>&2; Exit 1; }
skip_() { echo "$ME_: skipped test: $@" 1>&2; Exit 77; }

# This is a stub function that is run upon trap (upon regular exit and
# interrupt).  Override it with a per-test function, e.g., to unmount
# a partition, or to undo any other global state changes.
cleanup_() { :; }

if ( diff --version < /dev/null 2>&1 | grep GNU ) 2>&1 > /dev/null; then
  compare() { diff -u "$@"; }
elif ( cmp --version < /dev/null 2>&1 | grep GNU ) 2>&1 > /dev/null; then
  compare() { cmp -s "$@"; }
else
  compare() { cmp "$@"; }
fi

# An arbitrary prefix to help distinguish test directories.
testdir_prefix_() { printf gt; }

# Run the user-overridable cleanup_ function, remove the temporary
# directory and exit with the incoming value of $?.
remove_tmp_()
{
  __st=$?
  cleanup_
  # cd out of the directory we're about to remove
  cd "$initial_cwd_" || cd / || cd /tmp
  chmod -R u+rwx "$test_dir_"
  # If removal fails and exit status was to be 0, then change it to 1.
  rm -rf "$test_dir_" || { test $__st = 0 && __st=1; }
  exit $__st
}

# Use this function to prepend to PATH an absolute name for each
# specified, possibly-$initial_cwd_relative, directory.
path_prepend_()
{
  while test $# != 0; do
    path_dir_=$1
    case $path_dir_ in
      '') fail_ "invalid path dir: '$1'";;
      /*) abs_path_dir_=$path_dir_;;
      *) abs_path_dir_=`cd "$initial_cwd_/$path_dir_" && echo "$PWD"` \
           || fail_ "invalid path dir: $path_dir_";;
    esac
    case $abs_path_dir_ in
      *:*) fail_ "invalid path dir: '$abs_path_dir_'";;
    esac
    PATH="$abs_path_dir_:$PATH"
    shift
  done
  export PATH
}

setup_()
{
  test "$VERBOSE" = yes && set -x

  initial_cwd_=$PWD
  ME_=`expr "./$0" : '.*/\(.*\)$'`

  pfx_=`testdir_prefix_`
  test_dir_=`mktempd_ "$initial_cwd_" "$pfx_-$ME_.XXXX"` \
    || fail_ "failed to create temporary directory in $initial_cwd_"
  cd "$test_dir_"

  # This pair of trap statements ensures that the temporary directory,
  # $test_dir_, is removed upon exit as well as upon catchable signal.
  trap remove_tmp_ 0
  trap 'Exit $?' 1 2 13 15
}

# Create a temporary directory, much like mktemp -d does.
# Written by Jim Meyering.
#
# Usage: mktempd_ /tmp phoey.XXXXXXXXXX
#
# First, try to use the mktemp program.
# Failing that, we'll roll our own mktemp-like function:
#  - try to get random bytes from /dev/urandom
#  - failing that, generate output from a combination of quickly-varying
#      sources and gzip.  Ignore non-varying gzip header, and extract
#      "random" bits from there.
#  - given those bits, map to file-name bytes using tr, and try to create
#      the desired directory.
#  - make only $MAX_TRIES_ attempts

# Helper function.  Print $N pseudo-random bytes from a-zA-Z0-9.
rand_bytes_()
{
  n_=$1

  # Maybe try openssl rand -base64 $n_prime_|tr '+/=\012' abcd first?
  # But if they have openssl, they probably have mktemp, too.

  chars_=abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789
  dev_rand_=/dev/urandom
  if test -r "$dev_rand_"; then
    # Note: 256-length($chars_) == 194; 3 copies of $chars_ is 186 + 8 = 194.
    dd ibs=$n_ count=1 if=$dev_rand_ 2>/dev/null \
      | tr -c $chars_ 01234567$chars_$chars_$chars_
    return
  fi

  n_plus_50_=`expr $n_ + 50`
  cmds_='date; date +%N; free; who -a; w; ps auxww; ps ef; netstat -n'
  data_=` (eval "$cmds_") 2>&1 | gzip `

  # Ensure that $data_ has length at least 50+$n_
  while :; do
    len_=`echo "$data_"|wc -c`
    test $n_plus_50_ -le $len_ && break;
    data_=` (echo "$data_"; eval "$cmds_") 2>&1 | gzip `
  done

  echo "$data_" \
    | dd bs=1 skip=50 count=$n_ 2>/dev/null \
    | tr -c $chars_ 01234567$chars_$chars_$chars_
}

mktempd_()
{
  case $# in
  2);;
  *) fail_ "Usage: $ME DIR TEMPLATE";;
  esac

  destdir_=$1
  template_=$2

  MAX_TRIES_=4

  # Disallow any trailing slash on specified destdir:
  # it would subvert the post-mktemp "case"-based destdir test.
  case $destdir_ in
  /) ;;
  */) fail_ "invalid destination dir: remove trailing slash(es)";;
  esac

  case $template_ in
  *XXXX) ;;
  *) fail_ "invalid template: $template_ (must have a suffix of at least 4 X's)";;
  esac

  fail=0

  # First, try to use mktemp.
  d=`env -u TMPDIR mktemp -d -t -p "$destdir_" "$template_" 2>/dev/null` \
    || fail=1

  # The resulting name must be in the specified directory.
  case $d in "$destdir_"*);; *) fail=1;; esac

  # It must have created the directory.
  test -d "$d" || fail=1

  # It must have 0700 permissions.  Handle sticky "S" bits.
  perms=`ls -dgo "$d" 2>/dev/null|tr S -` || fail=1
  case $perms in drwx------*) ;; *) fail=1;; esac

  test $fail = 0 && {
    echo "$d"
    return
  }

  # If we reach this point, we'll have to create a directory manually.

  # Get a copy of the template without its suffix of X's.
  base_template_=`echo "$template_"|sed 's/XX*$//'`

  # Calculate how many X's we've just removed.
  template_length_=`echo "$template_" | wc -c`
  nx_=`echo "$base_template_" | wc -c`
  nx_=`expr $template_length_ - $nx_`

  err_=
  i_=1
  while :; do
    X_=`rand_bytes_ $nx_`
    candidate_dir_="$destdir_/$base_template_$X_"
    err_=`mkdir -m 0700 "$candidate_dir_" 2>&1` \
      && { echo "$candidate_dir_"; return; }
    test $MAX_TRIES_ -le $i_ && break;
    i_=`expr $i_ + 1`
  done
  fail_ "$err_"
}

# If you want to override the testdir_prefix_ function,
# or to add more utility functions, use this file.
test -f "$srcdir/init.cfg" \
  && . "$srcdir/init.cfg"

setup_ "$@"
