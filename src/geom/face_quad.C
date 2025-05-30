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

// Local includes
#include "libmesh/face_quad.h"
#include "libmesh/edge_edge2.h"
#include "libmesh/face_quad4.h"
#include "libmesh/enum_elem_quality.h"

// C++ includes
#include <array>


namespace libMesh
{

// ------------------------------------------------------------
// Quad class static member initializations
const int Quad::num_sides;
const int Quad::num_children;

// Note: we can omit initialization of the third entry of each row because
// static variables are automatically zero-initialized.
const Real Quad::_master_points[9][3] =
  {
    {-1, -1},
    {1, -1},
    {1, 1},
    {-1, 1},
    {0, -1},
    {1, 0},
    {0, 1},
    {-1, 0},
    {0, 0}
  };

const unsigned int Quad::adjacent_sides_map[/*num_vertices*/4][/*n_adjacent_sides*/2] =
  {
    {0, 3},  // Sides adjacent to node 0
    {0, 1},  // Sides adjacent to node 1
    {1, 2},  // Sides adjacent to node 2
    {2, 3}   // Sides adjacent to node 3
  };



// ------------------------------------------------------------
// Quad class member functions
dof_id_type Quad::key (const unsigned int s) const
{
  libmesh_assert_less (s, this->n_sides());

  return this->compute_key(this->node_id(Quad4::side_nodes_map[s][0]),
                           this->node_id(Quad4::side_nodes_map[s][1]));
}



dof_id_type Quad::low_order_key (const unsigned int s) const
{
  libmesh_assert_less (s, this->n_sides());

  return this->compute_key(this->node_id(Quad4::side_nodes_map[s][0]),
                           this->node_id(Quad4::side_nodes_map[s][1]));
}



unsigned int Quad::local_side_node(unsigned int side,
                                   unsigned int side_node) const
{
  libmesh_assert_less (side, this->n_sides());
  libmesh_assert_less (side_node, Quad4::nodes_per_side);

  return Quad4::side_nodes_map[side][side_node];
}



unsigned int Quad::local_edge_node(unsigned int edge,
                                   unsigned int edge_node) const
{
  return local_side_node(edge, edge_node);
}



dof_id_type Quad::key () const
{
  return this->compute_key(this->node_id(0),
                           this->node_id(1),
                           this->node_id(2),
                           this->node_id(3));
}



std::unique_ptr<Elem> Quad::side_ptr (const unsigned int i)
{
  libmesh_assert_less (i, this->n_sides());

  std::unique_ptr<Elem> edge = std::make_unique<Edge2>();

  for (auto n : edge->node_index_range())
    edge->set_node(n, this->node_ptr(Quad4::side_nodes_map[i][n]));

  return edge;
}



void Quad::side_ptr (std::unique_ptr<Elem> & side,
                     const unsigned int i)
{
  this->simple_side_ptr<Quad,Quad4>(side, i, EDGE2);
}



bool Quad::is_child_on_side(const unsigned int c,
                            const unsigned int s) const
{
  libmesh_assert_less (c, this->n_children());
  libmesh_assert_less (s, this->n_sides());

  // A quad's children and nodes don't share the same ordering:
  // child 2 and 3 are swapped;
  unsigned int n = (c < 2) ? c : 5-c;
  return (n == s || n == (s+1)%4);
}



unsigned int Quad::opposite_side(const unsigned int side_in) const
{
  libmesh_assert_less (side_in, 4);

  return (side_in + 2) % 4;
}



unsigned int Quad::opposite_node(const unsigned int node_in,
                                 const unsigned int side_in) const
{
  libmesh_assert_less (node_in, 8);
  libmesh_assert_less (node_in, this->n_nodes());
  libmesh_assert_less (side_in, this->n_sides());
  libmesh_assert(this->is_node_on_side(node_in, side_in));

  static const unsigned char side02_nodes_map[] =
    {3, 2, 1, 0, 6, 255, 4, 255};
  static const unsigned char side13_nodes_map[] =
    {1, 0, 3, 2, 255, 7, 255, 5};

  switch (side_in)
    {
    case 0:
    case 2:
      return side02_nodes_map[node_in];
    case 1:
    case 3:
      return side13_nodes_map[node_in];
    default:
      libmesh_error_msg("Unsupported side_in = " << side_in);
    }
}


bool Quad::is_flipped() const
{
  return (
#if LIBMESH_DIM > 2
          // Don't bother outside the XY plane
          !this->point(0)(2) && !this->point(1)(2) &&
          !this->point(2)(2) && !this->point(3)(2) &&
#endif
          ((this->point(1)(0)-this->point(0)(0))*
           (this->point(3)(1)-this->point(0)(1)) <
           (this->point(3)(0)-this->point(0)(0))*
           (this->point(1)(1)-this->point(0)(1))));
}


std::vector<unsigned int>
Quad::edges_adjacent_to_node(const unsigned int n) const
{
  libmesh_assert_less(n, this->n_nodes());

  // For vertices, we use the Quad::adjacent_sides_map, otherwise each
  // of the mid-edge nodes is adjacent only to the edge it is on, and the
  // center node is not adjacent to any edge.
  if (this->is_vertex(n))
    return {std::begin(adjacent_sides_map[n]), std::end(adjacent_sides_map[n])};
  else if (this->is_edge(n))
    return {n - this->n_vertices()};

  libmesh_assert(this->is_face(n));
  return {};
}

Real Quad::quality (const ElemQuality q) const
{
  switch (q)
    {
      // The EDGE_LENGTH_RATIO metric is handled by the base class.
      //
      // The CUBIT 15.1 User Documentation refers to the
      // EDGE_LENGTH_RATIO as the "Aspect Ratio" metric for Quads,
      // however, we prefer the slightly more robust aspect ratio
      // formula employed by Ansys, and have therefore designated that
      // as our ASPECT_RATIO metric here.
      //
      // As a counter-example for employing the EDGE_LENGTH_RATIO in
      // Quads, consider a "rhombus". All four side lengths are equal,
      // so the EDGE_LENGTH_RATIO of this element is always (the
      // optimal value) 1.0, regardless of the internal angle "theta"
      // of the rhombus. A more sensitive shape quality metric should
      // take this internal angle into account, so that very thin
      // rhombus-shaped Quads are not considered optimal.

    case ASPECT_RATIO:
      {
        // Aspect Ratio definition from Ansys Theory Manual.
        // Reference: Ansys, Inc. Theory Reference, Ansys Release 9.0, 2004 (Chapter: 13.7.3)
        //
        // For the rhombus case described above, the aspect ratio of
        // the element using the Ansys Aspect Ratio metric is
        // 1/sin(theta), where theta is the acute interior angle of
        // the rhombus. The aspect ratio therefore has a minimum value
        // of 1.0 (when theta = pi/2), and blows up to infinity as
        // sin(theta) -> 0, rather than remaining equal to 1.0 as in
        // the CUBIT formula.

        // Compute midpoint positions along each edge
        Point m0 = Real(0.5) * (this->point(0) + this->point(1));
        Point m1 = Real(0.5) * (this->point(1) + this->point(2));
        Point m2 = Real(0.5) * (this->point(2) + this->point(3));
        Point m3 = Real(0.5) * (this->point(3) + this->point(0));

        // Compute vectors adjoining opposite side midpoints
        Point v0 = m2 - m0;
        Point v1 = m1 - m3;

        // Compute the length of the midlines
        Real v0_norm = v0.norm();
        Real v1_norm = v1.norm();

        // Instead of dividing by zero in the next step, just return
        // 0.  The optimal aspect ratio is 1.0, and "high" aspect
        // ratios are bad, but an aspect ratio of 0 should also be
        // considered bad.
        if (v0_norm == 0. || v1_norm == 0.)
          return 0.;

        // Compute sine of the angle between v0, v1. For rectangular
        // elements, sin_theta == 1.
        Real sin_theta = cross_norm(v0, v1) / v0_norm / v1_norm;
        Real v0s = v0_norm*sin_theta;
        Real v1s = v1_norm*sin_theta;

        // Determine the min, max of each midline length and its
        // projection.
        //
        // Note: The return values min{0,1} and max{0,1} here are
        // *references* and we cannot pass a temporary to
        // std::minmax() since the reference returned becomes a
        // dangling reference at "the end of the full expression that
        // contains the call to std::minmax()"
        // https://en.cppreference.com/w/cpp/algorithm/minmax
        auto [min0, max0] = std::minmax(v0_norm, v1s);
        auto [min1, max1] = std::minmax(v0s, v1_norm);

        // Return the max of the two quotients
        return std::max(max0/min0, max1/min1);
      }

      // Compute the min/max diagonal ratio.
      // This is modeled after the Hex element
    case DISTORTION:
    case DIAGONAL:
      {
        // Diagonal between node 0 and node 2
        const Real d02 = this->length(0,2);

        // Diagonal between node 1 and node 3
        const Real d13 = this->length(1,3);

        // Find the biggest and smallest diagonals
        if ((d02 > 0.) && (d13 >0.))
          if (d02 < d13) return d02 / d13;
          else return d13 / d02;
        else
          return 0.;
        break;
      }

      // CUBIT 15.1 User Documentation:
      // Stretch: Sqrt(2) * minimum edge length / maximum diagonal length
    case STRETCH:
      {
        Real lengths[4] = {this->length(0,1), this->length(1,2), this->length(2,3), this->length(3,0)};
        Real min_edge = *std::min_element(lengths, lengths+4);
        Real d_max = std::max(this->length(0,2), this->length(1,3));

        // Return 0. instead of dividing by zero.
        if (d_max == 0.)
          return 0.;
        else
          return std::sqrt(2) * min_edge / d_max;
      }

    case SHAPE:
    case SKEW:
      {
        // From: P. Knupp, "Algebraic mesh quality metrics for
        // unstructured initial meshes," Finite Elements in Analysis
        // and Design 39, 2003, p. 217-241, Sections 5.2 and 5.3.
        typedef std::array<Real, 4> Array4;
        typedef std::array<Real, 6> Array6;

        // x, y, z node coordinates.
        std::vector<Real>
          x = {this->point(0)(0), this->point(1)(0), this->point(2)(0), this->point(3)(0)},
          y = {this->point(0)(1), this->point(1)(1), this->point(2)(1), this->point(3)(1)},
          z = {this->point(0)(2), this->point(1)(2), this->point(2)(2), this->point(3)(2)};

        // Nodal Jacobians. These are 3x2 matrices, hence we represent
        // them by Array6.
        std::array<Array6, 4> A;
        for (unsigned int k=0; k<4; ++k)
          {
            unsigned int
              kp1 = k+1 > 3 ? k+1-4 : k+1,
              kp3 = k+3 > 3 ? k+3-4 : k+3;

            // To initialize std::array we need double curly braces in
            // C++11 but not C++14 apparently.
            A[k] = {{x[kp1] - x[k], x[kp3] - x[k],
                     y[kp1] - y[k], y[kp3] - y[k],
                     z[kp1] - z[k], z[kp3] - z[k]}};
          }

        // Compute metric tensors, T_k = A_k^T * A_k. These are 2x2
        // square matrices, hence we represent them by Array4.
        std::array<Array4, 4> T;
        for (unsigned int k=0; k<4; ++k)
          {
            Real
              top_left = A[k][0]*A[k][0] + A[k][2]*A[k][2] + A[k][4]*A[k][4],
              off_diag = A[k][0]*A[k][1] + A[k][2]*A[k][3] + A[k][4]*A[k][5],
              bot_rigt = A[k][1]*A[k][1] + A[k][3]*A[k][3] + A[k][5]*A[k][5];

            T[k] = {{top_left, off_diag,
                     off_diag, bot_rigt}};
          }


        // Nodal areas. These are approximated as sqrt(det(A^T * A))
        // to handle the general case of a 2D element living in 3D.
        Array4 alpha;
        for (unsigned int k=0; k<4; ++k)
          alpha[k] = std::sqrt(T[k][0]*T[k][3] - T[k][1]*T[k][2]);

        // All nodal areas must be strictly positive. Return 0 (the
        // lowest quality) otherwise. We know they can't be negative
        // because they are the result of a sqrt, but they might be
        // identically 0.
        if (*std::min_element(alpha.begin(), alpha.end()) == 0.)
          return 0.;

        // Compute and return the shape metric. These only use the
        // diagonal entries of the T_k.
        Real den = 0.;
        if (q == SHAPE)
          {
            for (unsigned int k=0; k<4; ++k)
              den += (T[k][0] + T[k][3]) / alpha[k];
            return (den == 0.) ? 0 : (8. / den);
          }
        else
          {
            for (unsigned int k=0; k<4; ++k)
              den += std::sqrt(T[k][0] * T[k][3]) / alpha[k];
            return (den == 0.) ? 0 : (4. / den);
          }
      }

      // This test returns 0 if a Quad:
      // 1) Is "twisted" (i.e. has an invalid numbering)
      // 2) Has nearly parallel adjacent edges
      // 3) Has a nearly zero length edge
      // Otherwise, it returns 1.
    case TWIST:
      {
        // Compute the cross product induced by each "corner" of the QUAD
        // and check that:
        // 1) None of the cross products are zero (within a tolernace), and
        // 2) all of the cross products point in the same direction.

        // Corner 0
        Point vec_01 = point(1) - point(0);
        Point vec_03 = point(3) - point(0);
        Point corner_0_vec = vec_01.cross(vec_03);

        // Corner 1
        Point vec_12 = point(2) - point(1);
        Point vec_10 = point(0) - point(1);
        Point corner_1_vec = vec_12.cross(vec_10);

        // Corner 2
        Point vec_23 = point(3) - point(2);
        Point vec_21 = point(1) - point(2);
        Point corner_2_vec = vec_23.cross(vec_21);

        // Corner 3
        Point vec_30 = point(0) - point(3);
        Point vec_32 = point(2) - point(3);
        Point corner_3_vec = vec_30.cross(vec_32);

        // If any of these cross products is nearly zero, then either
        // we have nearly parallel adjacent edges or a nearly zero
        // length edge. We return 0 in this case.
        if ((corner_0_vec.norm() < TOLERANCE*TOLERANCE) ||
            (corner_1_vec.norm() < TOLERANCE*TOLERANCE) ||
            (corner_2_vec.norm() < TOLERANCE*TOLERANCE) ||
            (corner_3_vec.norm() < TOLERANCE*TOLERANCE))
          {
            return 0.;
          }

        // Now check whether the element is twisted.
        Real dot_01 = corner_0_vec * corner_1_vec;
        Real dot_02 = corner_0_vec * corner_2_vec;
        Real dot_03 = corner_0_vec * corner_3_vec;

        if ((dot_01 <= 0.) || (dot_02 <= 0.) || (dot_03 <= 0.))
          return 0.;

        // If we made it here, then Elem is not twisted.
        return 1.;
      }

    case WARP:
      {
        // We compute the angle between the two planes formed by
        // drawing an imaginary diagonal separating opposite vertices
        // A and B, where (A,B) = (0,2) and (1,3). The element warpage
        // is then taken to be the smaller of these two angles. For a
        // flat quadrilateral, the planes are parallel, thus the angle
        // between them is zero, and the element warpage is therefore
        // 1.
        //
        // Reference: https://cubit.sandia.gov/files/cubit/16.08/help_manual/WebHelp/mesh_generation/mesh_quality_assessment/quadrilateral_metrics.htm
        Point
          n0 = (this->point(1) - this->point(0)).cross(this->point(3) - this->point(0)).unit(),
          n1 = (this->point(2) - this->point(1)).cross(this->point(0) - this->point(1)).unit(),
          n2 = (this->point(3) - this->point(2)).cross(this->point(1) - this->point(2)).unit(),
          n3 = (this->point(0) - this->point(3)).cross(this->point(2) - this->point(3)).unit();

        return std::min(n0*n2, n1*n3);
      }

    default:
      return Elem::quality(q);
    }
}






std::pair<Real, Real> Quad::qual_bounds (const ElemQuality q) const
{
  std::pair<Real, Real> bounds;

  switch (q)
    {
    case EDGE_LENGTH_RATIO:
    case ASPECT_RATIO:
      bounds.first  = 1.;
      bounds.second = 4.;
      break;

    case TAPER:
      bounds.first  = 0.;
      bounds.second = 0.7;
      break;

    case WARP:
      bounds.first  = 0.9;
      bounds.second = 1.;
      break;

    case STRETCH:
      bounds.first  = 0.25;
      bounds.second = 1.;
      break;

    case MIN_ANGLE:
      bounds.first  = 45.;
      bounds.second = 90.;
      break;

    case MAX_ANGLE:
      bounds.first  = 90.;
      bounds.second = 135.;
      break;

    case CONDITION:
      bounds.first  = 1.;
      bounds.second = 4.;
      break;

    case JACOBIAN:
    case SCALED_JACOBIAN:
      bounds.first  = 0.5;
      bounds.second = 1.;
      break;

    case SHEAR:
    case SHAPE:
    case SKEW:
    case SIZE:
      bounds.first  = 0.3;
      bounds.second = 1.;
      break;

    case DISTORTION:
      bounds.first  = 0.6;
      bounds.second = 1.;
      break;

    case TWIST:
      bounds.first  = 0.;
      bounds.second = 1.;
      break;

    default:
      libMesh::out << "Warning: Invalid quality measure chosen." << std::endl;
      bounds.first  = -1;
      bounds.second = -1;
    }

  return bounds;
}



bool Quad::on_reference_element(const Point & p,
                                const Real eps) const
{
  const Real & xi = p(0);
  const Real & eta = p(1);

  // The reference quadrilateral element is [-1,1]^2.
  return ((xi  >= -1.-eps) &&
          (xi  <=  1.+eps) &&
          (eta >= -1.-eps) &&
          (eta <=  1.+eps));
}



const unsigned short int Quad::_second_order_adjacent_vertices[4][2] =
  {
    {0, 1}, // vertices adjacent to node 4
    {1, 2}, // vertices adjacent to node 5
    {2, 3}, // vertices adjacent to node 6
    {0, 3}  // vertices adjacent to node 7
  };



const unsigned short int Quad::_second_order_vertex_child_number[9] =
  {
    99,99,99,99, // Vertices
    0,1,2,0,     // Edges
    0            // Interior
  };



const unsigned short int Quad::_second_order_vertex_child_index[9] =
  {
    99,99,99,99, // Vertices
    1,2,3,3,     // Edges
    2            // Interior
  };



#ifdef LIBMESH_ENABLE_AMR

// We number 25 "possible node locations" for a 2x2 refinement of
// quads with up to 3x3 nodes each
const int Quad::_child_node_lookup[4][9] =
  {
    // node lookup for child 0 (near node 0)
    { 0, 2, 12, 10,  1, 7, 11, 5,  6},

    // node lookup for child 1 (near node 1)
    { 2, 4, 14, 12,  3, 9, 13, 7,  8},

    // node lookup for child 2 (near node 3)
    { 10, 12, 22, 20,  11, 17, 21, 15,  16},

    // node lookup for child 3 (near node 2)
    { 12, 14, 24, 22,  13, 19, 23, 17,  18}
  };


#endif

} // namespace libMesh
