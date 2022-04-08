#!/bin/sh
#
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
#
# -*- mode: shell-script; -*-

# This script simplifies the preparation of the environment for a Subversion
# client communicating with an svnserve server.
#
# The script runs svnserve, runs "make check", and kills the svnserve
# afterwards.  It makes sure to kill the svnserve even if the test run dies.
#
# This script should be run from the top level of the Subversion
# distribution; it's easiest to just run it as "make svnserveautocheck".
# Like "make check", you can specify further options like
# "make svnserveautocheck FS_TYPE=bdb TESTS=subversion/tests/cmdline/basic.py".
#
# Other environment variables that can be passed:
#
#  make svnserveautocheck CACHE_REVPROPS=1   # run svnserve --cache-revprops
#
#  make svnserveautocheck BLOCK_READ=1       # run svnserve --block-read on
#
#  make svnserveautocheck THREADED=1         # run svnserve -T

PYTHON=${PYTHON:-python}

SCRIPTDIR=$(dirname $0)
SCRIPT=$(basename $0)

set +e

trap trap_cleanup HUP TERM INT

# Ensure the server uses a known locale.
LC_ALL=C
export LC_ALL

really_cleanup() {
    if [ -e  "$SVNSERVE_PID" ]; then
        kill $(cat "$SVNSERVE_PID")
        rm -f $SVNSERVE_PID
    fi
}

trap_cleanup() {
    really_cleanup
    exit 1
}

say() {
  echo "$SCRIPT: $*"
}

fail() {
  say $*
  exit 1
}

query() {
    printf "%s" "$SCRIPT: $1 (y/n)? [$2] "
    if [ -n "$BASH_VERSION" ]; then
        read -n 1 -t 32
    else
        #
        prog="
import select as s
import sys
import tty, termios
tty.setcbreak(sys.stdin.fileno(), termios.TCSANOW)
if s.select([sys.stdin.fileno()], [], [], 32)[0]:
  sys.stdout.write(sys.stdin.read(1))
"
        stty_state=`stty -g`
        REPLY=`$PYTHON -u -c "$prog" "$@"`
        stty $stty_state
    fi
    echo
    [ "${REPLY:-$2}" = 'y' ]
}

# Compute ABS_BUILDDIR and ABS_SRCDIR.
if [ -x subversion/svn/svn ]; then
  # cwd is build tree root
  ABS_BUILDDIR=$(pwd)
elif [ -x $SCRIPTDIR/../../svn/svn ]; then
  # cwd is subversion/tests/cmdline/ in the build tree
  cd $SCRIPTDIR/../../../
  ABS_BUILDDIR=$(pwd)
  cd - >/dev/null
else
  fail "Run this script from the root of Subversion's build tree!"
fi
# Cater for out-of-tree builds
ABS_SRCDIR=`<$ABS_BUILDDIR/Makefile sed -ne 's/^srcdir = //p'`
if [ ! -e $ABS_SRCDIR/subversion/include/svn_version.h ]; then
  fail "Run this script from the root of Subversion's build tree!"
fi

# Create a directory for the PID and log files. If you change this, also make
# sure to change the svn:ignore entry for it and "make check-clean".
SVNSERVE_ROOT="$ABS_BUILDDIR/subversion/tests/cmdline/svnserve-$(date '+%Y%m%d-%H%M%S')"
mkdir "$SVNSERVE_ROOT" \
    || fail "couldn't create temporary directory '$SVNSERVE_ROOT'"

# If you change this, also make sure to change the svn:ignore entry
# for it and "make check-clean".
SVNSERVE_PID=$SVNSERVE_ROOT/svnserve.pid
SVNSERVE_LOG=$SVNSERVE_ROOT/svnserve.log

SERVER_CMD="$ABS_BUILDDIR/subversion/svnserve/svnserve"

rm -f $SVNSERVE_PID
rm -f $SVNSERVE_LOG

random_port() {
  if [ -n "$BASH_VERSION" ]; then
    echo $(($RANDOM+1024))
  else
    $PYTHON -c 'import random; print(random.randint(1024, 2**16-1))'
  fi
}

if type time > /dev/null ; then TIME_CMD() { time "$@"; } ; else TIME_CMD() { "$@"; } ; fi

MAKE=${MAKE:-make}
PATH="$PATH:/usr/sbin/:/usr/local/sbin/"

ss > /dev/null 2>&1 || netstat > /dev/null 2>&1 || fail "unable to find ss or netstat required to find a free port"

SVNSERVE_PORT=$(random_port)
while \
  (ss -ltn sport = :$SVNSERVE_PORT 2>&1 | grep :$SVNSERVE_PORT > /dev/null ) \
  || \
  (netstat -an 2>&1 | grep $SVNSERVE_PORT | grep 'LISTEN' > /dev/null ) \
  do
  SVNSERVE_PORT=$(random_port)
done

if [ "$THREADED" != "" ]; then
  SVNSERVE_ARGS="-T"
fi

if [ ${CACHE_REVPROPS:+set} ]; then
  SVNSERVE_ARGS="$SVNSERVE_ARGS --cache-revprops on"
fi

if [ ${BLOCK_READ:+set} ]; then
  SVNSERVE_ARGS="$SVNSERVE_ARGS --block-read on"
fi

"$SERVER_CMD" -d -r "$ABS_BUILDDIR/subversion/tests/cmdline" \
            --listen-host 127.0.0.1 \
            --listen-port $SVNSERVE_PORT \
            --pid-file $SVNSERVE_PID \
            --log-file $SVNSERVE_LOG \
            $SVNSERVE_ARGS &

BASE_URL=svn://127.0.0.1:$SVNSERVE_PORT
if [ $# = 0 ]; then
  TIME_CMD "$MAKE" check "BASE_URL=$BASE_URL"
  r=$?
else
  cd "$ABS_BUILDDIR/subversion/tests/cmdline/"
  TEST="$1"
  shift
  TIME_CMD "$ABS_SRCDIR/subversion/tests/cmdline/${TEST}_tests.py" "--url=$BASE_URL" $*
  r=$?
  cd - > /dev/null
fi

query 'Browse server log' n \
    && less "$SVNSERVE_LOG"

query 'Delete svnserve root directory' y \
    && rm -fr "$SVNSERVE_ROOT/"

really_cleanup
exit $r
