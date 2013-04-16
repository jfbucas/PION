/// \file dataio_silo_utility.cc
/// \author Jonathan Mackey
/// 
/// This is code for analysing silo data files in serial mode;
/// written so that a single function will determine if the file is
/// serial or parallel and read in the data regardless.  It gets the
/// parameters for the grid from the header.
///
///
///  - 2010-02-02 JM: Added support for N procs to read data written
///     by M procs, where N not equal to M.
///  - 2010-02-03 JM: Fixed all the bugs in yesterday's work (as far
///     as i could find).
///  - 2010-04-27 JM: renamed 'ngroups' to 'groupsize', and updated
///    logic so the last file can have fewer domains.
///  - 2010.11.15 JM: replaced endl with c-style newline chars.
/// - 2011.03.02 JM: Better handling of tracer variables (up to 
///    MAX_NVAR now).
/// - 2012.05.17 JM: Fixed bug in how pllel_read_any_data() dealt
///    with silo databases where files don't all have the same number
///    of meshes.
/// - 2013.02.19 JM: Got rid of dataio_utility class, and moved its
///    functions into file_status class, which now has its own file.
///    Renamed file to dataio_silo_utility.cpp from dataio_utility.cc

#include "dataIO/dataio_silo_utility.h"
#include <iostream>
#include <sstream>
using namespace std;




/********************************************************/
/*************** dataio_silo_utility ********************/
/********************************************************/


// ##################################################################
// ##################################################################


int dataio_silo_utility::SRAD_get_nproc_numfiles(string fname,
						 int *np,
						 int *nf
						 )
{
  int err=0;

  //
  // open file
  //
  cout <<"opening file: "<<fname<<"\n";
  DBfile *dbfile = 0;
  dbfile = DBOpen(fname.c_str(), DB_UNKNOWN, DB_READ);
  if (!dbfile) rep.error("open silo file failed.",dbfile);
  
  //
  // read nproc, numfiles from header
  //
  DBSetDir(dbfile,"/header");
  int nproc=0, numfiles=0; //, groupsize=0;
  err += DBReadVar(dbfile,"MPI_nproc",&nproc);
  err += DBReadVar(dbfile,"NUM_FILES",&numfiles);
  if (err) {
    cout <<"must be serial file -- failed to find NUM_FILES and MPI_nproc\n";
    cout <<"continuing assuming serial file....\n";
    //rep.error("error reading params from file",fname);
    nproc=1; numfiles=1; //groupsize=1;
  }
  else {
    cout <<"\tRead nproc="<<nproc<<"\tand numfiles="<<numfiles<<"\n";
    //    groupsize = nproc/numfiles;
    nproc = nproc;
  }
  DBClose(dbfile); dbfile=0; 
  
  *np = nproc;
  *nf = numfiles;
  return err;
}



// ##################################################################
// ##################################################################



bool dataio_silo_utility::SRAD_point_on_my_domain(const cell *c, ///< pointer to cell
						  class ParallelParams *filePM ///< pointer to class with nproc.
						  )
{
  //
  // Assume point is on domain, and set to false if found to be off.
  //
  bool on=true;
  double dpos[SimPM.ndim]; CI.get_dpos(c,dpos);
  for (int i=0; i<SimPM.ndim; i++) {
    if (dpos[i]<filePM->LocalXmin[i]) on=false;
    if (dpos[i]>filePM->LocalXmax[i]) on=false;
  }
  return on;
}



// ##################################################################
// ##################################################################



