/// \file setup_fixed_grid_MPI.h
/// 
/// \brief Declares setup_fixed_grid class for MPI-parallelised grids.
/// 
/// \author Jonathan Mackey
///
/// Modifications :\n
/// - 2015.01.26 JM: split off from setup_fixed_grid.h

#ifndef SETUP_FIXED_GRID_MPI_H
#define SETUP_FIXED_GRID_MPI_H


#include "decomposition/MCMD_control.h"
#include "setup_fixed_grid.h"
#include "boundaries/assign_update_bcs_MPI.h"


#ifdef PARALLEL


/// 
/// This class reimplements some functions of setup_fixed_grid, so that they
/// work on multiple processors with the domain split between them.
/// 
class setup_fixed_grid_pllel :
  virtual public setup_fixed_grid,
  virtual public assign_update_bcs_MPI
{
  public:
   setup_fixed_grid_pllel();
   ~setup_fixed_grid_pllel();

  ///
  /// initialise the grid class with appropriate parameters.
  /// 
  /// This function sets up the appropriate grid; for parallel execution
  /// I need to define the domain of my grid, and then pass the appropriate
  /// parameters to the UniformGrid class.
  ///
  int setup_grid(
      class GridBaseClass **, ///< address of pointer to computational grid.
      class SimParams &,  ///< pointer to simulation parameters
      class MCMDcontrol *     ///< address of MCMD controller class.
      );

  ///
  /// Decide if I need to setup RT class, and do it if i need to.
  ///
  virtual int setup_raytracing(
      class SimParams &,  ///< pointer to simulation parameters
      class GridBaseClass *, ///< pointer to computational grid 
      class RayTracingBase * ///< pointer to raytracing class
      );

  protected:
  
  ///
  /// Set the boundary conditions string and initialise BC_bd
  ///
  virtual int setup_boundary_structs(
      class SimParams &,  ///< reference to SimParams list.
      class MCMDcontrol &,  ///< address of MCMD controller class.
      class GridBaseClass *grid ///< pointer to grid.
      );


}; // setup_fixed_grid_pllel

#endif // PARALLEL
#endif // if not SETUP_FIXED_GRID_MPI_H

