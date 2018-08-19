#! /bin/bash

input=$1
output=$2
title=$3

# plot '$input' using (\$1):(\$2) with linespoints ls 1 title "$title"
# set xrange [+0:+4e5]

gnuplot <<EOF

set terminal unknown

load '../plot/common_dist.gnu'

set terminal pdf
set output "$output"

set title "$title"

plot '$input' using (rounded(to_nsec(\$1, 1200))):(1) smooth frequency with boxes

EOF