int dataio_silo_utility::SRAD_read_var2grid(DBfile *dbfile,        ///< pointer to silo file.
					    class GridBaseClass *ggg, ///< pointer to data.
					    const string variable, ///< variable name to read.
					    const long int npt,     ///< number of cells expected.
					    class ParallelParams *filePM ///< pointer to class with nproc.
					    )
{
  //
  // The dbfile pointer should already be in the directory containing
  // the named variable to read, so it's ok to bug out if we don't
  // find it.
  //
  DBquadvar *silodata=0;
  silodata = DBGetQuadvar(dbfile,variable.c_str());
  if (!silodata)
    rep.error("dataio_silo::read_variable2grid() failed to read variable",variable);
  if (silodata->nels != npt)
    rep.error("dataio_silo::read_variable2grid() wrong number of cells",silodata->nels-SimPM.Ncell);

  FAKE_DOUBLE **data = (FAKE_DOUBLE **)(silodata->vals);

  if (variable=="Velocity" || variable=="MagneticField") {
    int v1,v2,v3;
    if (variable=="Velocity") {v1=VX;v2=VY;v3=VZ;}
    else                      {v1=BX;v2=BY;v3=BZ;}
    //    cout <<"name: "<<silodata->name<<"\tnels="<<silodata->nels<<"\n";
    //    cout <<"ndims: "<<silodata->ndims<<"\tnvals: "<<silodata->nvals<<"\n";
    //cout <<"reading variable "<<variable<<" into element "<<v1<<" of state vec.\n";
    cell *c=ggg->FirstPt(); long int ct=0;
    do {
      if (SRAD_point_on_my_domain(c,filePM)) {
	//      cout <<"ct="<<ct<<"\t and ncell="<<npt<<"\n";
	c->P[v1] = data[0][ct];
	c->P[v2] = data[1][ct];
	c->P[v3] = data[2][ct];
	//c->P[v1] = silodata->vals[0][ct];
	//c->P[v2] = silodata->vals[1][ct];
	//c->P[v3] = silodata->vals[2][ct];
	//cout <<"ct="<<ct<<"\t and ncell="<<npt<<"\n";
	ct++;
      }
    } while ( (c=ggg->NextPt(c))!=0 );
    if (ct != npt) rep.error("wrong number of points read for vector variable",ct-npt);
  } // vector variable

  else {
    int v1=0;
    if      (variable=="Density")         v1=RO;
    else if (variable=="Pressure")        v1=PG;
    else if (variable=="VelocityX")       v1=VX;
    else if (variable=="VelocityY")       v1=VY;
    else if (variable=="VelocityZ")       v1=VZ;
    else if (variable=="MagneticFieldX")  v1=BX;
    else if (variable=="MagneticFieldY")  v1=BY;
    else if (variable=="MagneticFieldZ")  v1=BZ;
    else if (variable=="glmPSI")          v1=SI;
    //
    // Now loop over up to MAX_NVAR tracers...
    //
    else if (variable.substr(0,2)=="Tr") {
      int itr = atoi(variable.substr(2,3).c_str());
      if (!isfinite(itr) || itr<0 || itr>=MAX_NVAR) {
        rep.error("Bad diffuse Column-density identifier.",variable);
      }
      v1 = SimPM.ftr +itr;
    }
    //
    // The following are legacy for when I had a max of 10 tracers:
    //
    //else if (variable.substr(0,3)=="Tr0") {v1=SimPM.ftr;}
    //else if (variable.substr(0,3)=="Tr1") {v1=SimPM.ftr+1;}
    //else if (variable.substr(0,3)=="Tr2") {v1=SimPM.ftr+2;}
    //else if (variable.substr(0,3)=="Tr3") {v1=SimPM.ftr+3;}
    //else if (variable.substr(0,3)=="Tr4") {v1=SimPM.ftr+4;}
    //else if (variable.substr(0,3)=="Tr5") {v1=SimPM.ftr+5;}
    //else if (variable.substr(0,3)=="Tr6") {v1=SimPM.ftr+6;}
    //else if (variable.substr(0,3)=="Tr7") {v1=SimPM.ftr+7;}
    //else if (variable.substr(0,3)=="Tr8") {v1=SimPM.ftr+8;}
    //else if (variable.substr(0,3)=="Tr9") {v1=SimPM.ftr+9;}
    else rep.error("what var to read???",variable);
    //cout <<"reading variable "<<variable<<" into element "<<v1<<" of state vec.\n";

    //
    // Slow Way: SCANS EVERY CELL
    //
//     cell *c=ggg->FirstPt(); long int ct=0;
//     do {
//       if (SRAD_point_on_my_domain(c,filePM)) {
// 	//cout <<"val="<<silodata->vals[0][ct]<<" and data="<<data[0][ct]<<"\n";
// 	//c->P[v1] = silodata->vals[0][ct];
// 	c->P[v1] = data[0][ct];
// 	ct++;
// 	//      cout <<"ct="<<ct<<"\t and ncell="<<npt<<"\n";
//       }
//     } while ( (c=ggg->NextPt(c))!=0 );
//     if (ct != npt) rep.error("wrong number of points read for scalar variable",ct-npt);
    //
    // HOPEFULLY FASTER WAY: SCANS ONLY NEEDED CELLS.
    //
    // First get to start cell in local domain:
    //
    enum direction posdir[3] = {XP,YP,ZP};
    cell *start=ggg->FirstPt(); long int ct=0;
    for (int i=0;i<SimPM.ndim;i++) {
      while (CI.get_dpos(start,i) < filePM->LocalXmin[i])
	start = ggg->NextPt(start,posdir[i]);
    }
    //
    // Now use nested for-loops to only go through cells in the local domain.
    // We assume we go through x-dir column first, then along y, and finally Z
    //
    class cell 
      *cx=start,
      *cy=start,
      *cz=start;
    if (SimPM.ndim<3) filePM->LocalNG[ZZ]=1;
    if (SimPM.ndim<2) filePM->LocalNG[YY]=1;
    for (int k=0; k<filePM->LocalNG[ZZ]; k++) {
      cy=cz;
      for (int j=0; j<filePM->LocalNG[YY]; j++) {
	cx = cy;
	for (int i=0; i<filePM->LocalNG[XX]; i++) {
	  if (!SRAD_point_on_my_domain(cx,filePM))
	    rep.error("FAST READ IS IN THE WRONG PLACE!!!",cx->pos[XX]);
	  cx->P[v1] = data[0][ct];
	  ct++;
 	  cx = ggg->NextPt(cx,posdir[XX]);
	}
	if (SimPM.ndim>1) cy = ggg->NextPt(cy,posdir[YY]);
      }
      if (SimPM.ndim>2) cz = ggg->NextPt(cz,posdir[ZZ]);
    }
   if (ct != npt) rep.error("wrong number of points read for scalar variable",ct-npt);
   
  } // scalar variable

  //  cout <<"Read variable "<<variable<<"\n";
  DBFreeQuadvar(silodata); //silodata=0;
  data=0;
  return 0;
}



