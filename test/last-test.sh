#! /bin/sh

# Exercise LAST programs, and compare the output to a reference
# output.  More tests should be added!

cd $(dirname $0)
PATH=$PATH:../src

dnaSeq=galGal3-M-32.fa
protSeq=Q2LCP8.fa
mat=../examples/HOXD70
gc=../examples/vertebrateMito.gc
db=/tmp/last-test

{
    echo TEST 1  # spaced seeds, soft-masking, centroid alignment, matrix file
    lastdb -c -m110 $db $dnaSeq
    lastal -u2 -j5 -p $mat -x3400 -e2500 $db $dnaSeq
    echo

    echo TEST 2  # multiple volumes & query batches
    lastdb -s1 $db $dnaSeq
    lastal -f0 -i1 -w0 $db $dnaSeq
    echo

    echo TEST 3  # match-counting, with multiple query batches
    lastal -j0 -i1 -s0 $db $dnaSeq
    echo

    echo TEST 4  # translated alignment & genetic code file
    lastdb -p $db $protSeq
    lastal -F999 -e40 -G $gc $db $dnaSeq
    echo
} |
grep -v version |  # omit header lines with the LAST version number
diff last-test.out -

rm $db*
