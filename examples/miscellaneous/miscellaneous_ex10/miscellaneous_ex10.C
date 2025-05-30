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



// <h1>Miscellaneous Example 10 - Stitching meshes </h1>
// \author Dana Christen
// \date 2014
//
// This example shows how to generate a domain by stitching 8 cubic meshes
// together.  Then a Poisson problem is solved on the stitched domain,
// and compared to the Poisson problem on a reference (unstitched) mesh
// to verify that the results match.


// C++ include files that we need
#include <iostream>
#include <algorithm>
#include <math.h>
#include <set>
#include <sstream>
#include <fstream>

// libMesh includes
#include "libmesh/libmesh.h"
#include "libmesh/mesh.h"
#include "libmesh/mesh_generation.h"
#include "libmesh/linear_implicit_system.h"
#include "libmesh/equation_systems.h"
#include "libmesh/exact_solution.h"
#include "libmesh/exodusII_io.h"
#include "libmesh/fe.h"
#include "libmesh/quadrature_gauss.h"
#include "libmesh/dof_map.h"
#include "libmesh/sparse_matrix.h"
#include "libmesh/numeric_vector.h"
#include "libmesh/dense_matrix.h"
#include "libmesh/dense_vector.h"
#include "libmesh/elem.h"
#include "libmesh/dirichlet_boundaries.h"
#include "libmesh/zero_function.h"
#include "libmesh/libmesh_logging.h"
#include "libmesh/getpot.h"
#include "libmesh/error_vector.h"
#include "libmesh/kelly_error_estimator.h"
#include "libmesh/mesh_refinement.h"
#include "libmesh/enum_solver_package.h"

using namespace libMesh;

bool compare_elements(const UnstructuredMesh & mesh1,
                      const UnstructuredMesh & mesh2);
void assemble_poisson(EquationSystems & es,
                      const std::string & system_name);
void assemble_and_solve(MeshBase &,
                        EquationSystems &);

int main (int argc, char ** argv)
{
  LibMeshInit init (argc, argv);

  // This example requires a linear solver package.
  libmesh_example_requires(libMesh::default_solver_package() != INVALID_SOLVER_PACKAGE,
                           "--enable-petsc, --enable-trilinos, or --enable-eigen");

  // Check for proper calling arguments.
  libmesh_error_msg_if(argc < 3, "Usage:\n" << "\t " << argv[0] << " -n 15");

  // Brief message to the user regarding the program name
  // and command line arguments.
  libMesh::out << "Running " << argv[0];

  for (int i=1; i<argc; i++)
    libMesh::out << " " << argv[i];

  libMesh::out << std::endl << std::endl;

  // This is 3D-only problem
  const unsigned int dim = 3;

  // Skip higher-dimensional examples on a lower-dimensional libMesh build
  libmesh_example_requires(dim <= LIBMESH_DIM, "3D support");

  // We use Dirichlet boundary conditions here
#ifndef LIBMESH_ENABLE_DIRICHLET
  libmesh_example_requires(false, "--enable-dirichlet");
#endif

  // Read number of elements used in each cube from command line
  const int ps = libMesh::command_line_next("-n", 10);

  // Generate eight meshes that will be stitched
  Mesh mesh (init.comm());
  Mesh mesh1(init.comm());
  Mesh mesh2(init.comm());
  Mesh mesh3(init.comm());
  Mesh mesh4(init.comm());
  Mesh mesh5(init.comm());
  Mesh mesh6(init.comm());
  Mesh mesh7(init.comm());
  Mesh nostitch_mesh(init.comm());
  {
    LOG_SCOPE("Initialize and create cubes", "main");
    MeshTools::Generation::build_cube (mesh, ps, ps, ps, -1,    0,    0,  1,  0, 1, HEX8);
    MeshTools::Generation::build_cube (mesh1, ps, ps, ps,    0,  1,    0,  1,  0, 1, HEX8);
    MeshTools::Generation::build_cube (mesh2, ps, ps, ps, -1,    0, -1,    0,  0, 1, HEX8);
    MeshTools::Generation::build_cube (mesh3, ps, ps, ps,    0,  1, -1,    0,  0, 1, HEX8);
    MeshTools::Generation::build_cube (mesh4, ps, ps, ps, -1,    0,    0,  1, -1, 0, HEX8);
    MeshTools::Generation::build_cube (mesh5, ps, ps, ps,    0,  1,    0,  1, -1, 0, HEX8);
    MeshTools::Generation::build_cube (mesh6, ps, ps, ps, -1,    0, -1,    0, -1, 0, HEX8);
    MeshTools::Generation::build_cube (mesh7, ps, ps, ps,    0,  1, -1,    0, -1, 0, HEX8);

    // Generate a single unstitched reference mesh
    MeshTools::Generation::build_cube (nostitch_mesh, ps*2, ps*2, ps*2, -1, 1, -1, 1, -1, 1, HEX8);
  }

  // We stitch the meshes in a hierarchical way.
  {
    LOG_SCOPE("Stitching", "main");
    mesh.stitch_meshes(mesh1, 2, 4, TOLERANCE, true, true, false, false);
    mesh2.stitch_meshes(mesh3, 2, 4, TOLERANCE, true, true, false, false);
    mesh.stitch_meshes(mesh2, 1, 3, TOLERANCE, true, true, false, false);
    mesh4.stitch_meshes(mesh5, 2, 4, TOLERANCE, true, true, false, false);
    mesh6.stitch_meshes(mesh7, 2, 4, TOLERANCE, true, true, false, false);
    mesh4.stitch_meshes(mesh6, 1, 3, TOLERANCE, true, true, false, false);
    mesh.stitch_meshes(mesh4, 0, 5, TOLERANCE, true, true, false, false);
  }

  EquationSystems equation_systems_stitch (mesh);
  EquationSystems equation_systems_nostitch (nostitch_mesh);
  {
    LOG_SCOPE("Initialize and solve systems", "main");
    assemble_and_solve(mesh, equation_systems_stitch);
    assemble_and_solve(nostitch_mesh, equation_systems_nostitch);
  }

  {
    LOG_SCOPE("Result comparison", "main");
    ExactSolution comparison(equation_systems_stitch);
    comparison.attach_reference_solution(&equation_systems_nostitch);
    comparison.compute_error("Poisson", "u");
    Real error = comparison.l2_error("Poisson", "u");
    libmesh_assert_less(error, TOLERANCE*sqrt(TOLERANCE));
    libMesh::out << "L2 error between stitched and non-stitched cases: " << error << std::endl;
  }

#ifdef LIBMESH_HAVE_EXODUS_API
  {
    LOG_SCOPE("Output", "main");
    ExodusII_IO(mesh).write_equation_systems("solution_stitch.exo",
                                             equation_systems_stitch);
    ExodusII_IO(nostitch_mesh).write_equation_systems("solution_nostitch.exo",
                                                      equation_systems_nostitch);
  }
#endif // #ifdef LIBMESH_HAVE_EXODUS_API

  return 0;
}

