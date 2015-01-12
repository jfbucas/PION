///
/// \file reporting.h
/// \author Jonathan Mackey
///
/// This file declares the reporting class, for dealing with standard
/// input/output to screen.
///
/// Modifications:
/// - 2015.01.08 JM: created file, moved class from global.h


#ifndef REPORTING_H
#define REPORTING_H

#include <string>
#include <iostream>
#include <fstream>
#include <stdlib.h> 
#include "sim_params.h"

using namespace std;


///
/// Global Class For Writing Error messages and Reports.
///
/// This determines where to write different messages to.
///
class reporting {
  public:
  reporting(); ///< Default Constructor, write to std[out/err].
  ~reporting(); ///< Destructor.
   
  ///
  /// Redirects stdout/stderr to files in the path specified.
  ///
  int redirect(
    const string & ///< Location of files to write reporting to.
    );
   
  ///
  /// This exits the code cleanly if I know I have an error.
  ///
  ///
  /// This exits from the code, printing an error message and the  
  /// offending value.
  /// I would like to have this in reporting.cpp, but it's a template
  /// function, and the export keyword is not implemented in any
  /// compilers, so I have to have the definitions in the header.
  ///
  template<class T> inline void error(
        string msg, ///< Error Message.
        T err       ///< Value which is wrong.
        )
  {
  cerr <<msg<<"\t error code: "<<err<<" ...exiting.\n";
  cerr <<"\t timestep="<<SimPM.timestep<<" and simtime="<<SimPM.simtime<<endl;
  flush(cout);
  // set maxtime and then Finalise will output data in the current state.
  SimPM.maxtime=true;
  //if (integrator) {delete integrator; integrator=0;}
  //if (grid) {delete grid; grid=0;}
  //if (MP) {delete MP; MP=0;}
  //if (RT) {delete RT; RT=0;}
#ifdef PARALLEL
  if (COMM) {
    COMM->abort();
  }
#endif // PARALLEL
  exit(1);
  }

  ///
  /// Prints a warning message but don't stop execution.
  ///
  template<class T1, class T2> inline void warning(
        string msg,  ///< Message.
        T1 expected,///< Expected Value
        T2 found    ///< Received Value
        )
  {
    cout <<"WARNING: "<<msg<<"\t Expected: "<<expected;
    cout <<" but got "<<found<<endl;
    return;
  }

  ///
  /// Error if actual value is less than Expected Value.
  ///
  /// T1 and T2 must be compatible types for testing 'less-than' operation.
  ///
  template<class T1, class T2>inline void errorLT(
        string msg, ///< Message.
        T1 exptd,  ///< Expected Value
        T2 recvd   ///< Actual Value
        )
  {
    if(recvd<exptd) {
      cout<<"ERROR DETECTED: "<<msg<<"\t expected less than "<<exptd;
      cout <<" but got "<<recvd<<endl;
      error(msg,recvd);
      }
    return;
  }

  ///
  /// Tests if two values are the same, and error if not.
  ///
  /// If the expected value is not equal to the actual value, then
  /// print an error message and exit.
  ///
  inline void errorTest(
        string msg, ///< Error Message.
        int exptd, ///< Expected Value.
        int recvd  ///< Actual Value.
        )
  {
    if(exptd!=recvd) {
      cerr<<"ERROR DETECTED: "<<msg<<"\t expected "<<exptd;
      cerr <<" but got "<<recvd<<endl; 
      error(msg,recvd);
    }
    return;
  }

  ///
  /// Print out a vector.
  ///
  template<class T> inline void printVec(
        string msg, ///< Name of Vector
        T* vec,   ///<pointer to vector
        int nd    ///< length of vector.
        )
  {
    cout <<"Vector "<<msg<<" : [";
    for (int i=0;i<nd-1;i++)
      cout <<vec[i]<<", ";
    cout <<vec[nd-1]<<" ]\n";
    return;
  }

  private:
  ofstream errmsg, infomsg;//, iomsg;
  streambuf *saved_buffer_cout, *saved_buffer_cerr;
};
  
extern class reporting rep;


#endif // REPORTING_H

