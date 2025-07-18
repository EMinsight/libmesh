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



#ifndef LIBMESH_CELL_PRISM_H
#define LIBMESH_CELL_PRISM_H

// Local includes
#include "libmesh/cell.h"

namespace libMesh
{

/**
 * The \p Prism is an element in 3D with 5 sides.
 *
 * \author Benjamin S. Kirk
 * \date 2002
 * \brief The base class for all prismatic element types.
 */
class Prism : public Cell
{
public:

  /**
   * Default prismatic element, takes number of nodes and
   * parent. Derived classes implement 'true' elements.
   */
  Prism(const unsigned int nn, Elem * p, Node ** nodelinkdata) :
    Cell(nn, num_sides, p, _elemlinks_data, nodelinkdata)
  {
    // Make sure the interior parent isn't undefined
    if (LIBMESH_DIM > 3)
      this->set_interior_parent(nullptr);
  }

  Prism (Prism &&) = delete;
  Prism (const Prism &) = delete;
  Prism & operator= (const Prism &) = delete;
  Prism & operator= (Prism &&) = delete;
  virtual ~Prism() = default;

  /**
   * \returns The \p Point associated with local \p Node \p i,
   * in master element rather than physical coordinates.
   */
  virtual Point master_point (const unsigned int i) const override final
  {
    libmesh_assert_less(i, this->n_nodes());
    return Point(_master_points[i][0],
                 _master_points[i][1],
                 _master_points[i][2]);
  }

  /**
   * Geometric constants for all Prisms.
   */
  static const int num_sides = 5;
  static const int num_edges = 9;
  static const int num_children = 8;

  /**
   * \returns 6.  All prism-derivatives are guaranteed to have at
   * least 6 nodes.
   */
  virtual unsigned int n_nodes() const override { return 6; }

  /**
   * \returns 5.
   */
  virtual unsigned int n_sides() const override final { return 5; }

  /**
   * \returns 6.  All prisms have 6 vertices.
   */
  virtual unsigned int n_vertices() const override final { return 6; }

  /**
   * \returns 9.  All prisms have 9 edges.
   */
  virtual unsigned int n_edges() const override final { return 9; }

  /**
   * \returns 5.  All prisms have 5 faces.
   */
  virtual unsigned int n_faces() const override final { return 5; }

  /**
   * \returns 8.
   */
  virtual unsigned int n_children() const override final { return 8; }

  /**
   * \returns \p true if the specified child is on the specified side.
   */
  virtual bool is_child_on_side(const unsigned int c,
                                const unsigned int s) const override final;

  /**
   * \returns \p true if the specified edge is on the specified side.
   */
  virtual bool is_edge_on_side(const unsigned int e,
                               const unsigned int s) const override final;

  /**
   * \returns The side number opposite to \p s (for the triangular
   * sides) or \p invalid_uint (for the quad sides)
   */
  virtual unsigned int opposite_side(const unsigned int s) const override final;

  /**
   * Don't hide Elem::key() defined in the base class.
   */
  using Elem::key;

  /**
   * \returns An id associated with the \p s side of this element.
   * The id is not necessarily unique, but should be close.
   */
  virtual dof_id_type key (const unsigned int s) const override;

  /**
   * \returns An id associated with the \p s side of this element, as
   * defined solely by element vertices.  The id is not necessarily
   * unique, but should be close.  This is particularly useful in the
   * \p MeshBase::find_neighbors() routine.
   */
  virtual dof_id_type low_order_key (const unsigned int s) const override;

  /**
   * \returns \p Prism6::side_nodes_map[side][side_node] after doing some range checking.
   */
  virtual unsigned int local_side_node(unsigned int side,
                                       unsigned int side_node) const override;

  /**
   * \returns \p Prism6::edge_nodes_map[edge][edge_node] after doing some range checking.
   */
  virtual unsigned int local_edge_node(unsigned int edge,
                                       unsigned int edge_node) const override;

  /**
   * \returns A primitive triangle or quad for face i.
   */
  virtual std::unique_ptr<Elem> side_ptr (const unsigned int i) override final;

  /**
   * Rebuilds a primitive triangle or quad for face i.
   */
  virtual void side_ptr (std::unique_ptr<Elem> & side, const unsigned int i) override final;

  virtual std::vector<unsigned int> sides_on_edge(const unsigned int e) const override final;

  /**
   * Two triangular sides, three orientations.
   */
  virtual unsigned int n_permutations() const override final { return 6; }

  virtual bool is_flipped() const override final;

  virtual std::vector<unsigned int> edges_adjacent_to_node(const unsigned int n) const override;

  /**
   * This maps each edge to the sides that contain said edge.
   */
  static const unsigned int edge_sides_map[9][2];

  virtual bool on_reference_element(const Point & p,
                                    const Real eps = TOLERANCE) const override final;

protected:

  /**
   * Data for links to parent/neighbor/interior_parent elements.
   */
  Elem * _elemlinks_data[6+(LIBMESH_DIM>3)];

  /**
   * Matrix that tells which vertices define the location
   * of mid-side (or second-order) nodes.  for the first
   * 9 second-order nodes, this matrix is identical for
   * \p Prism15 and \p Prism18, therefore store it here.
   */
  static const unsigned short int _second_order_adjacent_vertices[9][2];

  /**
   * Vector that names a child sharing each second order node.
   */
  static const unsigned short int _second_order_vertex_child_number[18];

  /**
   * Vector that names the child vertex index for each second order node.
   */
  static const unsigned short int _second_order_vertex_child_index[18];

  /**
   * Master element node locations
   */
  static const Real _master_points[18][3];

  /**
   * This maps the \f$ j^{th} \f$ Prism vertex to the (in this case) 3
   * edge ids adjacent to the node. The edge numbering matches the one
   * used in the derived classes' edge_nodes_map. This data structure
   * is used in the Prism::edges_adjacent_to_node() override and is
   * shared by all the derived Prism types.
   */
  static const unsigned int adjacent_edges_map[/*num_vertices*/6][/*n_adjacent_edges*/3];
};

} // namespace libMesh

#endif // LIBMESH_CELL_PRISM_H
