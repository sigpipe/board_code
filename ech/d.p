set title 'samples';
set xlabel 'time (ns)';
set ylabel 'amplitude (adc)';
plot 'dg.txt' with points pointtype 7;
set label 1 '0.811' at 0.811,290.000 point pointtype 7 lc rgb 'red';
