# plot_memory.gnuplot — Gnuplot template for memory usage/fragmentation plots
# ============================================================================
# Usage: gnuplot -c plot_memory.gnuplot data.csv output.png "Title"

if (ARGC < 3) {
    print "Usage: gnuplot -c plot_memory.gnuplot <data.csv> <output.png> [title]"
    exit
}

data_file = ARG1
output_file = ARG2
if (ARGC >= 3) {
    plot_title = ARG3
} else {
    plot_title = "Memory Usage Comparison"
}

set terminal pngcairo size 1200,800 enhanced font 'Arial,12'
set output output_file

# Styling
set style data histogram
set style histogram stacked gap 1
set style fill solid 0.7 border -1
set boxwidth 0.8

# Colors
palloc_blue = "#0072BD"
system_gray = "#808080"
mimalloc_orange = "#D95319"
jemalloc_green = "#77AC30"
tcmalloc_red = "#EDB120"

set title plot_title font 'Arial,14' offset 0,-1
set xlabel "Allocator" font 'Arial,12'
set ylabel "Memory Usage (MiB)" font 'Arial,12'
set grid ytics
set key outside right top

# Plot memory usage (requested vs overhead)
plot data_file using 2:xtic(1) lc rgb "#4DBEEE" title 'Requested', \
     data_file using 3:xtic(1) lc rgb "#D95319" title 'Overhead'

# Sample CSV format:
# allocator,requested,overhead
# glibc,1024,1024
# mimalloc,1024,512
# jemalloc,1024,384
# tcmalloc,1024,448
# palloc,1024,256