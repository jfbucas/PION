#!/bin/bash
if [ "$1" = "" ]
  then
  echo "Usage: $0  <make_uname>"
#  echo "  <infile-path> can be a relative or absolute path to the directory containing the FITS files."
#  echo "  <infile-base> is the FITS filename, minus the _0000.XXXXXXXX.fits part."
#  echo "  <outfilebase> is the name of the merged fits files and should include a path, otherwise it will output to current directory."
#  echo "  <nproc> is the number of MPI processes the simulation was run with, equal to the number of FITS files for each timestep."
  echo "  <make_uname> is the machine identifier: standard (linux), OSX, other?"
  exit
else
  MAKE_UNAME=$1
fi

echo "Compiling with option: $MAKE_UNAME"

MAKE_UNAME=$MAKE_UNAME make -f Makefile -j4

exit


#valgrind  --leak-check=full --show-reachable=yes \
#./SILO2FITS $1 $2 $3 $4 $5 $6

#make clean

