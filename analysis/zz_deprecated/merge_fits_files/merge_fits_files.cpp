/// \file merge_fits_files.cpp
/// \author Jonathan Mackey
/// 
/// This file reads in fits files generated by parallel code and stitches the
/// partial files together into a single file.
/// input files are of name filename_procno.timestep.fits
/// 
/// 2016.06.15 JM: updated for new PION version.

#ifndef PARALLEL
# error "define PARALLEL so this will work!"
#endif


#include "defines/functionality_flags.h"
#include "defines/testing_flags.h"

#include "tools/reporting.h"
#include "tools/mem_manage.h"
#include "tools/timer.h"
#include "constants.h"
#include "sim_params.h"

#include "decomposition/MCMD_control.h"
#include "grid/setup_fixed_grid_MPI.h"
#include "grid/uniform_grid.h"
#include "dataIO/dataio_fits.h"

#include <sstream>
#include "fitsio.h"
using namespace std;

int main(int argc, char **argv)
{

  //
  // First initialise MPI, even though this is a single processor
  // piece of code.
  //
  int err = COMM->init(&argc, &argv);

  // Get an input file, an output file, and a step.
  if (argc!=5) {
    cerr << "Error: must call as follows...\n";
    cerr << "merge_fits_files <source_path> <infilebase> <outfile> <nproc>\n"; // <startstep> <step>\n";
    exit(1);
  }
  string input_path = argv[1];
  string infilebase  = argv[2];
  string outfilebase = argv[3];

  //
  // Also initialise the MCMD class with myrank and nproc.
  // Get nproc from command-line (number of fits files for each
  // snapshot)
  //
  int myrank=-1, nproc=-1;
  COMM->get_rank_nproc(&myrank,&nproc);
  nproc = atoi(argv[4]);
  class SimParams SimPM;
  SimPM.levels.clear();
  SimPM.levels.resize(1);
  SimPM.levels[0].MCMD.set_myrank(myrank);
  SimPM.levels[0].MCMD.set_nproc(nproc);
  class MCMDcontrol *MCMD = &(SimPM.levels[0].MCMD);

  cout <<"reading from file base "<<infilebase<<endl;
  cout <<"Writing to file "<<outfilebase<<endl;
  //cout <<"Step between outputs is "<<step<<" timesteps.\n";
  cout <<"number of processors: "<<MCMD->get_nproc()<<"\n";
  cout <<"**********************************************\n";
  // Open two input files and output file.
  class DataIOFits dataio(SimPM);
  class file_status fs;
  
  fitsfile *ffin,*ffout;
  int status=0;
  ostringstream temp; string infile,outfile;
  
  //*******************************************************************
  // Get input files
  //*******************************************************************
  cout <<"-------------------------------------------------------\n";
  cout <<"--------------- Getting List of Files to read ---------\n";
  //
  // Get list of files to read:
  //
  list<string> files;
  string infile_zero=infilebase+"_0000";
  err += dataio.get_files_in_dir(input_path, infile_zero,  &files);
  if (err) rep.error("failed to get list of files",err);
  //
  // Remove non-FITS files from list
  //
  for (list<string>::iterator s=files.begin(); s!=files.end(); s++) {
    // If file is not a .fits file, then remove it from the list.
    if ((*s).find(".fits")==string::npos) {
      cout <<"removing file "<<*s<<" from list.\n";
      files.erase(s);
      s=files.begin();
    }
    else {
      cout <<"files: "<<*s<<endl;
    }
  }
  for (list<string>::iterator s=files.begin(); s!=files.end(); s++) {
    cout <<"files: "<<*s<<endl;
  }
  size_t nfiles = files.size();
  size_t ifile=0;
  if (nfiles<1) rep.error("Need at least one file, but got none",nfiles);

  cout <<"--------------- Got list of Files ---------------------\n";
  cout <<"-------------------------------------------------------\n";
  //
  // Set up an iterator to run through all the files.
  //
  list<string>::iterator ff=files.begin();
  cout <<"-------------------------------------------------------\n";
  cout <<"--------------- Starting Loop over all input files ----\n";
  cout <<"-------------------------------------------------------\n";
  clk.start_timer("analyse_data");

  //*******************************************************************
  // loop over all files:
  //*******************************************************************

  for (ifile=0; ifile<nfiles; ifile++) {
    cout.flush();
    cout <<"------ Starting Next Loop: ifile="<<ifile<<", time so far=";
    cout <<clk.time_so_far("analyse_data")<<" ----\n";

    temp.str("");
    temp <<input_path<<"/"<<*ff;
    string infile = temp.str();
    ff++;
    
    cout <<"\n*****************************************************\n";
    cout <<"Opening fits file "<<infile<<"\n";
    err = fits_open_file(&ffin, infile.c_str(), READONLY, &status);
    if(status) {fits_report_error(stderr,status); return(err);}

    //
    // This should set LocalXmin and Xmin/max/range/NG
    //
    MCMD->set_myrank(0);
    err = dataio.ReadHeader(infile,SimPM);
    if (err) rep.error("Didn't read header",err);
    MCMD->decomposeDomain(SimPM, SimPM.levels[0]);

    //
    // Outfile:
    //
    temp.str(""); temp <<outfilebase<<".";
    temp.width(8); temp.fill('0');
    temp <<SimPM.timestep<<".fits";
    outfile = temp.str();

    if (dataio.file_exists(outfile)) {
      temp.str(""); temp << "!" << outfile; outfile=temp.str();
      cout <<"Output file exists!  hopefully this is ok.\n";
    }
    fits_create_file(&ffout, outfile.c_str(), &status);
    if(status) {cerr<<"outfile open went bad.\n";exit(1);}


    cout <<"infile[0]: "<<infile<<"\nand outfile: "<<outfile<<endl;

    //
    //  - copy header from first infile.
    //
    err = fits_copy_header(ffin,ffout,&status);
    if (status) {fits_report_error(stderr,status); return(err);}
     
    //
    //  - for each hdu in proc 0 infile, create full size image hdu in outfile
    //
    int num; err = fits_get_num_hdus(ffin,&num,&status);
    if (status) {fits_report_error(stderr,status); return(err);}
    long int *pix = new long int [SimPM.ndim];
    for (int j=0;j<SimPM.ndim;j++) pix[j]=SimPM.NG[j];
    cout <<"\tCreating "<<num<<" hdus in outfile (including header)...";
    rep.printVec("pix",pix,2);

    for (int i=2; i<=num; i++) { // hdu1 is header.
      fits_create_img(ffout,DOUBLE_IMG,SimPM.ndim,pix,&status);
      // get hdu name from ffin
      ffmahd(ffin,i,0,&status);
      if (status) {
        cout <<"move to infile hdu "<<i<<endl;
        fits_report_error(stderr,status); return(err);
      }
      char keyval[32];
      fits_read_keyword(ffin,"extname",keyval,0,&status);
      string sss=keyval;
      //cout <<"sss="<<sss;
      int length = sss.length(); sss=sss.substr(1,length-2);
      //cout <<" sss="<<sss<<endl;
      strcpy(keyval,sss.c_str());
      fits_write_key(ffout, TSTRING, "extname",  keyval, "Image Name", &status);
      if (status) {
        cout <<"reading/writing extname for hdu "<<i<<endl;
        fits_report_error(stderr,status); return(err);
      }
    }
    cout <<" done.\n";
    delete [] pix;
    err += fits_close_file(ffin,&status);
    if(status) {fits_report_error(stderr,status); return(err);}
     
    //
    // - for each hdu in proc i infile, copy image into appropriate
    // place in oufile image.
    //
    cout <<"\tNow copying infile hdus into outfile.\n";
    for (int proc=0; proc<MCMD->get_nproc(); proc++) {
      cout <<"\t\tproc "<<proc<<": ";
      MCMD->set_myrank(proc);
      MCMD->decomposeDomain(SimPM, SimPM.levels[0]);
      //
      // read infile header to get local ng/xmin/xmax
      //
      //temp.str("");temp<<infilebase<<"_"<<proc<<"."<<start<<".fits";
      //infile = temp.str();
      temp.str("");
      temp << input_path << "/" << infilebase << "_";
      temp.width(4); temp.fill('0'); temp << proc;
      infile = dataio.choose_filename(temp.str(),SimPM.timestep);
      //temp.str(""); temp <<outfilebase<<"."<<start<<".fits";
      if (!fs.file_exists(infile)) rep.error("infile doesn't exist",infile);

      err = dataio.ReadHeader(infile,SimPM);
      if (err) rep.error("Didn't read header",err);
      MCMD->set_myrank(proc);
      MCMD->decomposeDomain(SimPM, SimPM.levels[0]);

      err = fits_open_file(&ffin, infile.c_str(), READONLY, &status);
      if(status) {fits_report_error(stderr,status); return(err);}
       
      double *array=0;
      cout <<"MCMD->LocalNcell = "<<MCMD->LocalNcell<<"\n";
      array = new double[MCMD->LocalNcell];
      if (!array) rep.error("mem alloc",array);
       
      for (int im=2;im<=num;im++) { // hdu1 is header.
        ffmahd(ffin, im,0,&status);
        ffmahd(ffout,im,0,&status);
        if (status) {
          cout <<"move to infile/outfile hdu "<<im<<endl;
          fits_report_error(stderr,status); return(err);
        }
        cout <<" hdu"<<im;
        //
        // read image from infile into array.
        //
        long int *fpix = new long int [SimPM.ndim];
        long int *lpix = new long int [SimPM.ndim];
        long int *inc  = new long int [SimPM.ndim]; // I think this is the increment in num. pix. per read.
        long int npix=1; double dx = MCMD->LocalRange[XX]/MCMD->LocalNG[XX];
        for (int i=0;i<SimPM.ndim;i++) {
          inc[i] = 1;
          fpix[i] = 1;
          lpix[i] = MCMD->LocalNG[i]; // It's inclusive: fpix,fpix+1,...,lpix
          npix *= (lpix[i]-fpix[i]+1);  // +1 because of previous line.
          //    cout <<"fpix[i],lpix[i] = "<<fpix[i]<<", "<<lpix[i]<<endl;
        }
        if (npix != MCMD->LocalNcell) {
          cout <<"ncell = "<<MCMD->LocalNcell<<" and counted "<<npix<<" cells.\n";
          rep.error("Pixel counting failed in Image Read",npix-MCMD->LocalNcell);
        }
        double nulval = -1.e99; int anynul=0;
        // Read data from image.
        fits_read_subset(ffin, TDOUBLE, fpix, lpix, inc, &nulval, array, &anynul, &status);
        if (status) {fits_report_error(stderr,status); return status;}
         
        //
        // write image into subset of output file.
        //
        npix = 1;
        for (int i=0;i<SimPM.ndim;i++) {
          fpix[i] = static_cast<long int>((MCMD->LocalXmin[i]-SimPM.Xmin[i])/dx*1.00000001) +1;
          lpix[i] = fpix[i] + MCMD->LocalNG[i] -1; // -1 because it's inclusive: fpix,pfix+1,...,lpix
          npix *= (lpix[i]-fpix[i]+1);  // +1 because of previous line.
           //	   cout <<"fpix[i],lpix[i] = "<<fpix[i]<<", "<<lpix[i]<<endl;
        }
        if (npix != MCMD->LocalNcell) {
          cout <<"ncell = "<<MCMD->LocalNcell<<" and counted "<<npix<<" cells.\n";
          rep.error("Pixel counting failed in Image Write",npix-MCMD->LocalNcell);
        }
        fits_write_subset(ffout, TDOUBLE, fpix, lpix, array, &status);
        if(status) fits_report_error(stderr,status);
        delete [] fpix; fpix=0;
        delete [] lpix; lpix=0;
        delete [] inc;  inc=0;
      } // copy all images. (int im)
      cout <<"\n";

      err += fits_close_file(ffin,&status);
      if(status) {fits_report_error(stderr,status); return(err);}
      delete [] array; array=0;
    } // loop over all infiles (int proc)
    cout <<"\tOutfile "<<outfile<<" written.  Moving to next step.\n";
    //  - close outfile.
    err += fits_close_file(ffout,&status);
    if(status) {fits_report_error(stderr,status); return(err);}
     
  }  // loop over all timesteps.
  cout <<"\n***************************************************\n";
  //cout <<"couldn't find file "<<infile<<" for step "<<start<<"... assuming i'm finished!\n";

  if (COMM) {delete COMM; COMM=0;}

  return 0;
}
