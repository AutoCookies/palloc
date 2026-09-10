# Line chart throughput plot
set terminal pngcairo size 1200,800 enhanced font 'Arial,12'
set output '../plots/throughput_comparison.png'

set title "Allocator Throughput Comparison" font 'Arial,14' offset 0,-1
set xlabel "Benchmark" font 'Arial,12'
set ylabel "Throughput (Mops/sec)" font 'Arial,12'
set grid ytics
set key outside right top

# Colors
palloc_blue = "#0072BD"
system_gray = "#808080"
mimalloc_orange = "#D95319"
jemalloc_green = "#77AC30"
tcmalloc_red = "#EDB120"

# Rotate x-axis labels for better readability
set xtics rotate by -45

plot '../results/throughput_data.csv' using 2:xtic(1) with linespoints lc rgb system_gray lw 2 pt 7 title 'glibc', \
     '../results/throughput_data.csv' using 3:xtic(1) with linespoints lc rgb mimalloc_orange lw 2 pt 5 title 'mimalloc', \
     '../results/throughput_data.csv' using 4:xtic(1) with linespoints lc rgb jemalloc_green lw 2 pt 9 title 'jemalloc', \
     '../results/throughput_data.csv' using 5:xtic(1) with linespoints lc rgb tcmalloc_red lw 2 pt 13 title 'tcmalloc', \
     '../results/throughput_data.csv' using 6:xtic(1) with linespoints lc rgb palloc_blue lw 3 pt 6 title 'palloc'