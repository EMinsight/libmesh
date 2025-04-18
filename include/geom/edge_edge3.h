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



#ifndef LIBMESH_EDGE_EDGE3_H
#define LIBMESH_EDGE_EDGE3_H

// Local includes
#include "libmesh/libmesh_common.h"
#include "libmesh/edge.h"

namespace libMesh
{

/**
 * The \p Edge3 is an element in 1D composed of 3 nodes. It is numbered
 * like this:
 *
 * \verbatim
 *   EDGE3: o----o----o        o---> xi
 *          0    2    1
 * \endverbatim
 *
 * xi in [-1,1] is the reference element coordinate associated with
 * the given numbering.
 *
 * \author Benjamin S. Kirk
 * \date 2002
 * \brief A 1D geometric element with 3 nodes.
 */
class Edge3 : public Edge
{
public:

  /**
   * Constructor.  By default this element has no parent.
   */
  explicit
  Edge3 (Elem * p=nullptr) :
    Edge(num_nodes, p, _nodelinks_data) {}

  Edge3 (Edge3 &&) = delete;
  Edge3 (const Edge3 &) = delete;
  Edge3 & operator= (const Edge3 &) = delete;
  Edge3 & operator= (Edge3 &&) = delete;
  virtual ~Edge3() = default;

  /**
   * \returns The \p Point associated with local \p Node \p i,
   * in master element rather than physical coordinates.
   */
  virtual Point master_point (const unsigned int i) const override
  {
    libmesh_assert_less(i, this->n_nodes());
    if (i < 2)
      return Point(2.0f*Real(i)-1.0f,0,0);
    return Point(0,0,0);
  }

  /**
   * \returns 3.
   */
  virtual unsigned int n_nodes() const override { return num_nodes; }

  /**
   * \returns 2.
   */
  virtual unsigned int n_sub_elem() const override { return 2; }

  /**
   * \returns \p true if the specified (local) node number is a vertex.
   */
  virtual bool is_vertex(const unsigned int i) const override;

  /**
   * \returns \p true if the specified (local) node number is an edge.
   */
  virtual bool is_edge(const unsigned int i) const override;

  /**
   * \returns \p true if the specified (local) node number is a face.
   */
  virtual bool is_face(const unsigned int i) const override;

  /**
   * \returns \p true if the specified (local) node number is on the
   * specified side.
   */
  virtual bool is_node_on_side(const unsigned int n,
                               const unsigned int s) const override;

  /**
   * \returns \p true if the specified (local) node number is on the
   * specified edge (always true in 1D).
   */
  virtual bool is_node_on_edge(const unsigned int n,
                               const unsigned int e) const override;

  /**
   * \returns \p true if the element map is definitely affine within
   * numerical tolerances.
   */
  virtual bool has_affine_map () const override;

  /**
   * \returns \p true if the element map is everywhere invertible,
   * false otherwise. The user can pass a custom tol >= 0 to this
   * function if desired to make the check more stringent, i.e.
   * to also catch elements which are "almost" non-invertible.
   */
  virtual bool has_invertible_map(Real tol) const override;

  /**
   * \returns \p EDGE3.
   */
  virtual ElemType type() const override { return EDGE3; }

  /**
   * \returns SECOND.
   */
  virtual Order default_order() const override;

  virtual void connectivity(const unsigned int sc,
                            const IOPackage iop,
                            std::vector<dof_id_type> & conn) const override;

  /**
   * \returns 2 for all \p n.
   */
  virtual unsigned int n_second_order_adjacent_vertices (const unsigned int) const override
  { return 2; }

  /**
   * \returns The element-local number of the \f$ v^{th} \f$ vertex
   * that defines the \f$ n^{th} \f$ second-order node.
   */
  virtual unsigned short int second_order_adjacent_vertex (const unsigned int,
                                                           const unsigned int v) const override
  { return static_cast<unsigned short int>(v); }

  /**
   * \returns The child number \p c and element-local index \p v of the
   * \f$ n^{th} \f$ second-order node on the parent element.  See
   * elem.h for further details.
   */
  virtual std::pair<unsigned short int, unsigned short int>
  second_order_child_vertex (const unsigned int n) const override;

  /**
   * An optimized method for computing the length of a 3-node edge.
   */
  virtual Real volume () const override;

  /**
   * \returns A bounding box (not necessarily the minimal bounding box)
   * containing the edge.
   */
  virtual BoundingBox loose_bounding_box () const override;

#ifdef LIBMESH_ENABLE_INFINITE_ELEMENTS

  /**
   * \returns \p false.  This is a finite element.
   */
  virtual bool infinite () const override { return false; }

#endif

  /**
   * Don't hide Edge::key(side) defined in the base class.
   */
  using Edge::key;

  /**
   * Compute a unique key for this element which is suitable for
   * hashing (not necessarily unique, but close).  The key is based
   * solely on the mid-edge node's global id, to be consistent with 2D
   * elements that have Edge3 sides (Quad9, Quad8, etc.).
   */
  virtual dof_id_type key () const override;

  /**
   * Geometric constants for Edge3.
   */
  static const int num_nodes = 3;

  virtual void flip(BoundaryInfo *) override final;

protected:

  /**
   * Data for links to nodes.
   */
  Node * _nodelinks_data[num_nodes];



#ifdef LIBMESH_ENABLE_AMR

  /**
   * Matrix used to create the elements children.
   */
  virtual Real embedding_matrix (const unsigned int i,
                                 const unsigned int j,
                                 const unsigned int k) const override
  { return _embedding_matrix[i][j][k]; }

  /**
   * Matrix that computes new nodal locations/solution values
   * from current nodes/solution.
   */
  static const Real _embedding_matrix[num_children][num_nodes][num_nodes];

  LIBMESH_ENABLE_TOPOLOGY_CACHES;

#endif // LIBMESH_ENABLE_AMR

};

} // namespace libMesh


#endif // LIBMESH_EDGE_EDGE3_H
