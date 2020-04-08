#!/bin/bash

../../icgen-ng params_MHD_blastwave2D_B001_n040.txt silo
../../icgen-ng params_MHD_blastwave2D_B001_n200.txt silo
../../pion-ng BW2d_StoneMHD_B001_n040_level00_0000.00000000.silo \
  outfile=HLLD_B001_n040_l2 solver=7 opfreq_time=0.1
mpirun -np 4 ../../pion-ng \
  BW2d_StoneMHD_B001_n200_level00_0000.00000000.silo \
  outfile=HLLD_B001_n200_l2 solver=7 opfreq_time=0.1

../../icgen-ng params_MHD_blastwave2D_B010_n040.txt silo
../../icgen-ng params_MHD_blastwave2D_B010_n200.txt silo
../../pion-ng BW2d_StoneMHD_B010_n040_level00_0000.00000000.silo \
  outfile=HLLD_B010_n040_l2 solver=7 opfreq_time=0.1
mpirun -np 4 ../../pion-ng \
  BW2d_StoneMHD_B010_n200_level00_0000.00000000.silo \
  outfile=HLLD_B010_n200_l2 solver=7 opfreq_time=0.1

../../icgen-ng params_MHD_blastwave2D_B100_n040.txt silo
../../icgen-ng params_MHD_blastwave2D_B100_n200.txt silo
../../pion-ng BW2d_StoneMHD_B100_n040_level00_0000.00000000.silo \
  outfile=HLLD_B100_n040_l2 solver=7 opfreq_time=0.1
mpirun -np 4 ../../pion-ng \
  BW2d_StoneMHD_B100_n200_level00_0000.00000000.silo \
  outfile=HLLD_B100_n200_l2 solver=7 opfreq_time=0.1

