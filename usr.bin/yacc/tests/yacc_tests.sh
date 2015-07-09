#!/bin/sh
# $FreeBSD: stable/10/usr.bin/yacc/tests/yacc_tests.sh 272973 2014-10-11 23:31:44Z rodrigc $

set -e

# Setup the environment for run_test
# - run_test looks for `#define YYBTYACC` in ../config.h
# - run_test assumes a yacc binary exists in ../yacc instead of running "yacc"
# - run_test spams the test dir with files (polluting subsequent test runs),
#   so it's better to copy all the files to a temporary directory created by
#   kyua
echo > "./config.h"
mkdir "test"
cp -Rf "$(dirname "$0")"/* "test"
cp -p /usr/bin/yacc ./yacc

cd "test" && ./run_test