// ##################################################################
// ##################################################################



void dataio_silo_utility::set_pllel_filename(std::string &fname,  ///< filename
					     const int ifile      ///< file number to replace name with.
					     )
{
  //
  // If we have a parallel file, Parse filename, and replace file number 
  // with new ifile, store in name 'fname'.
  //
  ostringstream temp; 
  temp.fill('0');
  string::size_type pos = fname.find("_0000");
  if (pos==string::npos) {
    cout <<"didn't find _0000 in file, but claim we are reading pllel file!\n";
    rep.error("not a parallel i/o filename!",fname);
  }
  else {
    temp.str("");temp<<"_";temp.width(4);temp<<ifile;
    fname.replace(pos,5,temp.str());
    //cout <<"\tNew fname: "<<fname<<"\n";
    temp.str("");
  }
  return;
}



// ##################################################################
// ##################################################################



void dataio_silo_utility::set_dir_in_file(std::string &mydir,  ///< directory name.
					  const int my_rank,       ///< myrank (global).
					  const int my_group_rank  ///< myrank in group.
					  )
{
  ostringstream temp; temp.fill('0');
  temp.str(""); temp << "/rank_"; temp.width(4); temp << my_rank << "_domain_";
  temp.width(4); temp << my_group_rank;
  mydir = temp.str(); temp.str("");
  //cout <<"\t\tdomain: "<<mydir<<"\n";
  return;
}  



// ##################################################################
// ##################################################################



int dataio_silo_utility::serial_read_any_data(string firstfile,        ///< file to read from
					      class GridBaseClass *ggg ///< pointer to data.
					      )
{
  if (!ggg) rep.error("null pointer to computational grid!",ggg);

  //
  // Read file data onto grid; this may be serial or parallel, so we
  // need to decide first.
  //
  int nproc=0, numfiles=0, groupsize=0;
  int err = SRAD_get_nproc_numfiles(firstfile, &nproc, &numfiles);

  //
  // Set up a ParallelParams class for iterating over the quadmeshes
  //
  class ParallelParams filePM;
  filePM.nproc = nproc;

  if (err) {
    //
    // must be reading serial file, so use serial ReadData() function:
    //
    groupsize   = 1;
    err = dataio_silo::ReadData(firstfile,ggg);
    rep.errorTest("Failed to read serial data",0,err);
  }
  else {
    //
    // must be reading parallel file, so want to read in every subdomain onto grid.
    // Use local functions for this.
    //
    // groupsize is decided interestingly by PMPIO (silo): if numfiles divided nproc
    // evenly then groupsize is nproc/numfiles, but if not then groupsize is 
    // ((int) (nproc/numfiles))+1, I guess so the last files has at least one fewer
    // domain rather than at least one more domain.
    //
    groupsize   = nproc/numfiles;
    if ((nproc%numfiles) !=0) groupsize++;

    err = serial_read_pllel_silodata(firstfile, ggg, numfiles, groupsize, &filePM);
    rep.errorTest("Failed to read parallel data",0,err);
  }
  //  cout <<"read data successfully.\n";
  return 0;
}



// ##################################################################
// ##################################################################



int dataio_silo_utility::serial_read_pllel_silodata(const string firstfile, ///< filename
						    class GridBaseClass *ggg, ///< pointer to data.
						    const int numfiles, ///< number of files
						    const int groupsize, ///< number of groups
						    class ParallelParams *filePM ///< number of processes used to write file.
						    )
{
  int err=0;

  //
  // First loop over all files:
  //
  for (int ifile=0; ifile<numfiles; ifile++) {
    string infile = firstfile;
    //
    // Replace filename with new file corresponding to current 'ifile' value.
    //
    set_pllel_filename(infile,ifile);
    
    DBfile *dbfile = DBOpen(infile.c_str(), DB_UNKNOWN, DB_READ);
    if (!dbfile) rep.error("open first silo file failed.",dbfile);
    //
    // loop over domains within this file.  The last file may have fewer
    // domains, so we set ng to be the minimum of groupsize or all 
    // remaining domains.
    //
    int ng=min(filePM->nproc-ifile*groupsize, groupsize);
    for (int igroup=0; igroup<ng; igroup++) {
      DBSetDir(dbfile,"/");
      //
      // choose myrank, and decompose domain accordingly.
      //
      filePM->myrank = ifile*groupsize +igroup;
      filePM->decomposeDomain();
      
      //
      // set directory in file.
      //
      string mydir;
      set_dir_in_file(mydir, filePM->myrank, igroup);
      DBSetDir(dbfile, mydir.c_str());
      
      //
      // set variables to read: (ripped from dataio_silo class)
      //
      //std::vector<string> readvars;
      //set_readvars(readvars);
      set_readvars(SimPM.eqntype);
      
      //
      // now read each variable in turn from the mesh
      //
      for (std::vector<string>::iterator i=readvars.begin(); i!=readvars.end(); ++i) {
	err = SRAD_read_var2grid(dbfile, ggg, (*i), filePM->LocalNcell, filePM);
	if (err)
	  rep.error("error reading variable",(*i));
      }
    } // loop over domains within a file.
    
    //
    // Close this file
    //
    DBClose(dbfile);
    dbfile=0; 
  } // loop over files

  //cout <<"read parallel data successfully.\n";
  return 0;
}



