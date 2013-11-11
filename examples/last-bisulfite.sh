#! /bin/sh

# Align bisulfite-converted DNA reads to a genome.

# This assumes that the reads are all from the converted strand
# (i.e. they have C->T conversions, not G->A conversions).

[ $# -gt 1 ] || {
    cat <<EOF
Typical usage:

  lastdb -w 2 -u bisulfite_f.seed my_f mygenome.fa
  lastdb -w 2 -u bisulfite_r.seed my_r mygenome.fa

  $(basename $0) my_f my_r reads.fastq > results.maf

EOF
    exit 2
}
my_f=$1
my_r=$2
shift 2

# Try to get the LAST programs into the PATH, if they aren't already:
PATH=$PATH:$(dirname $0)/../src:$(dirname $0)/../scripts

tmp=${TMPDIR-/tmp}/$$
trap 'rm -f $tmp.*' EXIT

cat > "$tmp".fmat <<EOF
    A   C   G   T
A   6 -18 -18 -18
C -18   6 -18   3
G -18 -18   6 -18
T -18 -18 -18   3
EOF

cat > "$tmp".rmat <<EOF
    A   C   G   T
A   3 -18 -18 -18
C -18   6 -18 -18
G   3 -18   6 -18
T -18 -18 -18   6
EOF

# Convert C to t, and all other letters to uppercase:
perl -pe 'y/Cca-z/ttA-Z/ if $. % 4 == 2' "$@" > "$tmp".q

lastal -p "$tmp".fmat -s1 -Q1 -e120 "$my_f" "$tmp".q > "$tmp".f
lastal -p "$tmp".rmat -s0 -Q1 -e120 "$my_r" "$tmp".q > "$tmp".r

last-merge-batches.py "$tmp".f "$tmp".r | last-split -m0.1 |
perl -F'(\s+)' -ane '$F[12] =~ y/ta/CG/ if /^s/ and $s++ % 2; print @F'