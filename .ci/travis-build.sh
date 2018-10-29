#!/bin/sh
set -e -x

make -j2 $EXTRA

if [ "$TEST" != "NO" ]
then
  make tapfiles
  make -s test-results
fi
