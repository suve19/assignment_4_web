set terminal png size 1024,1024
set output 'statistics.png'
set xlabel 'Concurrency Level'
set ylabel 'Requests/s'
set style data linespoints

plot "statistics_fork.log" using 1:3 title 'Average Forked' with linespoints,  "statistics_fork.log" using 1:4 title 'std.dev (Fork)' with linespoints, \
   "statistics_thread.log" using 1:3 title 'Average Thread' with linespoints, "statistics_thread.log" using 1:4 title 'std.dev (Thread)' with linespoints