// ##################################################################
// ##################################################################



int dataio_silo_utility::parallel_read_any_data(string firstfile,        ///< file to read from
						class GridBaseClass *ggg ///< pointer to data.
						)
{
  if (!ggg) rep.error("null pointer to computational grid!",ggg);
  int err=0;

  //
  // If N==1, then we are running in serial and the local domain is
  // the full domain, so we call serial_read_any_data()
  // (THIS IS TRUE AS LONG AS WE ARE READING THE FULL DOMAIN, BUT WE
  //  MIGHT NOT BE, SO I'M RELAXING THIS FOR NOW...)
  //
  //if (mpiPM.nproc==1) {
  //  cout <<"Nproc=1 in dataio_silo_utility::parallel_read_any_data():";
  //  cout <<" calling serial_read_any_data().\n";
  //  err = serial_read_any_data(firstfile, ggg);
  //  return err;
  //}

  //
  // The idea behind this is that a program running on N cores can
  // read data written by M cores, where N and M can be any positive
  // integers (possibly powers of 2 for the domain decomposition to
  // work).
  // 
  // If we are a parallel program, then mpiPM should be already set,
  // and the domain decomposition into N sub-domains is already done,
  // and the grid is set up for this sub-domain.
  //
  int nproc=0, numfiles=0, groupsize=0;
  err = SRAD_get_nproc_numfiles(firstfile, &nproc, &numfiles);
  if (err) {
    //
    // must be reading serial file, so we only want part of the domain
    // read onto the local grid, so we need a new function to read
    // this.
    //
    groupsize   = 1;
    numfiles  = 1;
    err = parallel_read_serial_silodata(firstfile,ggg);
    rep.errorTest("(silocompare) Failed to read data",0,err);
  }
  else {
    //
    // must be reading parallel file, so want to read in every
    // subdomain onto grid.  Use local functions for this:
    //
    // groupsize is decided interestingly by PMPIO (silo): if numfiles divided nproc
    // evenly then groupsize is nproc/numfiles, but if not then groupsize is 
    // ((int) (nproc/numfiles))+1 for the first NX domains, and
    // (nproc/numfiles) for the remainder.  Here NX=(nproc%numfiles)
    //
    groupsize   = nproc/numfiles;
    if ((nproc%numfiles) !=0) groupsize++;

    //
    // We should take it in turns reading data (is this necessary????
    // not sure, but doing it anyway.)
    // Try allowing up to 16 simultaneous reads.
    //
    int max_reads=16;
    int nloops=0;
    if (mpiPM.nproc<max_reads) nloops = 1;
    else {
      nloops = mpiPM.nproc/max_reads;
      if (mpiPM.nproc%max_reads !=0) {
        nloops+=1; // this shouldn't happen, but anyway...
        cout <<"Nproc not a power of 2!  This will cause trouble.\n";
      }
    }

    GS.start_timer("readdata"); double tsf=0;
    for (int count=0; count<nloops; count++) {
      if ( (mpiPM.myrank+nloops)%nloops == count) {
	cout <<"!READING DATA!!... myrank="<<mpiPM.myrank<<"  i="<<count;
	err = parallel_read_parallel_silodata(firstfile, ggg, numfiles, groupsize, nproc);
	rep.errorTest("Failed to read parallel data",0,err);
      }
      else {
	cout <<"waiting my turn... myrank="<<mpiPM.myrank<<"  i="<<count;
      }
      COMM->barrier("pllel_file_read");
      tsf=GS.time_so_far("readdata");
      cout <<"\t time = "<<tsf<<" secs."<<"\n";
    }
    GS.stop_timer("readdata");

    //GS.start_timer("readdata"); double tsf=0;
    //for (int count=0; count<mpiPM.nproc; count++) {
    //  if (count==mpiPM.myrank) {
    //    cout <<"!READING DATA!!... myrank="<<mpiPM.myrank<<"  i="<<count;
    //    err = parallel_read_parallel_silodata(firstfile, ggg, numfiles, groupsize, nproc);
    //    rep.errorTest("Failed to read parallel data",0,err);
    //  }
    //  else {
    //    cout <<"waiting my turn... myrank="<<mpiPM.myrank<<"  i="<<count;
    //  }
    //  COMM->barrier("pllel_file_read");
    //  tsf=GS.time_so_far("readdata");
    //  cout <<"\t time = "<<tsf<<" secs."<<"\n";
    //}
    //GS.stop_timer("readdata");
  }
  //  cout <<"read data successfully.\n";
  return 0;
}


