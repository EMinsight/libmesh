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


#include "libmesh/twostep_time_solver.h"

#include "libmesh/adjoint_refinement_estimator.h"
#include "libmesh/diff_system.h"
#include "libmesh/enum_norm_type.h"
#include "libmesh/error_vector.h"
#include "libmesh/euler_solver.h"
#include "libmesh/int_range.h"
#include "libmesh/numeric_vector.h"
#include "libmesh/parameter_vector.h"
#include "libmesh/sensitivity_data.h"
#include "libmesh/solution_history.h"

// C++ includes
#include <memory>

namespace libMesh
{



TwostepTimeSolver::TwostepTimeSolver (sys_type & s)
  : AdaptiveTimeSolver(s)

{
  // We start with a reasonable time solver: implicit Euler
  core_time_solver = std::make_unique<EulerSolver>(s);
}



TwostepTimeSolver::~TwostepTimeSolver () = default;



void TwostepTimeSolver::solve()
{
  libmesh_assert(core_time_solver.get());

  // All actual solution history operations are handled by the outer
  // solver, so the outer solver has to call advance_timestep and
  // call solution_history->store to store the initial conditions
  if (first_solve)
    {
      advance_timestep();
      first_solve = false;
    }

  // We may have to repeat timesteps entirely if our error is bad
  // enough
  bool max_tolerance_met = false;

  // Calculating error values each time
  Real single_norm(0.), double_norm(0.), error_norm(0.),
    relative_error(0.);

  // The loop below will change system time and deltat based on calculations.
  // We will need to save these for calculating the deltat for the next timestep
  // after the while loop has converged.
  Real old_time;
  Real old_deltat;

  while (!max_tolerance_met)
    {
      // If we've been asked to reduce deltat if necessary, make sure
      // the core timesolver does so
      core_time_solver->reduce_deltat_on_diffsolver_failure =
        this->reduce_deltat_on_diffsolver_failure;

      if (!quiet)
        {
          libMesh::out << "\n === Computing adaptive timestep === "
                       << std::endl;
        }

      // Use the double-length timestep first (so the
      // old_nonlinear_solution won't have to change)
      core_time_solver->solve();

      // Save a copy of the double-length nonlinear solution
      // and the old nonlinear solution
      std::unique_ptr<NumericVector<Number>> double_solution =
        _system.solution->clone();
      std::unique_ptr<NumericVector<Number>> old_solution =
        _system.get_vector("_old_nonlinear_solution").clone();

      double_norm = calculate_norm(_system, *double_solution);
      if (!quiet)
        {
          libMesh::out << "Double norm = " << double_norm << std::endl;
        }

      // Then reset the initial guess for our single-length calcs
      *(_system.solution) = _system.get_vector("_old_nonlinear_solution");

      // Call two single-length timesteps
      // Be sure that the core_time_solver does not change the
      // timestep here.  (This is unlikely because it just succeeded
      // with a timestep twice as large!)
      // FIXME: even if diffsolver failure is unlikely, we ought to
      // do *something* if it happens
      core_time_solver->reduce_deltat_on_diffsolver_failure = 0;

      old_time = _system.time;
      old_deltat = _system.deltat;
      _system.deltat *= 0.5;

      // Attempt the 'half timestep solve'
      core_time_solver->solve();

      // If we successfully completed the solve, let the time solver know the deltat used
      this->last_deltat = _system.deltat;

      // Increment system.time, and save the half solution to solution history
      core_time_solver->advance_timestep();

      core_time_solver->solve();

      single_norm = calculate_norm(_system, *_system.solution);
      if (!quiet)
        {
          libMesh::out << "Single norm = " << single_norm << std::endl;
        }

      // Reset the core_time_solver's reduce_deltat... value.
      core_time_solver->reduce_deltat_on_diffsolver_failure =
        this->reduce_deltat_on_diffsolver_failure;

      // Find the relative error
      *double_solution -= *(_system.solution);
      error_norm  = calculate_norm(_system, *double_solution);
      relative_error = error_norm / old_deltat /
        std::max(double_norm, single_norm);

      // If the relative error makes no sense, we're done
      if (!double_norm && !single_norm)
        return;

      if (!quiet)
        {
          libMesh::out << "Error norm = " << error_norm << std::endl;
          libMesh::out << "Local relative error = "
                       << (error_norm /
                           std::max(double_norm, single_norm))
                       << std::endl;
          libMesh::out << "Global relative error = "
                       << (error_norm / old_deltat /
                           std::max(double_norm, single_norm))
                       << std::endl;
          libMesh::out << "old delta t = " << old_deltat << std::endl;
        }

      // If our upper tolerance is negative, that means we want to set
      // it based on the first successful time step
      if (this->upper_tolerance < 0)
        this->upper_tolerance = -this->upper_tolerance * relative_error;

      // If we haven't met our upper error tolerance, we'll have to
      // repeat this timestep entirely
      if (this->upper_tolerance && relative_error > this->upper_tolerance)
        {
          // If we are saving solution histories, the core time solver
          // will save half solutions, and these solves can be attempted
          // repeatedly. If we failed to meet the tolerance, erase the
          // half solution from solution history.
          core_time_solver->get_solution_history().erase(_system.time);

          // We will be retrying this timestep with deltat/2, so restore
          // all the necessary state.
          // FIXME: this probably doesn't work with multistep methods
          _system.get_vector("_old_nonlinear_solution") = *old_solution;
          _system.time = old_time;
          _system.deltat = old_deltat;

          // Update to localize the old nonlinear solution
          core_time_solver->update();

          // Reset the initial guess for our next try
          *(_system.solution) =
            _system.get_vector("_old_nonlinear_solution");

          // Chop delta t in half
          _system.deltat /= 2.;

          if (!quiet)
            {
              libMesh::out << "Failed to meet upper error tolerance"
                           << std::endl;
              libMesh::out << "Retrying with delta t = "
                           << _system.deltat << std::endl;
            }
        }
      else
        max_tolerance_met = true;

    }

  // We ended up taking two half steps of size system.deltat to
  // march our last time step.
  this->last_deltat = _system.deltat;
  this->completed_timestep_size = 2.0*_system.deltat;

  // TimeSolver::solve methods should leave system.time unchanged
  _system.time = old_time;

  // Compare the relative error to the tolerance and adjust deltat
  _system.deltat = old_deltat;

  // If our target tolerance is negative, that means we want to set
  // it based on the first successful time step
  if (this->target_tolerance < 0)
    this->target_tolerance = -this->target_tolerance * relative_error;

  const Real global_shrink_or_growth_factor =
    std::pow(this->target_tolerance / relative_error,
             static_cast<Real>(1. / core_time_solver->error_order()));

  const Real local_shrink_or_growth_factor =
    std::pow(this->target_tolerance /
             (error_norm/std::max(double_norm, single_norm)),
             static_cast<Real>(1. / (core_time_solver->error_order()+1.)));

  if (!quiet)
    {
      libMesh::out << "The global growth/shrink factor is: "
                   << global_shrink_or_growth_factor << std::endl;
      libMesh::out << "The local growth/shrink factor is: "
                   << local_shrink_or_growth_factor << std::endl;
    }

  // The local s.o.g. factor is based on the expected **local**
  // truncation error for the timestepping method, the global
  // s.o.g. factor is based on the method's **global** truncation
  // error.  You can shrink/grow the timestep to attempt to satisfy
  // either a global or local time-discretization error tolerance.

  Real shrink_or_growth_factor =
    this->global_tolerance ? global_shrink_or_growth_factor :
    local_shrink_or_growth_factor;

  if (this->max_growth && this->max_growth < shrink_or_growth_factor)
    {
      if (!quiet && this->global_tolerance)
        {
          libMesh::out << "delta t is constrained by max_growth" << std::endl;
        }
      shrink_or_growth_factor = this->max_growth;
    }

  _system.deltat *= shrink_or_growth_factor;

  // Restrict deltat to max-allowable value if necessary
  if ((this->max_deltat != 0.0) && (_system.deltat > this->max_deltat))
    {
      if (!quiet)
        {
          libMesh::out << "delta t is constrained by maximum-allowable delta t."
                       << std::endl;
        }
      _system.deltat = this->max_deltat;
    }

  // Restrict deltat to min-allowable value if necessary
  if ((this->min_deltat != 0.0) && (_system.deltat < this->min_deltat))
    {
      if (!quiet)
        {
          libMesh::out << "delta t is constrained by minimum-allowable delta t."
                       << std::endl;
        }
      _system.deltat = this->min_deltat;
    }

  if (!quiet)
    {
      libMesh::out << "new delta t = " << _system.deltat << std::endl;
    }
}

std::pair<unsigned int, Real> TwostepTimeSolver::adjoint_solve (const QoISet & qoi_indices)
{
  // Take the first adjoint 'half timestep'
  core_time_solver->adjoint_solve(qoi_indices);

  // We print the forward 'half solution' norms and we will do so for the adjoints if running in dbg.
  #ifdef DEBUG
    for(auto i : make_range(_system.n_qois()))
    {
      for(auto j : make_range(_system.n_vars()))
        libMesh::out<<"||Z_"<<"("<<_system.time<<";"<<i<<","<<j<<")||_H1: "<<_system.calculate_norm(_system.get_adjoint_solution(i), j,H1)<<std::endl;
    }
  #endif

  // Record the sub step deltat we used for the last adjoint solve.
  last_deltat = _system.deltat;

  // Adjoint advance the timestep
  core_time_solver->adjoint_advance_timestep();

 // We have to contend with the fact that the delta_t set by SolutionHistory will not be the
 // delta_t for the adjoint solve. At time t_i, the adjoint solve uses the same delta_t
 // as the primal solve, pulling the adjoint solution from t_i+1 to t_i.
 // FSH however sets delta_t to the value which takes us from t_i to t_i-1.
 // Therefore use the last_deltat for the solve and reset system delta_t after the solve.
  Real temp_deltat = _system.deltat;
  _system.deltat = last_deltat;

  // The second half timestep
  std::pair<unsigned int, Real> full_adjoint_output = core_time_solver->adjoint_solve(qoi_indices);

  // Record the sub step deltat we used for the last adjoint solve and reset the system deltat to the
  // value set by SolutionHistory.
  last_deltat = _system.deltat;
  _system.deltat = temp_deltat;

  // Record the total size of the last timestep, for a 2StepTS, this is
  // simply twice the deltat for each sub(half) step.
  this->completed_timestep_size = 2.0*_system.deltat;

  return full_adjoint_output;
}

void TwostepTimeSolver::integrate_qoi_timestep()
{
  // Vectors to hold qoi contributions from the first and second half timesteps
  std::vector<Number> qois_first_half(_system.n_qois(), 0.0);
  std::vector<Number> qois_second_half(_system.n_qois(), 0.0);

  // First half contribution
  core_time_solver->integrate_qoi_timestep();

  for (auto j : make_range(_system.n_qois()))
  {
    qois_first_half[j] = _system.get_qoi_value(j);
  }

  // Second half contribution
  core_time_solver->integrate_qoi_timestep();

  for (auto j : make_range(_system.n_qois()))
  {
    qois_second_half[j] = _system.get_qoi_value(j);
  }

  // Zero out the system.qoi vector
  for (auto j : make_range(_system.n_qois()))
  {
    _system.set_qoi(j, 0.0);
  }

  // Add the contributions from the two halftimesteps to get the full QoI
  // contribution from this timestep
  for (auto j : make_range(_system.n_qois()))
  {
    _system.set_qoi(j, qois_first_half[j] + qois_second_half[j]);
  }
}

void TwostepTimeSolver::integrate_adjoint_sensitivity(const QoISet & qois, const ParameterVector & parameter_vector, SensitivityData & sensitivities)
{
  // We are using the trapezoidal rule to integrate each timestep, and then pooling the contributions here.
  // (f(t_j) + f(t_j+1/2))/2 (t_j+1/2 - t_j) + (f(t_j+1/2) + f(t_j+1))/2 (t_j+1 - t_j+1/2)

  // First half timestep
  SensitivityData sensitivities_first_half(qois, _system, parameter_vector);

  core_time_solver->integrate_adjoint_sensitivity(qois, parameter_vector, sensitivities_first_half);

  // Second half timestep
  SensitivityData sensitivities_second_half(qois, _system, parameter_vector);

  core_time_solver->integrate_adjoint_sensitivity(qois, parameter_vector, sensitivities_second_half);

  // Get the contributions for each sensitivity from this timestep
  const auto pv_size = parameter_vector.size();
  for (auto i : make_range(qois.size(_system)))
    for (auto j : make_range(pv_size))
      sensitivities[i][j] = sensitivities_first_half[i][j] + sensitivities_second_half[i][j];
}

#ifdef LIBMESH_ENABLE_AMR
void TwostepTimeSolver::integrate_adjoint_refinement_error_estimate(AdjointRefinementEstimator & adjoint_refinement_error_estimator, ErrorVector & QoI_elementwise_error)
{
  // We use a numerical integration scheme consistent with the theta used for the timesolver.

  // Create first and second half error estimate vectors of the right size
  std::vector<Number> qoi_error_estimates_first_half(_system.n_qois());
  std::vector<Number> qoi_error_estimates_second_half(_system.n_qois());

  // First half timestep
  ErrorVector QoI_elementwise_error_first_half;

  core_time_solver->integrate_adjoint_refinement_error_estimate(adjoint_refinement_error_estimator, QoI_elementwise_error_first_half);

  // Also get the first 'half step' spatially integrated errors for all the QoIs in the QoI set
  for (auto j : make_range(_system.n_qois()))
  {
    // Skip this QoI if not in the QoI Set
    if (adjoint_refinement_error_estimator.qoi_set().has_index(j))
    {
      qoi_error_estimates_first_half[j] = _system.get_qoi_error_estimate_value(j);
    }
  }

  // Second half timestep
  ErrorVector QoI_elementwise_error_second_half;

  core_time_solver->integrate_adjoint_refinement_error_estimate(adjoint_refinement_error_estimator, QoI_elementwise_error_second_half);

  // Also get the second 'half step' spatially integrated errors for all the QoIs in the QoI set
  for (auto j : make_range(_system.n_qois()))
  {
    // Skip this QoI if not in the QoI Set
    if (adjoint_refinement_error_estimator.qoi_set().has_index(j))
    {
      qoi_error_estimates_second_half[j] = _system.get_qoi_error_estimate_value(j);
    }
  }

  // Error contribution from this timestep
  for (auto i : index_range(QoI_elementwise_error))
    QoI_elementwise_error[i] = QoI_elementwise_error_first_half[i] + QoI_elementwise_error_second_half[i];

  for (auto j : make_range(_system.n_qois()))
  {
    // Skip this QoI if not in the QoI Set
    if (adjoint_refinement_error_estimator.qoi_set().has_index(j))
    {
      _system.set_qoi_error_estimate(j, qoi_error_estimates_first_half[j] + qoi_error_estimates_second_half[j]);
    }
  }
}
#endif // LIBMESH_ENABLE_AMR

} // namespace libMesh
