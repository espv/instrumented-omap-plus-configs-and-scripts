#! /bin/zsh

output="peu-scale-`date +%FT%H:%M`.log"
let duration=60
let rate=10000

let repetitions=3

echo $output

counts=(1 2 4 8 32 64 128)

for count in $counts; do
    for n in `seq $repetitions`; do
        echo count=$count | tee -a $output
        { time LD_LIBRARY_PATH=./build ./build/scratch/scale --count=1 --device=./scaletest/multi/$count.device --duration="$duration" --rate="$rate" } 2>&1 | tee -a $output
    done
done