void assemble_and_solve(MeshBase & mesh,
                        EquationSystems & equation_systems)
{
  mesh.print_info();

  LinearImplicitSystem & system =
    equation_systems.add_system<LinearImplicitSystem> ("Poisson");

#ifdef LIBMESH_ENABLE_DIRICHLET
  unsigned int u_var = system.add_variable("u", FIRST, LAGRANGE);

  system.attach_assemble_function (assemble_poisson);

  ZeroFunction<> zf;

  // the cube has boundaries IDs 0, 1, 2, 3, 4 and 5

  // Most DirichletBoundary users will want to supply a "locally
  // indexed" functor
  DirichletBoundary dirichlet_bc({0,1,2,3,4,5}, {u_var}, zf,
                                 LOCAL_VARIABLE_ORDER);
  system.get_dof_map().add_dirichlet_boundary(dirichlet_bc);
#endif // LIBMESH_ENABLE_DIRICHLET

  equation_systems.init();
  equation_systems.print_info();

#ifdef LIBMESH_ENABLE_AMR
  MeshRefinement mesh_refinement(mesh);

  mesh_refinement.refine_fraction()  = 0.7;
  mesh_refinement.coarsen_fraction() = 0.3;
  mesh_refinement.max_h_level()      = 5;

  const unsigned int max_r_steps = 2;

  for (unsigned int r_step=0; r_step<=max_r_steps; r_step++)
    {
      system.solve();
      if (r_step != max_r_steps)
        {
          ErrorVector error;
          KellyErrorEstimator error_estimator;

          error_estimator.estimate_error(system, error);

          libMesh::out << "Error estimate\nl2 norm = "
                       << error.l2_norm()
                       << "\nmaximum = "
                       << error.maximum()
                       << std::endl;

          mesh_refinement.flag_elements_by_error_fraction (error);

          mesh_refinement.refine_and_coarsen_elements();

          equation_systems.reinit();
        }
    }
#else
  system.solve();
#endif
}

void assemble_poisson(EquationSystems & es,
                      const std::string & libmesh_dbg_var(system_name))
{
  libmesh_assert_equal_to (system_name, "Poisson");

  const MeshBase & mesh = es.get_mesh();
  const unsigned int dim = mesh.mesh_dimension();
  LinearImplicitSystem & system = es.get_system<LinearImplicitSystem>("Poisson");

  const DofMap & dof_map = system.get_dof_map();

  FEType fe_type = dof_map.variable_type(0);
  std::unique_ptr<FEBase> fe (FEBase::build(dim, fe_type));
  QGauss qrule (dim, FIFTH);
  fe->attach_quadrature_rule (&qrule);
  std::unique_ptr<FEBase> fe_face (FEBase::build(dim, fe_type));
  QGauss qface(dim-1, FIFTH);
  fe_face->attach_quadrature_rule (&qface);

  const std::vector<Real> & JxW = fe->get_JxW();
  const std::vector<std::vector<Real>> & phi = fe->get_phi();
  const std::vector<std::vector<RealGradient>> & dphi = fe->get_dphi();

  DenseMatrix<Number> Ke;
  DenseVector<Number> Fe;

  std::vector<dof_id_type> dof_indices;
  SparseMatrix<Number> & matrix = system.get_system_matrix();

  for (const auto & elem : mesh.active_local_element_ptr_range())
    {
      dof_map.dof_indices (elem, dof_indices);

      fe->reinit (elem);

      Ke.resize (dof_indices.size(),
                 dof_indices.size());

      Fe.resize (dof_indices.size());

      for (unsigned int qp=0; qp<qrule.n_points(); qp++)
        {
          for (std::size_t i=0; i<phi.size(); i++)
            {
              Fe(i) += JxW[qp]*phi[i][qp];
              for (std::size_t j=0; j<phi.size(); j++)
                Ke(i,j) += JxW[qp]*(dphi[i][qp]*dphi[j][qp]);
            }
        }

      dof_map.constrain_element_matrix_and_vector (Ke, Fe, dof_indices);

      matrix.add_matrix         (Ke, dof_indices);
      system.rhs->add_vector    (Fe, dof_indices);
    }
}
