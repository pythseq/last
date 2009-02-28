#! /bin/sh

# Exercise LAST programs, and compare the output to a reference
# output.  More tests should be added!

dir=$(dirname $0)/..

lastdb=$dir/src/lastdb
lastal=$dir/src/lastal
seq=$dir/test/galGal3-M-32.fa
mat=$dir/examples/HOXD70
ref=$dir/test/last-test.maf
db=/tmp/last-test

$lastdb -c -m110 $db $seq

$lastal -u2 -j5 -p $mat -x3400 -e2500 $db $seq | diff $ref -

rm $db*
