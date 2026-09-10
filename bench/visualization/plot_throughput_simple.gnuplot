# Simple throughput plot - bar chart
set terminal pngcairo size 1200,800 enhanced font 'Arial,12'
set output '../plots/throughput_comparison.png'

set title "Allocator Throughput Comparison" font 'Arial,14' offset 0,-1
set xlabel "Benchmark" font 'Arial,12'
set ylabel "Throughput (Mops/sec)" font 'Arial,12'
set grid ytics
set key outside right top
set style data histogram
set style histogram clustered gap 1
set style fill solid 0.7 border -1
set boxwidth 0.8

# Colors
palloc_blue = "#0072BD"
system_gray = "#808080"
mimalloc_orange = "#D95319"
jemalloc_green = "#77AC30"
tcmalloc_red = "#EDB120"

plot '../results/throughput_data.csv' using 2:xtic(1) lc rgb system_gray title 'glibc', \
     '../results/throughput_data.csv' using 3:xtic(1) lc rgb mimalloc_orange title 'mimalloc', \
     '../results/throughput_data.csv' using 4:xtic(1) lc rgb jemalloc_green title 'jemalloc', \
     '../results/throughput_data.csv' using 5:xtic(1) lc rgb tcmalloc_red title 'tcmalloc', \
     '../results/throughput_data.csv' using 6:xtic(1) lc rgb palloc_blue title 'palloc' linewidth 2