// ##################################################################
// ##################################################################




int dataio_silo_utility::parallel_read_serial_silodata(string firstfile,        ///< file to read from
						       class GridBaseClass *ggg ///< pointer to data.
						       )
{
  int err=0;
  //
  // This is quite simple -- the local domain reads in a subset of the
  // uniform grid in the file.
  //
  DBfile *dbfile = DBOpen(firstfile.c_str(), DB_UNKNOWN, DB_READ);
  if (!dbfile) rep.error("open first silo file failed.",dbfile);

  int ftype = DBGetDriverType(dbfile);
  if (ftype==DB_HDF5) {
    //cout <<"READING HDF5 FILE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
    //
    // Not sure if we need this for reading, but set it anyway.
    //
    int friendly=DBGetFriendlyHDF5Names();
    DBSetFriendlyHDF5Names(friendly);
  }

  //
  // Set variables to read based on what equations we are using (this
  // is read from the header previously)
  //
  err = set_readvars(SimPM.eqntype);
  if (err) rep.error("failed to set readvars in ReadData",err);

  string qm_dir("/");
  DBSetDir(dbfile, qm_dir.c_str());
  string qm_name="UniformGrid";

  //
  // Get max and min quadmesh positions (integers)
  //
  int mesh_iXmin[ndim], mesh_iXmax[ndim];
  get_quadmesh_integer_extents(dbfile, qm_dir, qm_name, mesh_iXmin, mesh_iXmax);
  //if (err) rep.error("Failed to get quadmesh extents!",qm_dir);

  //
  // now read each variable in turn from the mesh, using the parallel
  // read function.
  //
  for (std::vector<string>::iterator i=readvars.begin(); i!=readvars.end(); ++i) {
    err = PP_read_var2grid(dbfile, ggg, (*i), SimPM.Ncell, mesh_iXmin, mesh_iXmax);
    if (err)
      rep.error("dataio_silo::ReadData() error reading variable",(*i));
  }

  DBSetDir(dbfile,"/");
  DBClose(dbfile);
  dbfile=0;

  return err;
}




// ##################################################################
// ##################################################################




int dataio_silo_utility::parallel_read_parallel_silodata(string firstfile,        ///< file to read from
							 class GridBaseClass *ggg, ///< pointer to data.
							 const int numfiles, ///< number of files
							 const int groupsize, ///< number of groups
							 const int nmesh    ///< number of domains in file.
							 )
{
  int err=0;
  //
  // We need a ParallelParams struct to mimic the struct used to write the file.
  //
  class ParallelParams filePM;
  filePM.nproc = nmesh;

  //
  // First loop over all files:
  //
  for (int ifile=0; ifile<numfiles; ifile++) {
    string infile = firstfile;
    //
    // Replace filename with new file corresponding to current 'ifile' value.
    //
    set_pllel_filename(infile,ifile);
    
    DBfile *dbfile = DBOpen(infile.c_str(), DB_UNKNOWN, DB_READ);
    if (!dbfile) rep.error("open first silo file failed.",dbfile);
    //
    // loop over domains within this file. The number of domains per file is
    // not constant if (R=nproc%numfiles)!=0.  One extra domain is added to the
    // first R files.  If R==0, then we can ignore this, since all files have
    // ng=groupsize domains
    //
    int R = nmesh%numfiles; if (R<0) R+= numfiles;
    int ng=0;
    if (R!=0) {
      if (ifile<R) ng=groupsize;
      else         ng=groupsize-1;
    }
    else {
      ng=groupsize;
    }
    //int ng=min(nmesh-ifile*groupsize, groupsize);

    for (int igroup=0; igroup<ng; igroup++) {
      DBSetDir(dbfile,"/");
      //
      // choose myrank, and decompose domain accordingly.
      // This is complicated by the first R files having one more file per
      // group than the rest (but only if R!=0).
      //
      if (R!=0) {
        if (ifile<R)
          filePM.myrank = ifile*groupsize +igroup;
        else 
          filePM.myrank = (R*groupsize) +(ifile-R)*(groupsize-1) +igroup;
      }
      else {
        filePM.myrank = ifile*groupsize +igroup;
      }

      filePM.decomposeDomain();
      
      //
      // set directory in file.
      //
      string qm_dir;
      set_dir_in_file(qm_dir, filePM.myrank, igroup);
      DBSetDir(dbfile, qm_dir.c_str());
      
      //
      // Set mesh_name from rank. quadmesh is in the current directory
      // and called "unigridXXXX" where XXXX=filePM.myrank
      //
      string qm_name;
      mesh_name(filePM.myrank,qm_name);
      //cout <<"got mesh name= "<<qm_name<<" in mesh dir= "<<qm_dir<<"\n";

      //
      // Get max and min quadmesh positions (integers)
      //
      int mesh_iXmin[ndim], mesh_iXmax[ndim];
      get_quadmesh_integer_extents(dbfile, qm_dir, qm_name, mesh_iXmin, mesh_iXmax);
      //if (err) rep.error("Failed to get quadmesh extents!",qm_dir);

      //
      // Get max and min grid positions (integers)
      //
      int localmin[ndim], localmax[ndim];
      for (int v=0;v<ndim;v++) {
	localmin[v] = ggg->iXmin(static_cast<axes>(v));
	localmax[v] = ggg->iXmax(static_cast<axes>(v));
      }

      //
      // See if quadmesh intersects local domain at all.
      //
      bool get_data=true;
      for (int v=0;v<ndim;v++) {
	if ( (mesh_iXmax[v]<=localmin[v]) ||
	     (mesh_iXmin[v]>=localmax[v]) )
	  get_data=false;
      }

      if (!get_data) {
	//cout <<"*** skipping mesh "<<qm_name<<" because not on local domain.\n";
      }
      else {
	//cout <<"**** reading mesh "<<qm_name<<" because it is on local domain.\n";
	//
	// set variables to read: (ripped from dataio_silo class)
	//
	set_readvars(SimPM.eqntype);
	
	//
	// now read each variable in turn from the mesh, using the
	// parallel-parallel read function
	//
	for (std::vector<string>::iterator i=readvars.begin(); i!=readvars.end(); ++i) {
	  err = PP_read_var2grid(dbfile, ggg, (*i), filePM.LocalNcell, mesh_iXmin, mesh_iXmax);
	  if (err)
	    rep.error("error reading variable",(*i));
	} // loop over variables.
      }
    } // loop over domains within a file.
    

    DBClose(dbfile);
    dbfile=0; 
  } // loop over files

  return 0;
}



