///
/// \file mpv5_molecular.h
/// \author Jonathan Mackey
/// \date 2013.02.15
///
/// This class is for modelling photoevaporation of dense molecular
/// clouds.
/// The file inherits from mpv3, and instead of the Wolfire et al.
/// (2003) neutral gas heating/cooling rates, it uses the Henney et
/// al. (2009) molecular cooling rate and heating rates.  So the only
/// significant difference is in the Ydot() function.
///
/// Modifications:
/// - getting it written: mods up until 2013.02.15


#ifndef MPV5_MOLECULAR_H
#define MPV5_MOLECULAR_H

#include "defines/functionality_flags.h"
#include "defines/testing_flags.h"

#ifdef  NEW_METALLICITY

#include "microphysics/mp_explicit_H.h"

class mpv5_molecular
  :
  public mp_explicit_H
{
  public:
  ///
  /// Constructor
  ///
  mpv5_molecular(
          const int,           ///< Total number of variables in state vector
	  const int,           ///< Number of tracer variables in state vector.
	  const std::string &, ///< List of what the tracer variables mean.
          struct which_physics * ///< extra physics stuff.
	  );

  ///
  /// Destructor
  ///
  ~mpv5_molecular();


  //---------------------------------------------------------------------------
  //-------------- FUNCTIONS DERIVED FROM BASE CLASS FOLLOW -------------------
  //---------------------------------------------------------------------------
  public:
  ///
  /// calculate dy/dt for the vector of y-values.
  ///
  virtual int ydot(
          double,         ///< current time (probably not needed for rate equations)
          const N_Vector, ///< current Y-value
          N_Vector,       ///< vector for Y-dot values
          const double *  ///< extra user-data vector, P, for evaluating ydot(y,t,p)
          );

  //---------------------------------------------------------------------------
  //-------------- END OF FUNCTIONS DERIVED FROM BASE CLASS -------------------
  //---------------------------------------------------------------------------
};





#endif // NEW_METALLICITY

#endif // MPV5_MOLECULAR_H


