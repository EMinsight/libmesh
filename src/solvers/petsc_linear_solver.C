// The libMesh Finite Element Library.
// Copyright (C) 2002-2024 Benjamin S. Kirk, John W. Peterson, Roy H. Stogner

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

#include "libmesh/libmesh_common.h"

#ifdef LIBMESH_HAVE_PETSC


// Local Includes
#include "libmesh/dof_map.h"
#include "libmesh/libmesh_logging.h"
#include "libmesh/petsc_linear_solver.h"
#include "libmesh/shell_matrix.h"
#include "libmesh/petsc_matrix.h"
#include "libmesh/petsc_preconditioner.h"
#include "libmesh/petsc_vector.h"
#include "libmesh/enum_to_string.h"
#include "libmesh/system.h"
#include "libmesh/petsc_auto_fieldsplit.h"
#include "libmesh/solver_configuration.h"
#include "libmesh/enum_preconditioner_type.h"
#include "libmesh/enum_solver_type.h"
#include "libmesh/enum_convergence_flags.h"

// C++ includes
#include <memory>
#include <string.h>

namespace libMesh
{

extern "C"
{
  PetscErrorCode libmesh_petsc_preconditioner_setup (PC pc)
  {
    PetscFunctionBegin;

    void * ctx;
    PetscErrorCode ierr = PCShellGetContext(pc,&ctx);CHKERRQ(ierr);
    Preconditioner<Number> * preconditioner = static_cast<Preconditioner<Number> *>(ctx);

    libmesh_error_msg_if(!preconditioner->initialized(),
                         "Preconditioner not initialized!  Make sure you call init() before solve!");

    preconditioner->setup();

    PetscFunctionReturn(LIBMESH_PETSC_SUCCESS);
  }

  PetscErrorCode libmesh_petsc_preconditioner_apply(PC pc, Vec x, Vec y)
  {
    PetscFunctionBegin;

    void * ctx;
    PetscErrorCode ierr = PCShellGetContext(pc,&ctx);CHKERRQ(ierr);
    Preconditioner<Number> * preconditioner = static_cast<Preconditioner<Number> *>(ctx);

    PetscVector<Number> x_vec(x, preconditioner->comm());
    PetscVector<Number> y_vec(y, preconditioner->comm());

    preconditioner->apply(x_vec,y_vec);

    PetscFunctionReturn(LIBMESH_PETSC_SUCCESS);
  }

#ifdef LIBMESH_ENABLE_DEPRECATED
  PetscErrorCode __libmesh_petsc_preconditioner_setup (PC pc)
  {
    PetscFunctionBegin;
    libmesh_deprecated();
    PetscFunctionReturn(libmesh_petsc_preconditioner_setup(pc));
  }

  PetscErrorCode __libmesh_petsc_preconditioner_apply(PC pc, Vec x, Vec y)
  {
    PetscFunctionBegin;
    libmesh_deprecated();
    PetscFunctionReturn(libmesh_petsc_preconditioner_apply(pc, x, y));
  }
#endif
} // end extern "C"



template <typename T>
PetscLinearSolver<T>::PetscLinearSolver(const libMesh::Parallel::Communicator & comm_in) :
  LinearSolver<T>(comm_in),
  _restrict_solve_to_is(nullptr),
  _restrict_solve_to_is_complement(nullptr),
  _subset_solve_mode(SUBSET_ZERO)
{
  if (this->n_processors() == 1)
    this->_preconditioner_type = ILU_PRECOND;
  else
    this->_preconditioner_type = BLOCK_JACOBI_PRECOND;
}



template <typename T>
void PetscLinearSolver<T>::clear ()
{
  if (this->initialized())
    {
      this->_is_initialized = false;

      // Calls specialized destroy() functions
      if (_restrict_solve_to_is)
        _restrict_solve_to_is.reset_to_zero();
      if (_restrict_solve_to_is_complement)
        _restrict_solve_to_is_complement.reset_to_zero();

      // Previously we only called KSPDestroy(), we did not reset _ksp
      // to nullptr, so that behavior is maintained here.
      _ksp.destroy();

      // Mimic PETSc default solver and preconditioner
      this->_solver_type = GMRES;

      if (!this->_preconditioner)
        {
          if (this->n_processors() == 1)
            this->_preconditioner_type = ILU_PRECOND;
          else
            this->_preconditioner_type = BLOCK_JACOBI_PRECOND;
        }
    }
}



template <typename T>
void PetscLinearSolver<T>::init (const char * name)
{
  // Initialize the data structures if not done so already.
  if (!this->initialized())
    {
      this->_is_initialized = true;

      PetscErrorCode ierr = LIBMESH_PETSC_SUCCESS;

      ierr = KSPCreate (this->comm().get(), _ksp.get());
      LIBMESH_CHKERR(ierr);

      if (name)
        {
          ierr = KSPSetOptionsPrefix(_ksp, name);
          LIBMESH_CHKERR(ierr);
        }

      // Create the preconditioner context
      ierr = KSPGetPC        (_ksp, &_pc);
      LIBMESH_CHKERR(ierr);

      // Set user-specified  solver and preconditioner types
      this->set_petsc_solver_type();

      // If the SolverConfiguration object is provided, use it to set
      // options during solver initialization.
      if (this->_solver_configuration)
        {
          this->_solver_configuration->set_options_during_init();
        }

      // Set the options from user-input
      // Set runtime options, e.g.,
      //      -ksp_type <type> -pc_type <type> -ksp_monitor -ksp_rtol <rtol>
      //  These options will override those specified above as long as
      //  KSPSetFromOptions() is called _after_ any other customization
      //  routines.
      ierr = KSPSetFromOptions (_ksp);
      LIBMESH_CHKERR(ierr);

      // Have the Krylov subspace method use our good initial guess
      // rather than 0, unless the user requested a KSPType of
      // preonly, which complains if asked to use initial guesses.
      KSPType ksp_type;

      ierr = KSPGetType (_ksp, &ksp_type);
      LIBMESH_CHKERR(ierr);

      if (strcmp(ksp_type, "preonly"))
        {
          ierr = KSPSetInitialGuessNonzero (_ksp, PETSC_TRUE);
          LIBMESH_CHKERR(ierr);
        }

      // Notify PETSc of location to store residual history.
      // This needs to be called before any solves, since
      // it sets the residual history length to zero.  The default
      // behavior is for PETSc to allocate (internally) an array
      // of size 1000 to hold the residual norm history.
      ierr = KSPSetResidualHistory(_ksp,
                                   LIBMESH_PETSC_NULLPTR, // pointer to the array which holds the history
                                   PETSC_DECIDE, // size of the array holding the history
                                   PETSC_TRUE);  // Whether or not to reset the history for each solve.
      LIBMESH_CHKERR(ierr);

      PetscPreconditioner<T>::set_petsc_preconditioner_type(this->_preconditioner_type,_pc);

      //If there is a preconditioner object we need to set the internal setup and apply routines
      if (this->_preconditioner)
        {
          this->_preconditioner->init();
          ierr = PCShellSetContext(_pc,(void *)this->_preconditioner);
          LIBMESH_CHKERR(ierr);
          ierr = PCShellSetSetUp(_pc,libmesh_petsc_preconditioner_setup);
          LIBMESH_CHKERR(ierr);
          ierr = PCShellSetApply(_pc,libmesh_petsc_preconditioner_apply);
          LIBMESH_CHKERR(ierr);
        }
    }
}


template <typename T>
void PetscLinearSolver<T>::init (PetscMatrixBase<T> * matrix,
                                 const char * name)
{
  // Initialize the data structures if not done so already.
  if (!this->initialized())
    {
      this->_is_initialized = true;

      PetscErrorCode ierr = LIBMESH_PETSC_SUCCESS;

      ierr = KSPCreate (this->comm().get(), _ksp.get());
      LIBMESH_CHKERR(ierr);

      if (name)
        {
          ierr = KSPSetOptionsPrefix(_ksp, name);
          LIBMESH_CHKERR(ierr);
        }

      // Create the preconditioner context
      ierr = KSPGetPC        (_ksp, &_pc);
      LIBMESH_CHKERR(ierr);

      // Set operators. The input matrix works as the preconditioning matrix
      ierr = KSPSetOperators(_ksp, matrix->mat(), matrix->mat());
      LIBMESH_CHKERR(ierr);

      // Set user-specified  solver and preconditioner types
      this->set_petsc_solver_type();

      // If the SolverConfiguration object is provided, use it to set
      // options during solver initialization.
      if (this->_solver_configuration)
        {
          this->_solver_configuration->set_options_during_init();
        }

      // Set the options from user-input
      // Set runtime options, e.g.,
      //      -ksp_type <type> -pc_type <type> -ksp_monitor -ksp_rtol <rtol>
      //  These options will override those specified above as long as
      //  KSPSetFromOptions() is called _after_ any other customization
      //  routines.
      ierr = KSPSetFromOptions (_ksp);
      LIBMESH_CHKERR(ierr);

      // Have the Krylov subspace method use our good initial guess
      // rather than 0, unless the user requested a KSPType of
      // preonly, which complains if asked to use initial guesses.
      KSPType ksp_type;

      ierr = KSPGetType (_ksp, &ksp_type);
      LIBMESH_CHKERR(ierr);

      if (strcmp(ksp_type, "preonly"))
        {
          ierr = KSPSetInitialGuessNonzero (_ksp, PETSC_TRUE);
          LIBMESH_CHKERR(ierr);
        }

      // Notify PETSc of location to store residual history.
      // This needs to be called before any solves, since
      // it sets the residual history length to zero.  The default
      // behavior is for PETSc to allocate (internally) an array
      // of size 1000 to hold the residual norm history.
      ierr = KSPSetResidualHistory(_ksp,
                                   LIBMESH_PETSC_NULLPTR, // pointer to the array which holds the history
                                   PETSC_DECIDE, // size of the array holding the history
                                   PETSC_TRUE);  // Whether or not to reset the history for each solve.
      LIBMESH_CHKERR(ierr);

      PetscPreconditioner<T>::set_petsc_preconditioner_type(this->_preconditioner_type,_pc);
      if (this->_preconditioner)
        {
          this->_preconditioner->set_matrix(*matrix);
          this->_preconditioner->init();
          ierr = PCShellSetContext(_pc,(void *)this->_preconditioner);
          LIBMESH_CHKERR(ierr);
          ierr = PCShellSetSetUp(_pc,libmesh_petsc_preconditioner_setup);
          LIBMESH_CHKERR(ierr);
          ierr = PCShellSetApply(_pc,libmesh_petsc_preconditioner_apply);
          LIBMESH_CHKERR(ierr);
        }
    }
}



template <typename T>
void
PetscLinearSolver<T>::init_names (const System & sys)
{
  petsc_auto_fieldsplit(this->pc(), sys);
}



template <typename T>
void
PetscLinearSolver<T>::restrict_solve_to (const std::vector<unsigned int> * const dofs,
                                         const SubsetSolveMode subset_solve_mode)
{
  PetscErrorCode ierr = LIBMESH_PETSC_SUCCESS;

  // The preconditioner (in particular if a default preconditioner)
  // will have to be reset.  We call this->clear() to do that.  This
  // call will also remove and free any previous subset that may have
  // been set before.
  this->clear();

  _subset_solve_mode = subset_solve_mode;

  if (dofs != nullptr)
    {
      PetscInt * petsc_dofs = nullptr;
      ierr = PetscMalloc(dofs->size()*sizeof(PetscInt), &petsc_dofs);
      LIBMESH_CHKERR(ierr);

      for (auto i : index_range(*dofs))
        petsc_dofs[i] = (*dofs)[i];

      // Create the IS
      // PETSc now takes over ownership of the "petsc_dofs"
      // array, so we don't have to worry about it any longer.
      ierr = ISCreateGeneral(this->comm().get(),
                             cast_int<PetscInt>(dofs->size()),
                             petsc_dofs, PETSC_OWN_POINTER,
                             _restrict_solve_to_is.get());
      LIBMESH_CHKERR(ierr);
    }
}



template <typename T>
std::pair<unsigned int, Real>
PetscLinearSolver<T>::solve (SparseMatrix<T> &  matrix_in,
                             SparseMatrix<T> &  precond_in,
                             NumericVector<T> & solution_in,
                             NumericVector<T> & rhs_in,
                             const std::optional<double> tol,
                             const std::optional<unsigned int> m_its)
{
  LOG_SCOPE("solve()", "PetscLinearSolver");

  const double rel_tol = this->get_real_solver_setting("rel_tol", tol);
  const double abs_tol = this->get_real_solver_setting("abs_tol",
                                                       std::nullopt,
                                                       static_cast<Real>(PETSC_DEFAULT));
  const double max_its = this->get_int_solver_setting("max_its", m_its);

  return this->solve_common(matrix_in, precond_in, solution_in,
                            rhs_in, rel_tol, abs_tol, max_its, KSPSolve);
}

template <typename T>
std::pair<unsigned int, Real>
PetscLinearSolver<T>::adjoint_solve (SparseMatrix<T> &  matrix_in,
                                     NumericVector<T> & solution_in,
                                     NumericVector<T> & rhs_in,
                                     const std::optional<double> tol,
                                     const std::optional<unsigned int> m_its)
{
  LOG_SCOPE("adjoint_solve()", "PetscLinearSolver");

  const double rel_tol = this->get_real_solver_setting("rel_tol", tol);
  const double abs_tol = this->get_real_solver_setting("abs_tol",
                                                       std::nullopt,
                                                       static_cast<Real>(PETSC_DEFAULT));
  const double max_its = this->get_int_solver_setting("max_its", m_its);

  // Note that the matrix and precond matrix are the same
  return this->solve_common(matrix_in, matrix_in, solution_in,
                            rhs_in, rel_tol, abs_tol, max_its, KSPSolveTranspose);
}


template <typename T>
std::pair<unsigned int, Real>
PetscLinearSolver<T>::solve_common (SparseMatrix<T> &  matrix_in,
                                    SparseMatrix<T> &  precond_in,
                                    NumericVector<T> & solution_in,
                                    NumericVector<T> & rhs_in,
                                    const double rel_tol,
                                    const double abs_tol,
                                    const unsigned int m_its,
                                    ksp_solve_func_type solve_func)
{
  // Make sure the data passed in are really of Petsc types
  PetscMatrixBase<T> * matrix   = cast_ptr<PetscMatrixBase<T> *>(&matrix_in);
  PetscMatrixBase<T> * precond  = cast_ptr<PetscMatrixBase<T> *>(&precond_in);

  this->init (matrix);

  // Close the matrices in case this wasn't already done.
  matrix->close ();
  if (precond != matrix)
    precond->close ();

  auto mat = matrix->mat();

  return this->solve_base
    (matrix, precond, mat, solution_in, rhs_in, rel_tol, abs_tol, m_its, solve_func);
}


template <typename T>
std::pair<unsigned int, Real>
PetscLinearSolver<T>::solve (const ShellMatrix<T> & shell_matrix,
                             NumericVector<T> & solution_in,
                             NumericVector<T> & rhs_in,
                             const std::optional<double> tol,
                             const std::optional<unsigned int> m_its)
{
  LOG_SCOPE("solve()", "PetscLinearSolver");

  const double rel_tol = this->get_real_solver_setting("rel_tol", tol);
  const double abs_tol = this->get_real_solver_setting("abs_tol",
                                                       std::nullopt,
                                                       static_cast<Real>(PETSC_DEFAULT));
  const double max_its = this->get_int_solver_setting("max_its", m_its);

  return this->shell_solve_common(shell_matrix, nullptr, solution_in,
                                  rhs_in, rel_tol, abs_tol, max_its);
}



template <typename T>
std::pair<unsigned int, Real>
PetscLinearSolver<T>::solve (const ShellMatrix<T> & shell_matrix,
                             const SparseMatrix<T> & precond_matrix,
                             NumericVector<T> & solution_in,
                             NumericVector<T> & rhs_in,
                             const std::optional<double> tol,
                             const std::optional<unsigned int> m_its)
{
  const double rel_tol = this->get_real_solver_setting("rel_tol", tol);
  const double abs_tol = this->get_real_solver_setting("abs_tol",
                                                       std::nullopt,
                                                       static_cast<Real>(PETSC_DEFAULT));
  const double max_its = this->get_int_solver_setting("max_its", m_its);

  // Make sure the data passed in are really of Petsc types
  const PetscMatrixBase<T> * precond  = cast_ptr<const PetscMatrixBase<T> *>(&precond_matrix);

  return this->shell_solve_common
    (shell_matrix, const_cast<PetscMatrixBase<T> *>(precond), solution_in,
     rhs_in, rel_tol, abs_tol, max_its);
}



template <typename T>
std::pair<unsigned int, Real>
PetscLinearSolver<T>::shell_solve_common (const ShellMatrix<T> & shell_matrix,
                                          PetscMatrixBase<T> * precond,
                                          NumericVector<T> & solution_in,
                                          NumericVector<T> & rhs_in,
                                          const double rel_tol,
                                          const double abs_tol,
                                          const unsigned int m_its)
{
  LOG_SCOPE("solve()", "PetscLinearSolver");

  // We don't really have a matrix for our matrix here
  SparseMatrix<T> * matrix = nullptr;

  this->init ();

  PetscErrorCode ierr = LIBMESH_PETSC_SUCCESS;

  // Prepare the matrix.
  WrappedPetsc<Mat> mat;
  ierr = MatCreateShell(this->comm().get(),
                        rhs_in.local_size(),
                        solution_in.local_size(),
                        rhs_in.size(),
                        solution_in.size(),
                        const_cast<void *>(static_cast<const void *>(&shell_matrix)),
                        mat.get());
  // Note that the const_cast above is only necessary because PETSc
  // does not accept a const void *.  Inside the member function
  // _petsc_shell_matrix() below, the pointer is casted back to a
  // const ShellMatrix<T> *.

  LIBMESH_CHKERR(ierr);
  ierr = MatShellSetOperation(mat, MATOP_MULT, reinterpret_cast<void(*)(void)>(_petsc_shell_matrix_mult));
  LIBMESH_CHKERR(ierr);
  ierr = MatShellSetOperation(mat, MATOP_MULT_ADD, reinterpret_cast<void(*)(void)>(_petsc_shell_matrix_mult_add));
  LIBMESH_CHKERR(ierr);
  ierr = MatShellSetOperation(mat, MATOP_GET_DIAGONAL, reinterpret_cast<void(*)(void)>(_petsc_shell_matrix_get_diagonal));
  LIBMESH_CHKERR(ierr);

  return this->solve_base
    (matrix, precond, mat, solution_in, rhs_in, rel_tol, abs_tol, m_its, KSPSolve);
}



template <typename T>
std::pair<unsigned int, Real>
PetscLinearSolver<T>::solve_base (SparseMatrix<T> * matrix,
                                  PetscMatrixBase<T> * precond,
                                  Mat mat,
                                  NumericVector<T> & solution_in,
                                  NumericVector<T> & rhs_in,
                                  const double rel_tol,
                                  const double abs_tol,
                                  const unsigned int m_its,
                                  ksp_solve_func_type solve_func)
{
  // Make sure the data passed in are really of Petsc types
  PetscVector<T> * solution = cast_ptr<PetscVector<T> *>(&solution_in);
  PetscVector<T> * rhs      = cast_ptr<PetscVector<T> *>(&rhs_in);

  // Close the vectors in case this wasn't already done.
  solution->close ();
  rhs->close ();

  PetscErrorCode ierr = LIBMESH_PETSC_SUCCESS;
  PetscInt its=0, max_its = static_cast<PetscInt>(m_its);
  PetscReal final_resid=0.;

  std::unique_ptr<PetscMatrixBase<Number>> subprecond_matrix;
  WrappedPetsc<Mat> subprecond;

  WrappedPetsc<Mat> submat;
  WrappedPetsc<Vec> subrhs;
  WrappedPetsc<Vec> subsolution;
  WrappedPetsc<VecScatter> scatter;

  // Restrict rhs and solution vectors and set operators.  The input
  // matrix works as the preconditioning matrix.
  if (_restrict_solve_to_is)
    {
      PetscInt is_local_size = this->restrict_solve_to_is_local_size();

      ierr = VecCreate(this->comm().get(), subrhs.get());
      LIBMESH_CHKERR(ierr);
      ierr = VecSetSizes(subrhs, is_local_size, PETSC_DECIDE);
      LIBMESH_CHKERR(ierr);
      ierr = VecSetFromOptions(subrhs);
      LIBMESH_CHKERR(ierr);

      ierr = VecCreate(this->comm().get(), subsolution.get());
      LIBMESH_CHKERR(ierr);
      ierr = VecSetSizes(subsolution, is_local_size, PETSC_DECIDE);
      LIBMESH_CHKERR(ierr);
      ierr = VecSetFromOptions(subsolution);
      LIBMESH_CHKERR(ierr);

      ierr = VecScatterCreate(rhs->vec(), _restrict_solve_to_is, subrhs, nullptr, scatter.get());
      LIBMESH_CHKERR(ierr);

      VecScatterBeginEnd(this->comm(), scatter, rhs->vec(), subrhs, INSERT_VALUES, SCATTER_FORWARD);
      VecScatterBeginEnd(this->comm(), scatter, solution->vec(), subsolution, INSERT_VALUES, SCATTER_FORWARD);

      ierr = LibMeshCreateSubMatrix(mat,
                                    _restrict_solve_to_is,
                                    _restrict_solve_to_is,
                                    MAT_INITIAL_MATRIX,
                                    submat.get());
      LIBMESH_CHKERR(ierr);

      if (precond)
        {
          ierr = LibMeshCreateSubMatrix(const_cast<PetscMatrixBase<T> *>(precond)->mat(),
                                        _restrict_solve_to_is,
                                        _restrict_solve_to_is,
                                        MAT_INITIAL_MATRIX,
                                        subprecond.get());
          LIBMESH_CHKERR(ierr);
        }

      // Since removing columns of the matrix changes the equation
      // system, we will now change the right hand side to compensate
      // for this.  Note that this is not necessary if \p SUBSET_ZERO
      // has been selected.
      if (_subset_solve_mode!=SUBSET_ZERO)
        {
          this->create_complement_is(rhs_in);
          PetscInt is_complement_local_size =
            cast_int<PetscInt>(rhs_in.local_size()-is_local_size);

          WrappedPetsc<Vec> subvec1;
          ierr = VecCreate(this->comm().get(), subvec1.get());
          LIBMESH_CHKERR(ierr);
          ierr = VecSetSizes(subvec1, is_complement_local_size, PETSC_DECIDE);
          LIBMESH_CHKERR(ierr);
          ierr = VecSetFromOptions(subvec1);
          LIBMESH_CHKERR(ierr);

          WrappedPetsc<VecScatter> scatter1;
          ierr = VecScatterCreate(rhs->vec(), _restrict_solve_to_is_complement, subvec1, nullptr, scatter1.get());
          LIBMESH_CHKERR(ierr);

          VecScatterBeginEnd(this->comm(), scatter1, _subset_solve_mode==SUBSET_COPY_RHS ? rhs->vec() : solution->vec(), subvec1, INSERT_VALUES, SCATTER_FORWARD);

          ierr = VecScale(subvec1, -1.0);
          LIBMESH_CHKERR(ierr);

          WrappedPetsc<Mat> submat1;
          ierr = LibMeshCreateSubMatrix(mat,
                                        _restrict_solve_to_is,
                                        _restrict_solve_to_is_complement,
                                        MAT_INITIAL_MATRIX,
                                        submat1.get());
          LIBMESH_CHKERR(ierr);

          ierr = MatMultAdd(submat1, subvec1, subrhs, subrhs);
          LIBMESH_CHKERR(ierr);
        }

      if (precond)
        ierr = KSPSetOperators(_ksp, submat, subprecond);
      else
        ierr = KSPSetOperators(_ksp, submat, submat);

      LIBMESH_CHKERR(ierr);

      PetscBool ksp_reuse_preconditioner = this->same_preconditioner ? PETSC_TRUE : PETSC_FALSE;
      ierr = KSPSetReusePreconditioner(_ksp, ksp_reuse_preconditioner);
      LIBMESH_CHKERR(ierr);

      if (precond && this->_preconditioner)
        {
          subprecond_matrix = std::make_unique<PetscMatrix<Number>>(subprecond, this->comm());
          this->_preconditioner->set_matrix(*subprecond_matrix);
          this->_preconditioner->init();
        }
    }
  else
    {
      PetscBool ksp_reuse_preconditioner = this->same_preconditioner ? PETSC_TRUE : PETSC_FALSE;
      ierr = KSPSetReusePreconditioner(_ksp, ksp_reuse_preconditioner);
      LIBMESH_CHKERR(ierr);

      if (precond)
        {
          ierr = KSPSetOperators(_ksp, mat, const_cast<PetscMatrixBase<T> *>(precond)->mat());
          LIBMESH_CHKERR(ierr);
        }
      else
        {
          ierr = KSPSetOperators(_ksp, mat, mat);
          LIBMESH_CHKERR(ierr);
        }

      if (this->_preconditioner)
        {
          if (matrix)
            this->_preconditioner->set_matrix(*matrix);
          else if (precond)
            this->_preconditioner->set_matrix(const_cast<PetscMatrixBase<Number> &>(*precond));

          this->_preconditioner->init();
        }
    }

  // Set the tolerances for the iterative solver.  Use the user-supplied
  // tolerance for the relative residual & leave the others at default values.
  ierr = KSPSetTolerances (_ksp, rel_tol, abs_tol,
                           PETSC_DEFAULT, max_its);
  LIBMESH_CHKERR(ierr);

  // Allow command line options to override anything set programmatically.
  ierr = KSPSetFromOptions(_ksp);
  LIBMESH_CHKERR(ierr);

  // If the SolverConfiguration object is provided, use it to override
  // solver options.
  if (this->_solver_configuration)
    {
      this->_solver_configuration->configure_solver();
    }

  // Solve the linear system
  if (_restrict_solve_to_is)
    {
      ierr = solve_func (_ksp, subrhs, subsolution);
      LIBMESH_CHKERR(ierr);
    }
  else
    {
      ierr = solve_func (_ksp, rhs->vec(), solution->vec());
      LIBMESH_CHKERR(ierr);
    }

  // Get the number of iterations required for convergence
  ierr = KSPGetIterationNumber (_ksp, &its);
  LIBMESH_CHKERR(ierr);

  // Get the norm of the final residual to return to the user.
  ierr = KSPGetResidualNorm (_ksp, &final_resid);
  LIBMESH_CHKERR(ierr);

  if (_restrict_solve_to_is)
    {
      switch(_subset_solve_mode)
        {
        case SUBSET_ZERO:
          ierr = VecZeroEntries(solution->vec());
          LIBMESH_CHKERR(ierr);
          break;

        case SUBSET_COPY_RHS:
          ierr = VecCopy(rhs->vec(),solution->vec());
          LIBMESH_CHKERR(ierr);
          break;

        case SUBSET_DONT_TOUCH:
          // Nothing to do here.
          break;

        default:
          libmesh_error_msg("Invalid subset solve mode = " << _subset_solve_mode);
        }

      VecScatterBeginEnd(this->comm(), scatter, subsolution, solution->vec(), INSERT_VALUES, SCATTER_REVERSE);

      if (precond && this->_preconditioner)
        {
          // Before subprecond_matrix gets cleaned up, we should give
          // the _preconditioner a different matrix.
          if (matrix)
            this->_preconditioner->set_matrix(*matrix);
          else
            this->_preconditioner->set_matrix(*precond);

          this->_preconditioner->init();
        }
    }

  // return the # of its. and the final residual norm.
  return std::make_pair(its, final_resid);
}



template <typename T>
KSP PetscLinearSolver<T>::ksp()
{
  this->init();
  return _ksp;
}

template <typename T>
void PetscLinearSolver<T>::get_residual_history(std::vector<double> & hist)
{
  PetscErrorCode ierr = LIBMESH_PETSC_SUCCESS;
  PetscInt its  = 0;

  // Fill the residual history vector with the residual norms
  // Note that GetResidualHistory() does not copy any values, it
  // simply sets the pointer p.  Note that for some Krylov subspace
  // methods, the number of residuals returned in the history
  // vector may be different from what you are expecting.  For
  // example, TFQMR returns two residual values per iteration step.

  // Recent versions of PETSc require the residual
  // history vector pointer to be declared as const.
#if PETSC_VERSION_LESS_THAN(3,15,0)
  PetscReal * p;
#else
  const PetscReal * p;
#endif

  ierr = KSPGetResidualHistory(_ksp, &p, &its);
  LIBMESH_CHKERR(ierr);

  // Check for early return
  if (its == 0) return;

  // Create space to store the result
  hist.resize(its);

  // Copy history into the vector provided by the user.
  for (PetscInt i=0; i<its; ++i)
    {
      hist[i] = *p;
      p++;
    }
}




template <typename T>
Real PetscLinearSolver<T>::get_initial_residual()
{
  PetscErrorCode ierr = LIBMESH_PETSC_SUCCESS;
  PetscInt its  = 0;

  // Fill the residual history vector with the residual norms
  // Note that GetResidualHistory() does not copy any values, it
  // simply sets the pointer p.  Note that for some Krylov subspace
  // methods, the number of residuals returned in the history
  // vector may be different from what you are expecting.  For
  // example, TFQMR returns two residual values per iteration step.

  // Recent versions of PETSc require the residual
  // history vector pointer to be declared as const.
#if PETSC_VERSION_LESS_THAN(3,15,0)
  PetscReal * p;
#else
  const PetscReal * p;
#endif


  ierr = KSPGetResidualHistory(_ksp, &p, &its);
  LIBMESH_CHKERR(ierr);

  // Check no residual history
  if (its == 0)
    {
      libMesh::err << "No iterations have been performed, returning 0." << std::endl;
      return 0.;
    }

  // Otherwise, return the value pointed to by p.
  return *p;
}




template <typename T>
void PetscLinearSolver<T>::set_petsc_solver_type()
{
  PetscErrorCode ierr = LIBMESH_PETSC_SUCCESS;

  switch (this->_solver_type)
    {

    case CG:
      ierr = KSPSetType (_ksp, const_cast<KSPType>(KSPCG));
      LIBMESH_CHKERR(ierr);
      return;

    case CR:
      ierr = KSPSetType (_ksp, const_cast<KSPType>(KSPCR));
      LIBMESH_CHKERR(ierr);
      return;

    case CGS:
      ierr = KSPSetType (_ksp, const_cast<KSPType>(KSPCGS));
      LIBMESH_CHKERR(ierr);
      return;

    case BICG:
      ierr = KSPSetType (_ksp, const_cast<KSPType>(KSPBICG));
      LIBMESH_CHKERR(ierr);
      return;

    case TCQMR:
      ierr = KSPSetType (_ksp, const_cast<KSPType>(KSPTCQMR));
      LIBMESH_CHKERR(ierr);
      return;

    case TFQMR:
      ierr = KSPSetType (_ksp, const_cast<KSPType>(KSPTFQMR));
      LIBMESH_CHKERR(ierr);
      return;

    case LSQR:
      ierr = KSPSetType (_ksp, const_cast<KSPType>(KSPLSQR));
      LIBMESH_CHKERR(ierr);
      return;

    case BICGSTAB:
      ierr = KSPSetType (_ksp, const_cast<KSPType>(KSPBCGS));
      LIBMESH_CHKERR(ierr);
      return;

    case MINRES:
      ierr = KSPSetType (_ksp, const_cast<KSPType>(KSPMINRES));
      LIBMESH_CHKERR(ierr);
      return;

    case GMRES:
      ierr = KSPSetType (_ksp, const_cast<KSPType>(KSPGMRES));
      LIBMESH_CHKERR(ierr);
      return;

    case RICHARDSON:
      ierr = KSPSetType (_ksp, const_cast<KSPType>(KSPRICHARDSON));
      LIBMESH_CHKERR(ierr);
      return;

    case CHEBYSHEV:
      ierr = KSPSetType (_ksp, const_cast<KSPType>(KSPCHEBYSHEV));
      LIBMESH_CHKERR(ierr);
      return;

    default:
      libMesh::err << "ERROR:  Unsupported PETSC Solver: "
                   << Utility::enum_to_string(this->_solver_type) << std::endl
                   << "Continuing with PETSC defaults" << std::endl;
    }
}



template <typename T>
LinearConvergenceReason PetscLinearSolver<T>::get_converged_reason() const
{
  KSPConvergedReason reason;
  auto ierr = KSPGetConvergedReason(_ksp, &reason);
  LIBMESH_CHKERR(ierr);

  switch(reason)
    {
    case KSP_CONVERGED_RTOL_NORMAL     : return CONVERGED_RTOL_NORMAL;
    case KSP_CONVERGED_ATOL_NORMAL     : return CONVERGED_ATOL_NORMAL;
    case KSP_CONVERGED_RTOL            : return CONVERGED_RTOL;
    case KSP_CONVERGED_ATOL            : return CONVERGED_ATOL;
    case KSP_CONVERGED_ITS             : return CONVERGED_ITS;
#if PETSC_VERSION_LESS_THAN(3,19,0)
    case KSP_CONVERGED_CG_NEG_CURVE    : return CONVERGED_CG_NEG_CURVE;
    // This was deprecated for STEP_LENGTH
    case KSP_CONVERGED_CG_CONSTRAINED  : return CONVERGED_CG_CONSTRAINED;
#else
    case KSP_CONVERGED_NEG_CURVE       : return CONVERGED_CG_NEG_CURVE;
#endif
    case KSP_CONVERGED_STEP_LENGTH     : return CONVERGED_STEP_LENGTH;
    case KSP_CONVERGED_HAPPY_BREAKDOWN : return CONVERGED_HAPPY_BREAKDOWN;
    case KSP_DIVERGED_NULL             : return DIVERGED_NULL;
    case KSP_DIVERGED_ITS              : return DIVERGED_ITS;
    case KSP_DIVERGED_DTOL             : return DIVERGED_DTOL;
    case KSP_DIVERGED_BREAKDOWN        : return DIVERGED_BREAKDOWN;
    case KSP_DIVERGED_BREAKDOWN_BICG   : return DIVERGED_BREAKDOWN_BICG;
    case KSP_DIVERGED_NONSYMMETRIC     : return DIVERGED_NONSYMMETRIC;
    case KSP_DIVERGED_INDEFINITE_PC    : return DIVERGED_INDEFINITE_PC;
    case KSP_DIVERGED_NANORINF         : return DIVERGED_NAN;
    case KSP_DIVERGED_INDEFINITE_MAT   : return DIVERGED_INDEFINITE_MAT;
    case KSP_CONVERGED_ITERATING       : return CONVERGED_ITERATING;
#if !PETSC_VERSION_LESS_THAN(3,7,0)
// PETSc-3.7.0 to 3.10.x
#if PETSC_VERSION_LESS_THAN(3,11,0) && PETSC_VERSION_RELEASE
    case KSP_DIVERGED_PCSETUP_FAILED   : return DIVERGED_PCSETUP_FAILED;
// PETSc master and future PETSc
#else
    case KSP_DIVERGED_PC_FAILED        : return DIVERGED_PCSETUP_FAILED;
#endif
#endif
    default :
      libMesh::err << "Unknown convergence flag!" << std::endl;
      return UNKNOWN_FLAG;
    }
}


template <typename T>
PetscErrorCode PetscLinearSolver<T>::_petsc_shell_matrix_mult(Mat mat, Vec arg, Vec dest)
{
  PetscFunctionBegin;

  // Get the matrix context.
  PetscErrorCode ierr = LIBMESH_PETSC_SUCCESS;
  void * ctx;
  ierr = MatShellGetContext(mat,&ctx);

  // Get user shell matrix object.
  const ShellMatrix<T> & shell_matrix = *static_cast<const ShellMatrix<T> *>(ctx);
  CHKERRABORT(shell_matrix.comm().get(), ierr);

  // Make \p NumericVector instances around the vectors.
  PetscVector<T> arg_global(arg, shell_matrix.comm());
  PetscVector<T> dest_global(dest, shell_matrix.comm());

  // Call the user function.
  shell_matrix.vector_mult(dest_global,arg_global);

  PetscFunctionReturn(ierr);
}



template <typename T>
PetscErrorCode PetscLinearSolver<T>::_petsc_shell_matrix_mult_add(Mat mat, Vec arg, Vec add, Vec dest)
{
  PetscFunctionBegin;

  // Get the matrix context.
  PetscErrorCode ierr = LIBMESH_PETSC_SUCCESS;
  void * ctx;
  ierr = MatShellGetContext(mat,&ctx);

  // Get user shell matrix object.
  const ShellMatrix<T> & shell_matrix = *static_cast<const ShellMatrix<T> *>(ctx);
  CHKERRABORT(shell_matrix.comm().get(), ierr);

  // Make \p NumericVector instances around the vectors.
  PetscVector<T> arg_global(arg, shell_matrix.comm());
  PetscVector<T> dest_global(dest, shell_matrix.comm());
  PetscVector<T> add_global(add, shell_matrix.comm());

  if (add!=arg)
    {
      arg_global = add_global;
    }

  // Call the user function.
  shell_matrix.vector_mult_add(dest_global,arg_global);

  PetscFunctionReturn(ierr);
}



template <typename T>
PetscErrorCode PetscLinearSolver<T>::_petsc_shell_matrix_get_diagonal(Mat mat, Vec dest)
{
  PetscFunctionBegin;

  // Get the matrix context.
  PetscErrorCode ierr = LIBMESH_PETSC_SUCCESS;
  void * ctx;
  ierr = MatShellGetContext(mat,&ctx);

  // Get user shell matrix object.
  const ShellMatrix<T> & shell_matrix = *static_cast<const ShellMatrix<T> *>(ctx);
  CHKERRABORT(shell_matrix.comm().get(), ierr);

  // Make \p NumericVector instances around the vector.
  PetscVector<T> dest_global(dest, shell_matrix.comm());

  // Call the user function.
  shell_matrix.get_diagonal(dest_global);

  PetscFunctionReturn(ierr);
}



template <typename T>
void
PetscLinearSolver<T>::create_complement_is (const NumericVector<T> & vec_in)
{
  libmesh_assert(_restrict_solve_to_is);
  if (!_restrict_solve_to_is_complement)
    {
      PetscErrorCode ierr = ISComplement(_restrict_solve_to_is,
                                         vec_in.first_local_index(),
                                         vec_in.last_local_index(),
                                         _restrict_solve_to_is_complement.get());
      LIBMESH_CHKERR(ierr);
    }
}


template <typename T>
PetscInt
PetscLinearSolver<T>::restrict_solve_to_is_local_size() const
{
  libmesh_assert(_restrict_solve_to_is);

  PetscInt s;
  auto ierr = ISGetLocalSize(_restrict_solve_to_is, &s);
  LIBMESH_CHKERR(ierr);

  return s;
}


//------------------------------------------------------------------
// Explicit instantiations
template class LIBMESH_EXPORT PetscLinearSolver<Number>;

} // namespace libMesh



#endif // #ifdef LIBMESH_HAVE_PETSC
