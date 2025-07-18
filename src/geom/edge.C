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
#include "libmesh/edge.h"
#include "libmesh/node_elem.h"

namespace libMesh
{


// ------------------------------------------------------------
// Edge class static member initializations
const int Edge::num_sides;
const int Edge::num_edges;
const int Edge::num_children;
const int Edge::nodes_per_side;
const int Edge::nodes_per_edge;

// ------------------------------------------------------------
// Edge class member functions
unsigned int Edge::local_side_node(unsigned int side,
                                   unsigned int /*side_node*/) const
{
  libmesh_assert_less (side, this->n_sides());
  return side;
}



unsigned int Edge::local_edge_node(unsigned int /*edge*/,
                                   unsigned int /*edge_node*/) const
{
  libmesh_error_msg("Calling Edge::local_edge_node() does not make sense.");
  return 0;
}



std::unique_ptr<Elem> Edge::side_ptr (const unsigned int i)
{
  libmesh_assert_less (i, 2);
  std::unique_ptr<Elem> nodeelem = std::make_unique<NodeElem>();
  nodeelem->set_node(0, this->node_ptr(i));

  nodeelem->set_interior_parent(this);
  nodeelem->inherit_data_from(*this);

  return nodeelem;
}


void Edge::side_ptr (std::unique_ptr<Elem> & side,
                     const unsigned int i)
{
  libmesh_assert_less (i, this->n_sides());

  if (!side.get() || side->type() != NODEELEM)
    side = this->build_side_ptr(i);
  else
    {
      side->inherit_data_from(*this);
      side->set_node(0, this->node_ptr(i));
    }
}




std::unique_ptr<Elem> Edge::build_side_ptr (const unsigned int i)
{
  libmesh_assert_less (i, 2);
  std::unique_ptr<Elem> nodeelem = std::make_unique<NodeElem>();
  nodeelem->set_node(0, this->node_ptr(i));

  nodeelem->set_interior_parent(this);
  nodeelem->inherit_data_from(*this);

  return nodeelem;
}


void Edge::build_side_ptr (std::unique_ptr<Elem> & side,
                           const unsigned int i)
{
  libmesh_assert_less (i, this->n_sides());

  if (!side.get() || side->type() != NODEELEM)
    side = this->build_side_ptr(i);
  else
    {
      side->set_interior_parent(this);
      side->inherit_data_from(*this);
      side->set_node(0, this->node_ptr(i));
    }
}




bool Edge::is_child_on_side(const unsigned int c,
                            const unsigned int s) const
{
  libmesh_assert_less (c, this->n_children());
  libmesh_assert_less (s, this->n_sides());

  return (c == s);
}



unsigned int Edge::opposite_side(const unsigned int side_in) const
{
  libmesh_assert_less (side_in, 2);
  return 1 - side_in;
}



unsigned int Edge::opposite_node(const unsigned int node_in,
                                 const unsigned int libmesh_dbg_var(side_in)) const
{
  libmesh_assert_less (node_in, 2);
  libmesh_assert_less (side_in, this->n_sides());
  libmesh_assert(this->is_node_on_side(node_in, side_in));

  return 1 - node_in;
}

std::vector<unsigned>
Edge::nodes_on_side(const unsigned int s) const
{
  libmesh_assert_less(s, 2);
  return {s};
}

std::vector<unsigned>
Edge::nodes_on_edge(const unsigned int e) const
{
  return nodes_on_side(e);
}

unsigned int Edge::center_node_on_side(const unsigned short side) const
{
  libmesh_assert_less (side, this->n_sides());
  return side;
}

ElemType Edge::side_type (const unsigned int libmesh_dbg_var(s)) const
{
  libmesh_assert_less (s, 2);
  return NODEELEM;
}


bool
Edge::is_flipped() const
{
  // Don't trigger on 1D elements embedded in 2D/3D
  return (
#if LIBMESH_DIM > 2
          !this->point(0)(2) && !this->point(1)(2) &&
#endif
#if LIBMESH_DIM > 1
          !this->point(0)(1) && !this->point(1)(1) &&
#endif
          this->point(0)(0) > this->point(1)(0));
}



bool Edge::on_reference_element(const Point & p,
                                const Real eps) const
{
  return ((p(0) >= -1-eps) && (p(0) <= 1+eps));
}

} // namespace libMesh
