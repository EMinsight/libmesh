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

#include "libmesh/libmesh_config.h"

// C++ includes
#include <algorithm>
#include <fstream>
#include <iostream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

// Local Includes
#include "libmesh/boundary_info.h"
#include "libmesh/boundary_mesh.h"
#include "libmesh/dof_map.h"
#include "libmesh/elem.h"
#include "libmesh/elem_quality.h"
#include "libmesh/gmv_io.h"
#include "libmesh/inf_elem_builder.h"
#include "libmesh/libmesh.h"
#include "libmesh/mesh.h"
#include "libmesh/mesh_modification.h"
#include "libmesh/mesh_refinement.h"
#include "libmesh/mesh_netgen_interface.h"
#include "libmesh/poly2tri_triangulator.h"
#include "libmesh/simplex_refiner.h"
#include "libmesh/statistics.h"
#include "libmesh/string_to_enum.h"
#include "libmesh/enum_elem_quality.h"
#include "libmesh/getpot.h"
#include <libmesh/mesh_smoother_vsmoother.h>

using namespace libMesh;

// convenient enum for the mode in which the boundary mesh
// should be written
enum BoundaryMeshWriteMode {BM_DISABLED=0, BM_MESH_ONLY};

// Print a usage message before exiting the program.
void usage(const std::string & prog_name);

