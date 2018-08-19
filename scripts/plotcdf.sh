#! /bin/bash

input=$1
output=$2
title=$3

# plot '$input' using (\$1):(\$2) with linespoints ls 1 title "$title"
# set xrange [+0:+4e5]

gnuplot <<EOF

set terminal unknown

load '../plot/common_dist.gnu'

set yrange [+0:+100]
set xtics 3e5

set terminal pdf
set output "$output"

set title "$title"

plot '$1' using (rounded(to_nsec(\$1, 1200))):(100.0/52804) smooth cumulative

EOF
