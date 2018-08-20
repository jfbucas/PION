/// \file sim_control_SMR.h
/// 
/// \brief Declares grid parameter class, and grid methods classes.
/// 
/// \author Jonathan Mackey
/// 
/// Modifications :\n
/// - 2018.05.04 JM: worked on class.

#ifndef SIM_CONTROL_NESTED_H
#define SIM_CONTROL_NESTED_H

#include "defines/functionality_flags.h"
#include "defines/testing_flags.h"

#include "grid/grid_base_class.h"
#include "grid/uniform_grid.h"
#include "spatial_solvers/solver_eqn_base.h"
#include "dataIO/dataio_base.h"
#include "decomposition/MCMD_control.h"
#include "sim_control/sim_control.h" 
#include "sim_control/sim_init_SMR.h"
#include "sim_control/calc_timestep.h"
#include "sim_control/time_integrator.h"

/// 
/// This can integrate any system of equations if given the right solver class.
/// It can solve the equations in 1st or 2nd order accuracy in space and time.
///
class sim_control_SMR :
  virtual public sim_control,
  virtual public sim_init_SMR

{
  public:
  sim_control_SMR();  ///< Simple constructor
  virtual ~sim_control_SMR(); ///< Destructor

  ///
  /// Time integration
  ///
  /// This is the main part of the code -- It does all the time integration
  /// until the stopping condition is reached and then returns.
  /// It calls a sequence of functions to advance the time by one timestep,
  /// all in a loop which runs until end-of-sim is reached.
  ///
  virtual int Time_Int(
      vector<class GridBaseClass *> &  ///< address of vector of grid pointers.
      );

  ///
  /// finalise the simulation, clean up, delete data.
  /// This function finished the simulation gracefully (hopefully!).
  ///
  ///int Finalise(
  ///    vector<class GridBaseClass *> &  ///< address of vector of grid pointers.
  ///    );


   //---------------------------------------
  protected:

#ifdef BLAST_WAVE_CHECK
  ///
  /// If running a spherical blast wave, calculate the shock
  /// position and output to screen.
  ///
  void calculate_blastwave_radius(
      vector<class GridBaseClass *> &  ///< address of vector of grid pointers.
      );
#endif // BLAST_WAVE_CHECK


  ///
  /// Advance a grid by one time step for a given level in a SMR grid.
  /// This is a recursive function that calls itself on the next finer level
  /// if it exists.  This is a wrapper function that calls either the
  /// first-order or second-order update.
  ///
  virtual double advance_time(
      const int ///< level in SMR grid.
      );

  ///
  /// First-order-accurate time integration for a single step on level l.
  /// Returns the sum of delta-t for the timestep just completed, and the
  /// step to come.
  ///
  virtual double advance_step_OA1(
      const int ///< level in SMR grid.
      );

  ///
  /// Second-order-accurate time integration for a single step on level l.
  /// Returns the sum of delta-t for the timestep just completed, and the
  /// step to come.
  ///
  virtual double advance_step_OA2(
      const int ///< level in SMR grid.
      );

  ///
  /// Run through all diffuse and direct radiation sources and calculate column
  /// densities through the grid for each one.  Tau, DTau, and Vshell are stored
  /// in extra_data[i] for each cell.
  ///
  int calculate_raytracing_column_densities(
      class SimParams &,      ///< pointer to simulation parameters
      class GridBaseClass *,  ///< grid to trace rays on.
      const int               ///< level of SMR grid.
      );

  ///
  /// Checks Total energy relative to initial value, and prints a
  /// message if not.
  ///
  int check_energy_cons(
      vector<class GridBaseClass *> &  ///< address of vector of grid pointers.
      );

  ///
  /// This function takes the contents of each cell->dU[] vector and
  /// updates Ph[] the changes.  If we are on the full-step then it
  /// also updates P[] so that Ph[] is identical.
  /// For the SMR grid, it performs an additional step of
  /// correcting the fluxes for cells that have an interface above
  /// the boundary of a grid on the next finer level.
  ///
  int grid_update_state_vector(
      const double ,  ///< dt, timestep
      const int,      ///< TIMESTEP_FULL or TIMESTEP_FIRST_PART
      const int,       ///< Full order of accuracy of simulation
      class GridBaseClass * ///< grid pointer
      );

}; // sim_control_SMR
   
/*************************************************************************/
/*************************************************************************/
/*************************************************************************/


#endif // if not SIM_CONTROL_NESTED_H