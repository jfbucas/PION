#!/bin/bash

#
# This file should generate a gnu.plt file which gnuplot can use to read data and generate a figure.
#
infile1=$1
infile2=$2
outfile=$3

cat << EOF  > gnu.plt
#set terminal postscript enhanced eps
set terminal postscript enhanced color eps
set output "${outfile}.eps"
#set size 3.0, 1.0
set size 1.5, 0.5
set origin 0.0,0.0
set multiplot
#
# first plot
#
#set size 1.0, 1.0
set size 0.5, 0.5
set size square
set origin -0.1,0.0
#set yrange [0:1.05]
set xlabel "Position"
set ylabel "Density" offset 1.3,0.0
set xtics 0,.2
set key off
#set key noautotitles
#set key 0.45,0.2
plot '${infile1}'  using 1:2 w lp lt 1 pt 1 title 'density', \
     '${infile2}'  using 1:2 w l lt -1
#
# second plot
#
set origin 0.3,0.0
set ylabel "Total Pressure" offset 1.3,0.0
plot '${infile1}'  using 1:13 w lp lt 2 pt 2 , \
     '${infile2}'  using 1:13 w l lt -1

# THIRD PLOT
set origin 0.675,0.0
set ylabel "v_x" offset 1.3,0.0
plot '${infile1}'  using 1:4 with lp  lt 3 pt 3, \
     '${infile2}'  using 1:4 w l lt -1

# Fourth PLOT
set origin 1.05,0.0
set ylabel "B_y" offset 1.3,0.0
#plot '${infile1}'  using 1:sqrt($8*$8+$9*$9) with lp  lt 4 pt 4
plot '${infile1}'  using 1:8 with lp  lt 4 pt 4, \
     '${infile2}'  using 1:8 w l lt -1


unset multiplot
#pause -1
EOF

gnuplot gnu.plt
convert -density 300 -quality 100 ${outfile}.eps ${outfile}.jpeg
convert -adaptive-resize 900x210 ${outfile}.jpeg ${outfile}.jpeg

