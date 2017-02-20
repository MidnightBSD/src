#!/bin/sh

tmpfiles=""
trap 'rm -fr $tmpfiles' 1 2 3 15

tmpfiles="t-c-stack.tmp"
./test-c-stack${EXEEXT} 2> t-c-stack.tmp
case $? in
  77) cat t-c-stack.tmp >&2; (exit 77); exit 77 ;;
  1) ;;
  *) (exit 1); exit 1 ;;
esac
if grep 'stack overflow' t-c-stack.tmp >/dev/null ; then
  :
else
  (exit 1); exit 1
fi

rm -fr $tmpfiles

exit 0
