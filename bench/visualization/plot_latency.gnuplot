# plot_latency.gnuplot — Gnuplot template for latency distribution plots
# ============================================================================
# Usage: gnuplot -c plot_latency.gnuplot data.csv output.png "Title"

if (ARGC < 3) {
    print "Usage: gnuplot -c plot_latency.gnuplot <data.csv> <output.png> [title]"
    exit
}

data_file = ARG1
output_file = ARG2
if (ARGC >= 3) {
    plot_title = ARG3
} else {
    plot_title = "Allocation Latency Distribution"
}

set terminal pngcairo size 1200,800 enhanced font 'Arial,12'
set output output_file

# Styling
set style line 1 lc rgb "#808080" lt 1 lw 2 pt 7 ps 1.5
set style line 2 lc rgb "#D95319" lt 1 lw 2 pt 5 ps 1.5
set style line 3 lc rgb "#77AC30" lt 1 lw 2 pt 9 ps 1.5
set style line 4 lc rgb "#EDB120" lt 1 lw 2 pt 13 ps 1.5
set style line 5 lc rgb "#0072BD" lt 1 lw 3 pt 6 ps 2.0

set title plot_title font 'Arial,14' offset 0,-1
set xlabel "Percentile" font 'Arial,12'
set ylabel "Latency (ns)" font 'Arial,12'
set grid ytics
set key outside right top
set logscale y

# Plot latency percentiles
plot data_file using 1:2 with linespoints ls 1 title 'glibc', \
     data_file using 1:3 with linespoints ls 2 title 'mimalloc', \
     data_file using 1:4 with linespoints ls 3 title 'jemalloc', \
     data_file using 1:5 with linespoints ls 4 title 'tcmalloc', \
     data_file using 1:6 with linespoints ls 5 title 'palloc'

# Sample CSV format:
# percentile,glibc,mimalloc,jemalloc,tcmalloc,palloc
# 50,128,95,88,92,72
# 90,245,165,152,158,128
# 95,312,198,185,192,165
# 99,458,285,268,278,225
# 99.9,756,425,398,412,352