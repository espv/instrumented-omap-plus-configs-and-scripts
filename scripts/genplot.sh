#! /bin/bash

input=$1
output=$2
title=$3

gnuplot -persist <<EOF

load '../plot/common.gnu'

set terminal "pdf"
set output "$output"
plot '$input' using (\$1):(\$2) with linespoints ls 1 title "$title"

EOF
