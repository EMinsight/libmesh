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



// <h1>Vector Finite Elements Example 3 - Nedelec Elements</h1>
// \author Paul Bauman
// \date 2012
//
// This example shows an example of using the Nedelec elements of the
// first type to solve a model problem in H(curl).

// Basic include files
#include "libmesh/equation_systems.h"
#include "libmesh/getpot.h"
#include "libmesh/exodusII_io.h"
#include "libmesh/mesh.h"
#include "libmesh/mesh_generation.h"
#include "libmesh/mesh_modification.h"
#include "libmesh/mesh_refinement.h"
#include "libmesh/exact_solution.h"
#include "libmesh/string_to_enum.h"
#include "libmesh/enum_solver_package.h"
#include "libmesh/enum_norm_type.h"

// The systems and solvers we may use
#include "curl_curl_system.h"
#include "libmesh/diff_solver.h"
#include "libmesh/steady_solver.h"
#include "solution_function.h"

// Bring in everything from the libMesh namespace
using namespace libMesh;

// Define static data member holding (optional) rotation matrix
RealTensor CurlCurlExactSolution::R;

// The main program.
int main (int argc, char ** argv)
{
  // Initialize libMesh.
  LibMeshInit init (argc, argv);

  // This example requires a linear solver package.
  libmesh_example_requires(libMesh::default_solver_package() != INVALID_SOLVER_PACKAGE,
                           "--enable-petsc, --enable-trilinos, or --enable-eigen");

  // Parse the input file
  GetPot infile("vector_fe_ex3.in");

  // But allow the command line to override it.
  infile.parse_command_line(argc, argv);

  // Read in parameters from the input file
  const unsigned int grid_size = infile("grid_size", 2);

  // Skip higher-dimensional examples on a lower-dimensional libMesh build
  libmesh_example_requires(2 <= LIBMESH_DIM, "2D support");

  // Create a mesh, with dimension to be overridden later, on the
  // default MPI communicator.
  Mesh mesh(init.comm());

  // Use the MeshTools::Generation mesh generator to create a uniform
  // grid on the square [-1,1]^D. We must use at least TRI6 or QUAD8 elements
  // for the Nedelec triangle or quadrilateral elements, respectively.
  const std::string elem_str = infile("element_type", std::string("TRI6"));

  libmesh_error_msg_if(elem_str != "TRI6" && elem_str != "TRI7" && elem_str != "QUAD8" && elem_str != "QUAD9",
                       "You selected: " << elem_str <<
                       " but this example must be run with TRI6, TRI7, QUAD8, or QUAD9.");

  MeshTools::Generation::build_square (mesh,
                                       grid_size,
                                       grid_size,
                                       -1., 1.,
                                       -1., 1.,
                                       Utility::string_to_enum<ElemType>(elem_str));

  // Make sure the code is robust against nodal reorderings.
  MeshTools::Modification::permute_elements(mesh);

#ifdef LIBMESH_ENABLE_AMR
  // Make sure the code is robust against mesh refinements.
  MeshRefinement mesh_refinement(mesh);
  mesh_refinement.uniformly_refine(infile("refine", 0));
#endif

  // Make sure the code is robust against solves on 2d meshes rotated out of
  // the xy plane. By default, all Euler angles are zero, the rotation matrix
  // is the identity, and the mesh stays in place.
  const Real phi = infile("phi", 0.), theta = infile("theta", 0.), psi = infile("psi", 0.);
  CurlCurlExactSolution::RM(MeshTools::Modification::rotate(mesh, phi, theta, psi));

  // Print information about the mesh to the screen.
  mesh.print_info();

  // Create an equation systems object.
  EquationSystems equation_systems (mesh);

  // Declare the system "CurlCurl" and its variables.
  CurlCurlSystem & system =
    equation_systems.add_system<CurlCurlSystem> ("CurlCurl");

  // Set the FE approximation order.
  const Order order = static_cast<Order>(infile("order", 1u));

  libmesh_error_msg_if(order < FIRST || order > FIFTH,
                       "You selected: " << order <<
                       " but this example must be run with 1 <= order <= 5.");

  system.order(order);

  // This example only implements the steady-state problem
  system.time_solver = std::make_unique<SteadySolver>(system);

  // Initialize the system
  equation_systems.init();

  // And the nonlinear solver options
  DiffSolver & solver = *(system.time_solver->diff_solver().get());
  solver.quiet = infile("solver_quiet", true);
  solver.verbose = !solver.quiet;
  solver.max_nonlinear_iterations = infile("max_nonlinear_iterations", 15);
  solver.relative_step_tolerance = infile("relative_step_tolerance", 1.e-3);
  solver.relative_residual_tolerance = infile("relative_residual_tolerance", 1.0e-13);
  solver.absolute_residual_tolerance = infile("absolute_residual_tolerance", 0.0);

  // And the linear solver options
  solver.max_linear_iterations = infile("max_linear_iterations", 50000);
  solver.initial_linear_tolerance = infile("initial_linear_tolerance", 1.e-10);

  // Print information about the system to the screen.
  equation_systems.print_info();

  // Solve the system.
  system.solve();

  ExactSolution exact_sol(equation_systems);

  SolutionFunction soln_func(system.variable_number("u"));
  SolutionGradient soln_grad(system.variable_number("u"));

  // Build FunctionBase* containers to attach to the ExactSolution object.
  std::vector<FunctionBase<Number> *> sols(1, &soln_func);
  std::vector<FunctionBase<Gradient> *> grads(1, &soln_grad);

  exact_sol.attach_exact_values(sols);
  exact_sol.attach_exact_derivs(grads);

  // Use higher quadrature order for more accurate error results
  int extra_error_quadrature = infile("extra_error_quadrature", 2);
  exact_sol.extra_quadrature_order(extra_error_quadrature);

  // Compute the error.
  exact_sol.compute_error("CurlCurl", "u");

  // Print out the error values
  libMesh::out << "L2-Error is: "
               << exact_sol.l2_error("CurlCurl", "u")
               << std::endl;
  libMesh::out << "HCurl semi-norm error is: "
               << exact_sol.error_norm("CurlCurl", "u", HCURL_SEMINORM)
               << std::endl;
  libMesh::out << "HCurl-Error is: "
               << exact_sol.hcurl_error("CurlCurl", "u")
               << std::endl;

#ifdef LIBMESH_HAVE_EXODUS_API

  // We write the file in the ExodusII format.
  ExodusII_IO(mesh).write_equation_systems("out.e", equation_systems);

#endif // #ifdef LIBMESH_HAVE_EXODUS_API

  // All done.
  return 0;
}
