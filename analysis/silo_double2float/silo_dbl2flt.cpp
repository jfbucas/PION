/// \file silo_dbl2flt.cc
/// \author Jonathan Mackey
/// 
/// This file reads in silo files generated by serial and parallel code
/// and, if the input files are double precision, it replaces them with
/// floating point to save disk space.
///
/// Mods:
/// - 2015.06.18-22 JM: started to program it.

#ifndef PARALLEL
# error "define PARALLEL so this will work!"
#endif

#include <list>
#include <iostream>
#include <sstream>
#include <silo.h>
using namespace std;

#include "defines/functionality_flags.h"
#include "defines/testing_flags.h"

#include "tools/reporting.h"
#include "constants.h"

#include "global.h"
#include "dataIO/dataio.h"
#include "dataIO/dataio_silo.h"
#include "dataIO/dataio_silo_utility.h"
#include "grid/uniform_grid.h"

#include "MCMD_control.h"
#include "setup_fixed_grid_MPI.h"


// ##################################################################
// ##################################################################




int main(int argc, char **argv)
{

  //
  // First initialise MPI, even though this is a single processor
  // piece of code.
  //
  int err = COMM->init(&argc, &argv);
  //
  // Also initialise the MCMD class with myrank and nproc.
  //
  class MCMDcontrol MCMD;
  int r=-1, np=-1;
  COMM->get_rank_nproc(&r,&np);
  MCMD.set_myrank(r);
  MCMD.set_nproc(np);

  //
  // Get an input file and an output file.
  //
  if (argc!=5) {
    cerr << "Error: must call as follows...\n";
    cerr << "silo_dbl2flt: <silo_dbl2flt> <input-dir> <input-file> <output-dir> <output-file> \n";
    rep.error("Bad number of args",argc);
  }
  string indir = argv[1];
  string inputfile  = argv[2];
  string outdir = argv[3];
  string outfilebase = argv[4];

  //redirect stdout if using more than one core.
  ostringstream path; path << outfilebase <<"_"<<r<<"_";
  if (np>1) rep.redirect(path.str());


  cout <<"indir="<<indir<<"\toutdir="<<outdir<<"\n";
  cout <<"input file: "<<inputfile;
  cout <<"\toutput file: "<<outfilebase<<"\n";

  //
  // set up dataio_utility classes, one to read double data, and the
  // other to write float data.
  //
  class dataio_silo_utility io_read("DOUBLE",&MCMD);
  class dataio_silo_pllel io_write("FLOAT",&MCMD);

  // ----------------------------------------------------------------
  // ----------------------------------------------------------------

  //
  // Get list of files to read:
  //
  list<string> ffiles;
  err += io_read.get_files_in_dir(indir, inputfile,  &ffiles);
  if (err) rep.error("failed to get first list of files",err);

  for (list<string>::iterator s=ffiles.begin(); s!=ffiles.end(); s++) {
    // If file is not a .silo file, then remove it from the list.
    if ((*s).find(".silo")==string::npos) {
      cout <<"removing file "<<*s<<" from list.\n";
      ffiles.erase(s);
    }
    else {
      cout <<"files: "<<*s<<endl;
    }
  }

  //
  // Set up iterators to run through all the files.
  //
  list<string>::iterator ff=ffiles.begin();

  //
  // Set the number of files to be the length of the shortest list.
  //
  unsigned int nfiles = ffiles.size();
  if (nfiles<1) rep.error("Need at least one file, but got none",nfiles);
  
  // ----------------------------------------------------------------
  // ----------------------------------------------------------------

  //
  // Set low-memory cells
  //
  CI.set_minimal_cell_data();
  
  //
  // Read the first file, and setup the grid based on its parameters.
  //
  ostringstream oo;
  oo.str(""); oo<<indir<<"/"<<*ff; inputfile =oo.str();
  err = io_read.ReadHeader(inputfile);
  if (err) rep.error("Didn't read header",err);

  //
  // get a setup_grid class, and use it to set up the grid.
  //
  class setup_fixed_grid *SimSetup =0;
  SimSetup = new setup_fixed_grid_pllel();
  class GridBaseClass *grid = 0;
  err  = MCMD.decomposeDomain();
  if (err) rep.error("main: failed to decompose domain!",err);
  //
  // Now we have read in parameters from the file, so set up a grid.
  //
  SimSetup->setup_grid(&grid,&MCMD);
  if (!grid) rep.error("Grid setup failed",grid);

  // ----------------------------------------------------------------
  // ----------------------------------------------------------------

  //
  // loop over all files: open first+output and write the difference.
  //
  for (unsigned int fff=0; fff<nfiles; fff++) {
    
    oo.str(""); oo<<indir<<"/"<<*ff; inputfile  =oo.str(); 
    cout <<"\n****************************************************\n";
    cout <<"fff="<<fff<<"\tinput file: "<<inputfile<<"\n";

    class file_status fstat;
    if (!fstat.file_exists(inputfile)) {
       cout <<"first file: "<<inputfile;
       rep.error("First or output file doesn't exist",inputfile);
    }
    
    // ----------------------------------------------------------------
    // ----------------------------------------------------------------

    //
    // Read in first code header so i know how to setup grid.
    //
    err = io_read.ReadHeader(inputfile);
    if (err) rep.error("Didn't read header",err);
  
    // ----------------------------------------------------------------
    // ----------------------------------------------------------------

    //
    // Read data (this reader can read serial or parallel data).
    //
    err = io_read.parallel_read_any_data(inputfile, grid);
    rep.errorTest("(silo_dbl2flt) Failed to read inputfile",0,err);

    // ----------------------------------------------------------------
    // ----------------------------------------------------------------
    //cout <<"FINISHED reading file: "<<inputfile<<endl;

    // *********************************************************************
    // ********* FINISHED READING FILE, NOW WRITE REPLACEMENT *********
    // *********************************************************************
    cout <<"Writing output file: "<<outfilebase<<endl;
    oo.str(""); oo<<outdir<<"/"<<outfilebase;
    io_write.OutputData(oo.str(), grid, SimPM.timestep);
    //cout <<"FINISHED writing output file: "<<outfilebase<<endl;

    //
    // move onto next first and output files
    //
    ff++;
  } // move onto next file

  //
  // Finish up and quit.
  //
  if (grid) {delete grid; grid=0;}
  COMM->finalise();
  delete COMM; COMM=0;
  //MPI_Finalize();
  return 0;
}



