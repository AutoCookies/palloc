# plot_throughput.gnuplot — Gnuplot template for throughput comparison plots
# ============================================================================
# Usage: gnuplot -c plot_throughput.gnuplot data.csv output.png "Title"

# Check arguments
if (ARGC < 3) {
    print "Usage: gnuplot -c plot_throughput.gnuplot <data.csv> <output.png> [title]"
    exit
}

data_file = ARG1
output_file = ARG2
if (ARGC >= 3) {
    plot_title = ARG3
} else {
    plot_title = "Allocator Throughput Comparison"
}

# Set output format
set terminal pngcairo size 1200,800 enhanced font 'Arial,12'
set output output_file

# Styling
set style data histogram
set style histogram clustered gap 1
set style fill solid 0.7 border -1
set boxwidth 0.8

# Colors (matching LaTeX template)
palloc_blue = "#0072BD"
system_gray = "#808080"
mimalloc_orange = "#D95319"
jemalloc_green = "#77AC30"
tcmalloc_red = "#EDB120"

# Plot settings
set title plot_title font 'Arial,14' offset 0,-1
set xlabel "Benchmark" font 'Arial,12'
set ylabel "Throughput (Mops/sec)" font 'Arial,12'
set grid ytics
set key outside right top

# Read data and plot
plot data_file using 2:xtic(1) lc rgb system_gray title 'glibc', \
     data_file using 3:xtic(1) lc rgb mimalloc_orange title 'mimalloc', \
     data_file using 4:xtic(1) lc rgb jemalloc_green title 'jemalloc', \
     data_file using 5:xtic(1) lc rgb tcmalloc_red title 'tcmalloc', \
     data_file using 6:xtic(1) lc rgb palloc_blue title 'palloc' linewidth 2

# Sample CSV format:
# benchmark,glibc,mimalloc,jemalloc,tcmalloc,palloc
# sh6bench,45.2,68.5,62.4,58.7,75.8
# sh8bench,42.8,65.2,58.9,55.4,72.6
# alloc-test,38.5,58.4,52.6,49.8,65.2
# larson,35.2,52.8,47.5,45.2,58.9