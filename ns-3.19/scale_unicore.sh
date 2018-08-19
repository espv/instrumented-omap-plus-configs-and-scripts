#! /bin/zsh

output="unicore-scale-`date +%FT%H:%M`.log"
let duration=60
let rate=10000

let repetitions=3

# echo $output

for file in `ls -v ./scaletest/uni`; do
    for n in `seq $repetitions`; do
        echo "\$file=$file" | tee -a $output
        { time LD_LIBRARY_PATH=./build ./build/scratch/scale --device=./scaletest/uni/$file --duration="$duration" --rate="$rate" } 2>&1 | tee -a $output
    done
done
