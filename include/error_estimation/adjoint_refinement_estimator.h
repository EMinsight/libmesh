// The libMesh Finite Element Library.
// Copyright (C) 2002-2025 Benjamin S. Kirk, John W. Peterson, Roy H. Stogner

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA



#ifndef LIBMESH_ADJOINT_REFINEMENT_ESTIMATOR_H
#define LIBMESH_ADJOINT_REFINEMENT_ESTIMATOR_H

// Local Includes
#include "libmesh/error_estimator.h"
#include "libmesh/libmesh.h"
#include "libmesh/qoi_set.h"

// C++ includes
#include <cstddef>
#include <vector>

#ifdef LIBMESH_ENABLE_AMR

namespace libMesh
{

// Forward declarations
class DifferentiablePhysics;

/**
 * This class implements a "brute force" goal-oriented error
 * estimator which computes an estimate of error in a quantity of
 * interest based on the residual of the current coarse grid primal
 * solution as weighted against an adjoint solution on a uniformly
 * refined (in h and/or p, for an arbitrary number of levels) grid.
 *
 * \author Roy H. Stogner
 * \date 2009
 */
class AdjointRefinementEstimator : public ErrorEstimator
{
public:

  /**
   * Constructor.  Sets the most common default parameter values.
   */
  AdjointRefinementEstimator();

  /**
   * Copy/move ctor, copy/move assignment operator, and destructor are
   * all explicitly defaulted for this class.
   */
  AdjointRefinementEstimator (const AdjointRefinementEstimator &) = default;
  AdjointRefinementEstimator (AdjointRefinementEstimator &&) = default;
  AdjointRefinementEstimator & operator= (const AdjointRefinementEstimator &) = default;
  AdjointRefinementEstimator & operator= (AdjointRefinementEstimator &&) = default;
  virtual ~AdjointRefinementEstimator() = default;

  /**
   * Access to the QoISet (default: weight all QoIs equally) to use
   * when computing errors
   */
  QoISet & qoi_set() { return _qoi_set; }

  /**
   * Access to the QoISet (default: weight all QoIs equally) to use
   * when computing errors
   */
  const QoISet & qoi_set() const { return _qoi_set; }

  /**
   * This function does uniform refinements and an adjoint
   * solve to get an adjoint solution on each cell,
   * then estimates the error by finding the weighted residual
   * of the coarse solution with the fine adjoint solution.
   *
   * system.solve() and system.assembly() must be called, and so
   * should have no side effects.
   *
   * Only the provided system is solved on the refined mesh;
   * we don't support adjoint solves on loosely coupled
   * collections of Systems.
   *
   * The estimated error is output in the vector
   * \p error_per_cell
   */
  virtual void estimate_error (const System & system,
                               ErrorVector & error_per_cell,
                               const NumericVector<Number> * solution_vector = nullptr,
                               bool estimate_parent_error = false) override;

  /**
   * This is an accessor function to access the computed global
   * QoI error estimates
   */
  Number & get_global_QoI_error_estimate(unsigned int qoi_index)
  {
    return computed_global_QoI_errors[qoi_index];
  }

  virtual ErrorEstimatorType type() const override;

  /**
   * How many h refinements to perform to get the fine grid
   */
  unsigned char number_h_refinements;

  /**
   * How many p refinements to perform to get the fine grid
   */
  unsigned char number_p_refinements;

  /**
   * \returns A pointer to the DifferentiablePhysics object or \p nullptr if
   * no external Physics object is attached.
   */
  DifferentiablePhysics * get_residual_evaluation_physics()
  { return this->_residual_evaluation_physics; }

  /**
   * Set the _residual_evaluation_physics member to argument
   */
  void set_residual_evaluation_physics(DifferentiablePhysics* set_physics)
  { this->_residual_evaluation_physics = set_physics; }

  /**
   * \returns A pointer to the DifferentiablePhysics object or \p nullptr if
   * no external Physics object is attached.
   */
  DifferentiablePhysics * get_adjoint_evaluation_physics()
  { return this->_adjoint_evaluation_physics; }

  /**
   * Set the _adjoint_evaluation_physics member to argument
   */
  void set_adjoint_evaluation_physics(DifferentiablePhysics* set_physics)
  { this->_adjoint_evaluation_physics = set_physics; }

protected:

  /**
   * Pointer to object to use for physics assembly evaluations.
   * Defaults to nullptr for backwards compatibility.
   */
  DifferentiablePhysics * _residual_evaluation_physics;

  /**
   * Pointer to object to use for adjoint assembly.
   * Defaults to nullptr for backwards compatibility.
   */
  DifferentiablePhysics * _adjoint_evaluation_physics;

  /* A vector to hold the computed global QoI error estimate */
  std::vector<Number> computed_global_QoI_errors;

  /**
   * A QoISet to handle cases with multiple QoIs available
   */
  QoISet _qoi_set;
};

} // namespace libMesh

#endif // #ifdef LIBMESH_ENABLE_AMR

#endif // LIBMESH_ADJOINT_REFINEMENT_ESTIMATOR_H