// ##################################################################
// ##################################################################



void dataio_silo_utility::mesh_name(const int rank, ///< rank
				    string &mesh_name
				    )
{
  //
  // Get mesh_name from rank
  //
  ostringstream temp; temp.str(""); temp.fill('0');
  temp<<"unigrid"; temp.width(4); temp<<rank;
  mesh_name = temp.str();
  return;
}



// ##################################################################
// ##################################################################



void dataio_silo_utility::get_quadmesh_extents(DBfile *dbfile,        ///< pointer to silo file.
					       const string mesh_dir, ///< directory of mesh
					       const string qm_name,  ///< name of mesh
					       double *mesh_xmin, ///< Xmin for mesh (output)
					       double *mesh_xmax ///< Xmax for mesh (output)
					       )
{

  //
  // Make sure we're in the right dir
  //
  DBSetDir(dbfile, mesh_dir.c_str());

  //
  // Now get the mesh from the file.
  //
  DBquadmesh *qm=0;
  qm = DBGetQuadmesh(dbfile,qm_name.c_str());
  if (!qm) rep.error("failed to get quadmesh",qm_name);

  FAKE_DOUBLE *qmmin = (FAKE_DOUBLE *)(qm->min_extents);
  FAKE_DOUBLE *qmmax = (FAKE_DOUBLE *)(qm->max_extents);

  for (int v=0;v<ndim;v++) {
    mesh_xmin[v] = qmmin[v];
    mesh_xmax[v] = qmmax[v];
    //cout <<"dir: "<<v<<"\t min="<<mesh_xmin[v]<<" and max="<<mesh_xmax[v]<<"\n";
  }

  DBFreeQuadmesh(qm); //qm=0;
  return;
}



// ##################################################################
// ##################################################################



void dataio_silo_utility::get_quadmesh_integer_extents(DBfile *dbfile,        ///< pointer to silo file.
						      const string mesh_dir, ///< directory of mesh
						      const string qm_name,  ///< name of mesh
						      int *iXmin, ///< integer Xmin for mesh (output)
						      int *iXmax  ///< integer Xmax for mesh (output)
						      )
{
  //
  // First get the double precision extents
  //
  double mesh_xmin[ndim], mesh_xmax[ndim];
  get_quadmesh_extents(dbfile, mesh_dir, qm_name, mesh_xmin, mesh_xmax);

  //
  // Now use the cell interface to get the integer extents (Note that
  // this will fail and bug out if the global grid class isn't set up,
  // since that defines the coordinate system).
  //
  CI.get_ipos_vec(mesh_xmin, iXmin);
  CI.get_ipos_vec(mesh_xmax, iXmax);
  
  return;
}



// ##################################################################
// ##################################################################