int main (int argc, char ** argv)
{
  LibMeshInit init(argc, argv);

  unsigned int n_subdomains = 1;
  bool vsmooth = false;
  unsigned int n_rsteps = 0;
  Real simplex_refine = 0.;
  double dist_fact = 0.;
  bool verbose = false;
  BoundaryMeshWriteMode write_bndry = BM_DISABLED;
  bool convert_first_order = false;
  unsigned int convert_second_order = 0;
  bool triangulate = false;
  bool simplex_fill = false;
  Real desired_measure = 1;
  bool do_quality = false;
  ElemQuality quality_type = DIAGONAL;

#ifdef LIBMESH_ENABLE_INFINITE_ELEMENTS
  bool addinfelems = false;
  InfElemBuilder::InfElemOriginValue origin_x(false, 0.);
  InfElemBuilder::InfElemOriginValue origin_y(false, 0.);
  InfElemBuilder::InfElemOriginValue origin_z(false, 0.);
  bool x_sym=false;
  bool y_sym=false;
  bool z_sym=false;
#endif

  std::vector<std::string> names;
  std::vector<std::string> output_names;

  // Check for minimum number of command line args
  if (argc < 3)
    usage(std::string(argv[0]));

  // Create a GetPot object to parse the command line
  GetPot command_line (argc, argv);

  // Print usage and exit immediately if user asked for help.
  if (command_line.search(2, "-h", "-?"))
    usage(argv[0]);

  // Input file name(s)
  command_line.disable_loop();
  while (command_line.search(1, "-i"))
    {
      std::string tmp;
      tmp = command_line.next(tmp);
      names.push_back(tmp);
    }
  command_line.reset_cursor();

  // Output file name
  while (command_line.search(1, "-o"))
    {
      std::string tmp;
      tmp = command_line.next(tmp);
      output_names.push_back(tmp);
    }
  command_line.enable_loop();

  // Get the mesh distortion factor
  if (command_line.search(1, "-D"))
    dist_fact = command_line.next(dist_fact);

  // Number of refinements
  if (command_line.search(1, "-r"))
    {
      int tmp = 0;
      tmp = command_line.next(tmp);
      n_rsteps = cast_int<unsigned int>(tmp);
    }

  // Split edges to reach specified element volume
  if (command_line.search(1, "-s"))
    simplex_refine = command_line.next(simplex_refine);

  // Number of subdomains for partitioning
  if (command_line.search(1, "-p"))
    {
      int tmp = 0;
      tmp = command_line.next(tmp);
      n_subdomains = cast_int<unsigned int>(tmp);
    }

  // Whether to apply variational smoother
  if (command_line.search(1, "-V"))
      vsmooth = true;

  // Should we call all_tri()?
  if (command_line.search(1, "-t"))
    triangulate = true;

  // Should we triangulate/tetrahedralize a boundary interior?
  if (command_line.search(1, "-T"))
    {
      simplex_fill = true;
      desired_measure = command_line.next(desired_measure);
    }

  // Should we calculate element quality?
  if (command_line.search(1, "-q"))
    {
      do_quality = true;
      std::string tmp;
      tmp = command_line.next(tmp);
      if (tmp != "")
        quality_type = Utility::string_to_enum<ElemQuality>(tmp);
    }

  // Should we be verbose?
  if (command_line.search(1, "-v"))
    verbose = true;

  // Should we write the boundary?
  if (command_line.search(1, "-b"))
    write_bndry = BM_MESH_ONLY;

  // Should we convert all elements to 1st order?
  if (command_line.search(1, "-1"))
    convert_first_order = true;

  // Should we convert all elements to 2nd order?
  if (command_line.search(1, "-2"))
    convert_second_order = 2;

  // Should we convert all elements to "full" 2nd order?
  if (command_line.search(1, "-3"))
    convert_second_order = 22;

#ifdef LIBMESH_ENABLE_INFINITE_ELEMENTS

  // Add infinite elements
  if (command_line.search(1, "-a"))
    addinfelems = true;

  // Specify origin x coordinate
  if (command_line.search(1, "-x"))
    {
      origin_x.first  = true;
      origin_x.second = command_line.next(origin_x.second);
    }

  // Specify origin y coordinate
  if (command_line.search(1, "-y"))
    {
      origin_y.first  = true;
      origin_y.second = command_line.next(origin_y.second);
    }

  // Specify origin z coordinate
  if (command_line.search(1, "-z"))
    {
      origin_z.first  = true;
      origin_z.second = command_line.next(origin_z.second);
    }

  // Symmetries
  if (command_line.search(1, "-X"))
    x_sym = true;
  if (command_line.search(1, "-Y"))
    y_sym = true;
  if (command_line.search(1, "-Z"))
    z_sym = true;

#endif // LIBMESH_ENABLE_INFINITE_ELEMENTS

  // Read the input mesh
  Mesh mesh(init.comm());
  if (!names.empty())
    {
      mesh.read(names[0]);

      if (verbose)
        {
          libMesh::out << "Mesh " << names[0] << ":" << std::endl;
          mesh.print_info();
          mesh.get_boundary_info().print_summary();
        }

      for (unsigned int i=1; i < names.size(); ++i)
        {
          Mesh extra_mesh(init.comm());
          extra_mesh.read(names[i]);

          if (verbose)
            {
              libMesh::out << "Mesh " << names[i] << ":" << std::endl;
              extra_mesh.print_info();
              extra_mesh.get_boundary_info().print_summary();
            }

          unique_id_type max_uid = 0;
#ifdef LIBMESH_ENABLE_UNIQUE_ID
          max_uid = mesh.parallel_max_unique_id();
#endif

          mesh.copy_nodes_and_elements(extra_mesh, false,
                                       mesh.max_elem_id(),
                                       mesh.max_node_id(),
                                       max_uid);
          mesh.prepare_for_use();

          if (verbose)
            {
              libMesh::out << "Combined Mesh:" << std::endl;
              mesh.print_info();
              mesh.get_boundary_info().print_summary();
            }
        }
    }

  else
    {
      libMesh::out << "No input specified." << std::endl;
      return 1;
    }



#ifdef LIBMESH_ENABLE_INFINITE_ELEMENTS

  if (addinfelems)
    {
      libmesh_error_msg_if(write_bndry != BM_DISABLED,
                           "ERROR: Invalid combination: Building infinite elements\n"
                           "not compatible with writing boundary conditions.");

      // Sanity checks: -X/Y/Z can only be used, when the
      // corresponding coordinate is also given (using -x/y/z)
      libmesh_error_msg_if((x_sym && !origin_x.first) ||     // claim x-symmetry, but x-coordinate of origin not given!
                           (y_sym && !origin_y.first) ||     // the same for y
                           (z_sym && !origin_z.first),       // the same for z
                           "ERROR: When x-symmetry is requested using -X, then\n"
                           "the option -x <coord> also has to be given.\n"
                           "This holds obviously for y and z, too.");

      // build infinite elements
      InfElemBuilder(mesh).build_inf_elem(origin_x, origin_y, origin_z,
                                          x_sym, y_sym, z_sym,
                                          verbose);

      if (verbose)
        {
          mesh.print_info();
          mesh.get_boundary_info().print_summary();
        }
    }

  // sanity check
  else
    libmesh_error_msg_if((origin_x.first ||  origin_y.first || origin_z.first) ||
                         (x_sym          ||  y_sym          || z_sym),
                         "ERROR:  -x/-y/-z/-X/-Y/-Z is only to be used when\n"
                         "the option -a is also specified!");

#endif


  // Maybe convert non-simplex elements into simplices
  if (triangulate)
    {
      if (verbose)
        libMesh::out << "...Converting to all simplices...\n";

      MeshTools::Modification::all_tri(mesh);
    }

  // Maybe fill an interior mesh specified by an input boundary mesh
  if (simplex_fill)
    {
      if (mesh.mesh_dimension() == 2)
        {
#ifdef LIBMESH_HAVE_NETGEN
          NetGenMeshInterface ngint(mesh);
          ngint.desired_volume() = desired_measure;
          ngint.triangulate();
#else
          libmesh_error_msg("Requested triangulation of a 2D boundary without Poly2Tri enabled");
#endif
        }
      else if (mesh.mesh_dimension() == 1)
        {
#ifdef LIBMESH_HAVE_POLY2TRI
          Poly2TriTriangulator poly2tri(mesh);
          poly2tri.triangulation_type() = TriangulatorInterface::PSLG;
          poly2tri.desired_area() = desired_measure;
          poly2tri.minimum_angle() = 0;
          poly2tri.triangulate();
#else
          libmesh_error_msg("Requested triangulation of a 1D boundary without Poly2Tri enabled");
#endif
        }
    }

  // Compute Shape quality metrics
  if (do_quality)
    {
      StatisticsVector<Real> sv;
      sv.reserve(mesh.n_elem());

      libMesh::out << "Quality type is: " << Quality::name(quality_type) << std::endl;

      // What are the quality bounds for this element?
      std::pair<Real, Real> bounds = mesh.elem_ref(0).qual_bounds(quality_type);
      libMesh::out << "Quality bounds for this element type are: ("
                   << bounds.first
                   << ", "
                   << bounds.second
                   << ") "
                   << std::endl;

      for (const auto & elem : mesh.active_element_ptr_range())
        sv.push_back(elem->quality(quality_type));

      const unsigned int n_bins = 10;
      libMesh::out << "Avg. shape quality: " << sv.mean() << std::endl;

      // Find element indices below the specified cutoff.
      // These might be considered "bad" elements which need refinement.
      std::vector<dof_id_type> bad_elts  = sv.cut_below(0.8);
      libMesh::out << "Found " << bad_elts.size()
                   << " of " << mesh.n_elem()
                   << " elements below the cutoff." << std::endl;

      // Compute the histogram for this distribution
      std::vector<dof_id_type> histogram;
      sv.histogram(histogram, n_bins);

      const bool do_matlab = true;

      if (do_matlab)
        {
          std::ofstream out ("histo.m");

          out << "% This is a sample histogram plot for Matlab." << std::endl;
          out << "bin_members = [" << std::endl;
          for (unsigned int i=0; i<n_bins; i++)
            out << static_cast<Real>(histogram[i]) / static_cast<Real>(mesh.n_elem())
                << std::endl;
          out << "];" << std::endl;

          std::vector<Real> bin_coords(n_bins);
          const Real max   = *(std::max_element(sv.begin(), sv.end()));
          const Real min   = *(std::min_element(sv.begin(), sv.end()));
          const Real delta = (max - min) / static_cast<Real>(n_bins);
          for (unsigned int i=0; i<n_bins; i++)
            bin_coords[i] = min + (i * delta) + delta / 2.0 ;

          out << "bin_coords = [" << std::endl;
          for (unsigned int i=0; i<n_bins; i++)
            out << bin_coords[i] << std::endl;
          out << "];" << std::endl;

          out << "bar(bin_coords, bin_members, 1);" << std::endl;
          out << "hold on" << std::endl;
          out << "plot (bin_coords, 0, 'kx');" << std::endl;
          out << "xlabel('Quality (0=Worst, 1=Best)');" << std::endl;
          out << "ylabel('Percentage of elements in each bin');" << std::endl;
          out << "axis([" << min << "," << max << ",0, max(bin_members)]);" << std::endl;

          out << "title('" << Quality::name(quality_type) << "');" << std::endl;

        }
    }

   // Possibly convert higher-order elements into first-order
   // counterparts
  if (convert_first_order)
    {
      if (verbose)
        libMesh::out << "Converting elements to first order counterparts\n";

      mesh.all_first_order();

      if (verbose)
        {
          mesh.print_info();
          mesh.get_boundary_info().print_summary();
        }
    }

  // Possibly convert all linear elements into second-order counterparts
  if (convert_second_order > 0)
    {
      bool second_order_mode = true;
      std:: string message = "Converting elements to second order counterparts";
      if (convert_second_order == 2)
        {
          second_order_mode = false;
          message += ", lower version: Quad4 -> Quad8, not Quad9";
        }

      else if (convert_second_order == 22)
        {
          second_order_mode = true;
          message += ", highest version: Quad4 -> Quad9";
        }

      else
        libmesh_error_msg("Invalid value, convert_second_order = " << convert_second_order);

      if (verbose)
        libMesh::out << message << std::endl;

      mesh.all_second_order(second_order_mode);

      if (verbose)
        {
          mesh.print_info();
          mesh.get_boundary_info().print_summary();
        }
    }

  if (simplex_refine)
    {
      SimplexRefiner simplex_refiner (mesh);
      simplex_refiner.desired_volume() = simplex_refine;
      simplex_refiner.refine_elements();
    }

#ifdef LIBMESH_ENABLE_AMR

  // Possibly refine the mesh
  if (n_rsteps > 0)
    {
      if (verbose)
        libMesh::out << "Refining the mesh "
                     << n_rsteps << " times"
                     << std::endl;

      MeshRefinement mesh_refinement (mesh);
      mesh_refinement.uniformly_refine(n_rsteps);

      if (verbose)
        {
          mesh.print_info();
          mesh.get_boundary_info().print_summary();
        }
    }


  // Possibly distort the mesh
  if (dist_fact > 0.)
    {
      libMesh::out << "Distorting the mesh by a factor of "
                   << dist_fact
                   << std::endl;

      MeshTools::Modification::distort(mesh,dist_fact);
    }

#endif



  // Possibly partition the mesh
  if (n_subdomains > 1)
    mesh.partition(n_subdomains);

  // Possibly smooth the mesh
  if (vsmooth)
  {
    VariationalMeshSmoother vsmoother(mesh);
    vsmoother.smooth();
  }

  // Possibly write the mesh
  if (output_names.size())
    {
      // When the mesh got refined, it is likely that
      // the user does _not_ want to write also
      // the coarse elements, but only the active ones.
      // Use Mesh::create_submesh() to create a mesh
      // of only active elements, and then write _this_
      // new mesh.
      if (n_rsteps > 0)
        {
          if (verbose)
            libMesh::out << " Mesh got refined, will write only _active_ elements." << std::endl;

          Mesh new_mesh (init.comm(), cast_int<unsigned char>(mesh.mesh_dimension()));

          mesh.create_submesh
            (new_mesh,
             mesh.active_elements_begin(),
             mesh.active_elements_end());

          // now write the new_mesh in as many formats as requested
          for (auto & output_name : output_names)
            new_mesh.write(output_name);
        }
      else
        {
          // Write the new_mesh in as many formats as requested
          for (auto & output_name : output_names)
            mesh.write(output_name);
        }



      // Possibly write the BCs
      if (write_bndry != BM_DISABLED)
        {
          BoundaryMesh boundary_mesh
            (mesh.comm(), cast_int<unsigned char>(mesh.mesh_dimension()-1));

          if (write_bndry == BM_MESH_ONLY)
            mesh.get_boundary_info().sync(boundary_mesh);

          else
            libmesh_error_msg("Invalid value write_bndry = " << write_bndry);

          // Write the mesh in as many formats as requested
          for (auto boundary_name : output_names)
            {
              boundary_name = "bndry_" + boundary_name;
              boundary_mesh.write(boundary_name);
            }
        }
    }

  return 0;
}


