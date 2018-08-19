#! /bin/zsh

output="node-scale-`date +%FT%H:%M`.log"
let duration=60
let rate=10000

let repetitions=3

echo $output

# counts=(1 2 3 5 10 20 30 50 100)
# counts=(50 100)
counts=(1 2 3 5)

for count in $counts; do
    for n in `seq $repetitions`; do
        echo count=$count | tee -a $output
        { time LD_LIBRARY_PATH=./build ./build/scratch/scale --count="$count" --device=./scaletest/base.device --duration="$duration" --rate="$rate" } 2>&1 | tee -a $output
    done
done