int dataio_silo_utility::PP_read_var2grid(DBfile *dbfile,        ///< pointer to silo file.
					  class GridBaseClass *ggg, ///< pointer to data.
					  const string variable, ///< variable name to read.
					  const long int,   ///< number of cells expected (defunct!)
					  const int *iXmin, ///< integer Xmin for mesh
					  const int *iXmax  ///< integer Xmax for mesh
					  )
{
  //
  // The dbfile pointer should already be in the directory containing
  // the named variable to read, so it's ok to bug out if we don't
  // find it.
  //
  DBquadvar *qv=0;
  qv = DBGetQuadvar(dbfile,variable.c_str());
  if (!qv)
    rep.error("dataio_silo::read_variable2grid() failed to read variable",variable);

  //
  // The first thing to do is see if the mesh we are reading
  // intersects the local domain at all, so we get the mesh associated
  // with this variable, and find its extents.
  //
//   DBquadmesh *qm=0;
//   string qm_name(qv->meshname);
//   //  cout <<"quadmesh for variable "<<variable<<" is called "<<qm_name<<" (and same?) "<<qv->meshname<<"\n";
//   qm = DBGetQuadmesh(dbfile,qm_name.c_str());
//   if (!qm) rep.error("failed to get quadmesh",qm_name);

//   double mesh_xmin[ndim], mesh_xmax[ndim];
//   int iXmin[ndim], iXmax[ndim], localmin[ndim], localmax[ndim];
//   FAKE_DOUBLE *qmmin = (FAKE_DOUBLE *)(qm->min_extents);
//   FAKE_DOUBLE *qmmax = (FAKE_DOUBLE *)(qm->max_extents);

//   for (int v=0;v<ndim;v++) {
//     mesh_xmin[v] = qmmin[v];
//     mesh_xmax[v] = qmmax[v];
//     //cout <<"dir: "<<v<<"\t min="<<mesh_xmin[v]<<" and max="<<mesh_xmax[v]<<"\n";
//     localmin[v] = ggg->iXmin(static_cast<axes>(v));
//     localmax[v] = ggg->iXmax(static_cast<axes>(v));
//   }
//   CI.get_ipos_vec(mesh_xmin,iXmin);
//   CI.get_ipos_vec(mesh_xmax,iXmax);
  
//   bool get_data=true;
//   for (int v=0;v<ndim;v++) {
//     if ( (iXmax[v]<=localmin[v]) ||
// 	 (iXmin[v]>=localmax[v]) )
//       get_data=false;
//   }

//   if (!get_data) {
//     cout <<"this mesh not on my domain, so skipping it.\n";
//     rep.printVec("mesh xmin", iXmin, ndim);
//     rep.printVec("mesh xmax", iXmax, ndim);
//     rep.printVec("grid xmin", localmin, ndim);
//     rep.printVec("grid xmax", localmax, ndim);
//     return 0;
//   }

  //
  // So now part of the quadmesh intersects the local domain, so we
  // run through the data and pick out the ones we want.  Silo stores
  // the data in a big 1D array with elements stored in the order
  // D[NY*NY*iz+NX*iy+ix], so we go along x-columns, then y, then z.
  //
  //
  // Set a pointer to the data in the quadmesh
  //
  FAKE_DOUBLE **data = (FAKE_DOUBLE **)(qv->vals);

  //
  // Set variables to read, first check for vector and then scalar
  // data
  //
  int v1=-1, v2=-1, v3=-1;

  if (variable=="Velocity" || variable=="MagneticField") {
    if (variable=="Velocity") {v1=VX;v2=VY;v3=VZ;}
    else                      {v1=BX;v2=BY;v3=BZ;}
  }
  else {
    if      (variable=="Density")         v1=RO;
    else if (variable=="Pressure")        v1=PG;
    else if (variable=="VelocityX")       v1=VX;
    else if (variable=="VelocityY")       v1=VY;
    else if (variable=="VelocityZ")       v1=VZ;
    else if (variable=="MagneticFieldX")  v1=BX;
    else if (variable=="MagneticFieldY")  v1=BY;
    else if (variable=="MagneticFieldZ")  v1=BZ;
    else if (variable=="glmPSI")          v1=SI;
    //
    // Now loop over up to MAX_NVAR tracers...
    //
    else if (variable.substr(0,2)=="Tr") {
      int itr = atoi(variable.substr(2,3).c_str());
      if (!isfinite(itr) || itr<0 || itr>=MAX_NVAR) {
        rep.error("Bad diffuse Column-density identifier.",variable);
      }
      v1 = SimPM.ftr +itr;
    }
    //else if (variable.substr(0,3)=="Tr0") {v1=SimPM.ftr;}
    //else if (variable.substr(0,3)=="Tr1") {v1=SimPM.ftr+1;}
    //else if (variable.substr(0,3)=="Tr2") {v1=SimPM.ftr+2;}
    //else if (variable.substr(0,3)=="Tr3") {v1=SimPM.ftr+3;}
    //else if (variable.substr(0,3)=="Tr4") {v1=SimPM.ftr+4;}
    //else if (variable.substr(0,3)=="Tr5") {v1=SimPM.ftr+5;}
    //else if (variable.substr(0,3)=="Tr6") {v1=SimPM.ftr+6;}
    //else if (variable.substr(0,3)=="Tr7") {v1=SimPM.ftr+7;}
    //else if (variable.substr(0,3)=="Tr8") {v1=SimPM.ftr+8;}
    //else if (variable.substr(0,3)=="Tr9") {v1=SimPM.ftr+9;}
    else rep.error("what var to read???",variable);
  }
  //cout <<"vars=["<<v1<<", "<<v2<<", "<<v3<<"]\n";

  //
  // Get to first cell in the quadmesh/grid intersection region.
  //
  cell *c=ggg->FirstPt();

  for (int v=0;v<ndim;v++) {
    enum direction posdir = static_cast<direction>(2*v+1);

    while (c!=0 && c->pos[v]<iXmin[v]) {
      c=ggg->NextPt(c,posdir);
    }
    if (!c)
      rep.error("Went off end of grid looking for starting cell",iXmin[v]-ggg->FirstPt()->pos[v]);
  }


  //
  // Get the starting indices for the quadmesh -- the starting cell is
  // at least dx/2 greater than iXmin in every direction, and the
  // difference is an odd number for cell-size=2, so this integer
  // division will always get the right answer, since all compilers
  // round down for integer division of positive numbers.
  //
  int dx = CI.get_integer_cell_size();
  int qm_start[ndim];
  int qm_ix[ndim], qm_NX[ndim];
  for (int v=0;v<ndim;v++) {
    qm_start[v] = (c->pos[v]-iXmin[v])/dx;
    // cout <<"\t\tv="<<v<<" start="<<qm_start[v]<<" pos="<<c->pos[v]<< "xmin="<<iXmin[v]<<" dims="<<qv->dims[v]<<"\n";
    qm_ix[v] = qm_start[v];
    //
    // Get number of elements in each direction for this subdomain.
    // Can read it from the quadvar struct or else we could get it
    // from mpiPM.localNG[] I suppose...  N.B. qv->dims is the number
    // of data entries in each direction (by contrast quadmesh has
    // qm->dims[] = num.nodes = qv->dims[]+1).
    //
    qm_NX[v] = qv->dims[v];
  }
  
  class cell 
    *cx=c,
    *cy=c,
    *cz=c;
  long int ct=0;
  long int qv_index=0;

  while (cz!=0) {
    //
    // Trace an x-y plane in the current z-plane.
    // Set index, and reset the y-index counter
    //
    qm_ix[YY] = qm_start[YY];
    cy = cz;
    
    while (cy!=0) {
      //
      // Trace an x-column starting at the mesh start point.
      //
      cx = cy;
      //
      // Get to starting index [NX*NY*iz+NX*iy+ix]
      //
      qv_index = 0;
      if (ndim>2) qv_index += qm_NX[XX]*qm_NX[YY]*qm_ix[ZZ];
      if (ndim>1) qv_index += qm_NX[XX]*qm_ix[YY];
      qm_ix[XX] = qm_start[XX];
      qv_index += qm_ix[XX];
      
      while ((cx!=0) && cx->pos[XX]<iXmax[XX]) {
	cx->P[v1] = data[0][qv_index];
	if (v2>0) cx->P[v2] = data[1][qv_index];
	if (v3>0) cx->P[v3] = data[2][qv_index];
	//rep.printVec("Cell",cx->pos,ndim);
	cx = ggg->NextPt(cx,XP);
	qv_index++;
	qm_ix[XX] ++;
	ct++;
      } // x-column
      
      if (ndim>1) {
	//
	// move to next x-column in YP direction, if it exists, and if
	// it is on the mesh domain.  Also increment qm_ix[YY] to
	// indicate this.
	//
	cy = ggg->NextPt(cy,YP);
	if (cy!=0 && cy->pos[YY]>iXmax[YY]) cy = 0;
	qm_ix[YY] ++;
      }
      else {
	//
	// ndim==1, so we want to break out of the y-dir loop
	//
	cy = 0;
      }

    } // y-loop

    if (ndim>2) {
      //
      // move to next XY-plane in the ZP direction, if it exists and
      // if it is on the mesh domain.  Also increment the qm_ix[ZZ]
      // counter.
      //
      cz = ggg->NextPt(cz,ZP);
      if (cz!=0 && cz->pos[ZZ]>iXmax[ZZ]) cz = 0;
      qm_ix[ZZ] ++;
    }
    else {
      //
      // ndim<=2, so we want to break out of the z-dir loop
      //
      cz = 0;
    }
    
  } // z-loop. 


  //  cout <<"Read variable "<<variable<<"\n";
  DBFreeQuadvar(qv); //qv=0;
  data=0;
  return 0;
}


// ##################################################################
// ##################################################################



/********************************************************/
/*************** dataio_silo_utility ********************/
/********************************************************/