void usage(const std::string & prog_name)
{
  std::ostringstream helpList;
  helpList << "usage:\n"
           << "        "
           << prog_name
           << " [options] ...\n"
           << "\n"
           << "options:\n"
           << "    -i <string>                   Input file name, one option per file\n"
           << "                                    Multiple files get concatenated"
           << "    -o <string>                   Output file name, one option per file\n"
           << "\n    -b                            Write the boundary conditions\n"
           << "    -D <factor>                   Randomly move interior nodes by D*hmin\n"
           << "    -h                            Print help menu\n"
           << "    -p <count>                    Partition into <count> subdomains\n"
           << "    -V                            Apply the variational mesh smoother\n"
#ifdef LIBMESH_ENABLE_AMR
           << "    -r <count>                    Uniformly refine <count> times\n"
#endif
           << "    -s <volume>                   Split-edge refine elements exceeding <volume>\n"
           << "    -t                            Convert all elements to triangles/tets\n"
           << "    -T <desired elem area/vol>    Triangulate/Tetrahedralize the interior\n"
           << "    -v                            Verbose\n"
           << "    -q <metric>                   Evaluates the named element quality metric\n"
           << "    -1                            Converts a mesh of higher order elements\n"
           << "                                  to their first-order counterparts:\n"
           << "                                  Quad8 -> Quad4, Tet10 -> Tet4 etc\n"
           << "    -2                            Converts a mesh of linear elements\n"
           << "                                  to their second-order counterparts:\n"
           << "                                  Quad4 -> Quad8, Tet4 -> Tet10 etc\n"
           << "    -3                            Same, but to the highest possible:\n"
           << "                                  Quad4 -> Quad9, Hex8 -> Hex27 etc\n"
#ifdef LIBMESH_ENABLE_INFINITE_ELEMENTS
           << "\n    -a                            Add infinite elements\n"
           << "    -x <coord>                    Specify infinite element origin\n"
           << "    -y <coord>                    coordinates. If none given, origin\n"
           << "    -z <coord>                    is determined automatically.\n"
           << "    -X                            When building infinite elements \n"
           << "    -Y                            treat mesh as x/y/z-symmetric.\n"
           << "    -Z                            When -X is given, -x <coord> also\n"
           << "                                  has to be given.  Similar for y,z.\n"
#endif
           << "\n"
           << "\n"
           << " This program is used to convert and partition from/to a variety of\n"
           << " formats.  File types are inferred from file extensions.  For example,\n"
           << " the command:\n"
           << "\n"
           << "  ./meshtool -i in.e -o out.plt\n"
           << "\n"
           << " will read a mesh in the ExodusII format (from Cubit, for example)\n"
           << " from the file in.e.  It will then write the mesh in the Tecplot\n"
           << " binary format to out.plt.\n"
           << "\n"
           << " and\n"
           << "\n"
           << "  ./meshtool -i cylinder.msh -i square.xda -o out.e -2\n"
           << "\n"
           << " will read the Gmsh formatted mesh file cylinder.msh, concatenate\n"
           << " the elements and nodes (with shifted id numbers) from the libMesh\n"
           << " mesh file square.xda, convert all first-order elements from both\n"
           << " to second-order, and write the combined mesh in Exodus format to\n"
           << " out.e.\n"
#ifdef LIBMESH_ENABLE_INFINITE_ELEMENTS
           << "\n"
           << " and\n"
           << "\n"
           << "  ./meshtool -i dry.unv -o packed.gmv -a -x 30.5 -y -10.5 -X\n"
           << "\n"
           << " will read a Universal file, determine the z-coordinate of the origin\n"
           << " automatically, e.g. z_origin = 3., build infinite elements with the\n"
           << " origin (30.5, -10.5, 3.) on top of volume elements, while preserving\n"
           << " a symmetry plane through (30.5, -10.5, 3.) perpendicular to x.\n"
           << " It is imperative that the origin lies _inside_ the given volume mesh.\n"
           << " If not, infinite elements are not correctly built!\n"
#endif
           << "\n"
           << " This program supports the I/O formats:\n"
           << "\n"
           << "     *.cpa   -- libMesh ASCII checkpoint format\n"
           << "     *.cpr   -- libMesh binary checkpoint format,\n"
           << "     *.e     -- Sandia's ExodusII format\n"
           << "     *.exd   -- Sandia's ExodusII format\n"
           << "     *.gmv   -- LANL's GMV (General Mesh Viewer) format\n"
           << "     *.mesh  -- MEdit mesh format\n"
           << "     *.msh   -- GMSH ASCII file\n"
           << "     *.n     -- Sandia's Nemesis format\n"
           << "     *.nem   -- Sandia's Nemesis format\n"
           << "     *.plt   -- Tecplot binary file\n"
           << "     *.poly  -- TetGen ASCII file\n"
           << "     *.ucd   -- AVS's ASCII UCD format\n"
           << "     *.ugrid -- Kelly's DIVA ASCII format\n"
           << "     *.unv   -- I-deas Universal format\n"
           << "     *.vtu   -- VTK (paraview-readable) format\n"
           << "     *.xda   -- libMesh ASCII format\n"
           << "     *.xdr   -- libMesh binary format,\n"
           << "\n   As well as for input only:\n\n" \
           << "     *.bext  -- Bezier files in DYNA format\n" \
           << "     *.bxt   -- Bezier files in DYNA format\n" \
           << "     *.inp   -- Abaqus .inp format\n" \
           << "     *.mat   -- Matlab triangular ASCII file\n" \
           << "     *.off   -- OOGL OFF surface format\n" \
           << "     *.ogl   -- OOGL OFF surface format\n" \
           << "     *.oogl  -- OOGL OFF surface format\n" \
           << "\n   As well as for output only:\n\n" \
           << "     *.dat   -- Tecplot ASCII file\n"
           << "     *.fro   -- ACDL's surface triangulation file\n"
           << "     *.poly  -- TetGen ASCII file\n"
           << "\n";

  libMesh::out << helpList.str();
  exit(0);
}
