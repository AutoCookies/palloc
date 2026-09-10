# plot_pmu.gnuplot — Gnuplot template for PMU counter analysis
# ============================================================================
# Usage: gnuplot -c plot_pmu.gnuplot data.csv output.png "Title" "metric_type"

if (ARGC < 4) {
    print "Usage: gnuplot -c plot_pmu.gnuplot <data.csv> <output.png> [title] [metric_type]"
    print "metric_type: cache, tlb, branch, ipc"
    exit
}

data_file = ARG1
output_file = ARG2
if (ARGC >= 3) {
    plot_title = ARG3
} else {
    plot_title = "PMU Counter Analysis"
}
if (ARGC >= 4) {
    metric_type = ARG4
} else {
    metric_type = "cache"
}

set terminal pngcairo size 1200,800 enhanced font 'Arial,12'
set output output_file

# Colors
palloc_blue = "#0072BD"
system_gray = "#808080"
mimalloc_orange = "#D95319"
jemalloc_green = "#77AC30"
tcmalloc_red = "#EDB120"

set title plot_title font 'Arial,14' offset 0,-1
set grid ytics
set key outside right top

if (metric_type eq "cache") {
    set xlabel "Allocator" font 'Arial,12'
    set ylabel "Cache Miss Rate (%)" font 'Arial,12'
    set style data histogram
    set style histogram clustered gap 1
    set style fill solid 0.7 border -1
    set boxwidth 0.8
    
    plot data_file using 2:xtic(1) lc rgb "#808080" title 'L1 Miss Rate', \
         data_file using 3:xtic(1) lc rgb "#0072BD" title 'LLC Miss Rate'
         
    # Sample CSV format:
    # allocator,l1_miss_rate,llc_miss_rate
    # glibc,12.5,8.2
    # mimalloc,10.2,6.1
    # jemalloc,9.8,5.9
    # tcmalloc,10.0,6.0
    # palloc,8.5,5.1
}

if (metric_type eq "tlb") {
    set xlabel "Working Set (GB)" font 'Arial,12'
    set ylabel "dTLB Miss Rate (%)" font 'Arial,12'
    set style line 1 lc rgb "#808080" lt 1 lw 2 pt 7 ps 1.5
    set style line 2 lc rgb "#D95319" lt 1 lw 2 pt 5 ps 1.5
    set style line 3 lc rgb "#77AC30" lt 1 lw 2 pt 9 ps 1.5
    set style line 4 lc rgb "#EDB120" lt 1 lw 2 pt 13 ps 1.5
    set style line 5 lc rgb "#0072BD" lt 1 lw 3 pt 6 ps 2.0
    
    plot data_file using 1:2 with linespoints ls 1 title 'glibc', \
         data_file using 1:3 with linespoints ls 2 title 'mimalloc', \
         data_file using 1:4 with linespoints ls 3 title 'jemalloc', \
         data_file using 1:5 with linespoints ls 4 title 'tcmalloc', \
         data_file using 1:6 with linespoints ls 5 title 'palloc'
         
    # Sample CSV format:
    # working_set_gb,glibc,mimalloc,jemalloc,tcmalloc,palloc
    # 1,15.2,12.5,11.8,12.2,8.5
    # 2,22.8,18.2,16.8,17.5,12.4
    # 4,35.6,28.4,25.2,26.8,18.6
    # 8,48.2,38.6,34.8,36.4,24.8
}

if (metric_type eq "branch") {
    set xlabel "Allocator" font 'Arial,12'
    set ylabel "Branch Misprediction Rate (%)" font 'Arial,12'
    set style data histogram
    set style histogram clustered gap 1
    set style fill solid 0.7 border -1
    set boxwidth 0.8
    
    plot data_file using 2:xtic(1) lc rgb "#808080" title 'Branch Misprediction Rate'
    
    # Sample CSV format:
    # allocator,branch_mispred_rate
    # glibc,8.5
    # mimalloc,7.5
    # jemalloc,7.2
    # tcmalloc,7.4
    # palloc,6.4
}

if (metric_type eq "ipc") {
    set xlabel "Allocator" font 'Arial,12'
    set ylabel "IPC (Instructions Per Cycle)" font 'Arial,12'
    set style data histogram
    set style histogram clustered gap 1
    set style fill solid 0.7 border -1
    set boxwidth 0.8
    
    plot data_file using 2:xtic(1) lc rgb "#0072BD" title 'IPC'
    
    # Sample CSV format:
    # allocator,ipc
    # glibc,0.85
    # mimalloc,0.95
    # jemalloc,0.98
    # tcmalloc,0.94
    # palloc,1.09
}