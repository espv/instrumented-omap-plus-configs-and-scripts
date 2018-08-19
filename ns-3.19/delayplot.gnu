reset
clear
set style fill solid 1.0 noborder

bin_width = 1000
set boxwidth 750 absolute

bin_number(x) = floor(x/bin_width)
rounded(x) = bin_width * ( bin_number(x) + 0.5 )

to_nsec(cycles, mhz) = cycles * 1000 / mhz

plot 'delay.gnu' using (rounded(to_nsec($2,1200))):(1) smooth frequency with boxes
