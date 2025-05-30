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
#include "libmesh/libmesh_config.h"

#ifdef LIBMESH_ENABLE_HIGHER_ORDER_SHAPES

#include "libmesh/elem.h"
#include "libmesh/enum_to_string.h"
#include "libmesh/fe.h"
#include "libmesh/fe_interface.h"
#include "libmesh/fe_macro.h"

namespace libMesh
{

// ------------------------------------------------------------
// Bernstein-specific implementations, Steffen Petersen 2005

// Anonymous namespace for local helper functions
namespace {

void bernstein_nodal_soln(const Elem * elem,
                          const Order order,
                          const std::vector<Number> & elem_soln,
                          std::vector<Number> & nodal_soln,
                          const bool add_p_level)
{
  const unsigned int n_nodes = elem->n_nodes();

  const ElemType elem_type = elem->type();

  nodal_soln.resize(n_nodes);

  const Order totalorder = order + add_p_level*elem->p_level();

  // FEType object to be passed to various FEInterface functions below.
  FEType fe_type(order, BERNSTEIN);

  switch (totalorder)
    {
      // Constant shape functions
    case CONSTANT:
      {
        libmesh_assert_equal_to (elem_soln.size(), 1);

        std::fill(nodal_soln.begin(), nodal_soln.end(), elem_soln[0]);

        return;
      }


      // For other bases do interpolation at the nodes
      // explicitly.
    case FIRST:
    case SECOND:
    case THIRD:
    case FOURTH:
    case FIFTH:
    case SIXTH:
      {
        const unsigned int n_sf =
          FEInterface::n_shape_functions(fe_type, elem);

        std::vector<Point> refspace_nodes;
        FEBase::get_refspace_nodes(elem_type,refspace_nodes);
        libmesh_assert_equal_to (refspace_nodes.size(), n_nodes);
        libmesh_assert_equal_to (elem_soln.size(), n_sf);

        // Zero before summation
        std::fill(nodal_soln.begin(), nodal_soln.end(), 0);

        for (unsigned int n=0; n<n_nodes; n++)
          // u_i = Sum (alpha_i phi_i)
          for (unsigned int i=0; i<n_sf; i++)
            nodal_soln[n] += elem_soln[i] *
              FEInterface::shape(fe_type, elem, i, refspace_nodes[n]);

        return;
      }

    default:
      libmesh_error_msg("ERROR: Invalid total order " << totalorder);
    }
} //  bernstein_nodal_soln()



unsigned int BERNSTEIN_n_dofs(const ElemType t, const Order o)
{
  switch (t)
    {
    case NODEELEM:
      return 1;
    case EDGE2:
    case EDGE3:
      return (o+1);
    case QUAD4:
    case QUADSHELL4:
      libmesh_assert_less (o, 2);
      libmesh_fallthrough();
    case QUAD8:
    case QUADSHELL8:
      {
        if (o == 1)
          return 4;
        else if (o == 2)
          return 8;
        else
          libmesh_error_msg("ERROR: Invalid Order " << Utility::enum_to_string(o) << " selected for BERNSTEIN FE family!");
      }
    case QUAD9:
    case QUADSHELL9:
      return ((o+1)*(o+1));
    case HEX8:
      libmesh_assert_less (o, 2);
      libmesh_fallthrough();
    case HEX20:
      {
        if (o == 1)
          return 8;
        else if (o == 2)
          return 20;
        else
          libmesh_error_msg("ERROR: Invalid Order " << Utility::enum_to_string(o) << " selected for BERNSTEIN FE family!");
      }
    case HEX27:
      return ((o+1)*(o+1)*(o+1));
    case TRI3:
    case TRISHELL3:
      libmesh_assert_less (o, 2);
      libmesh_fallthrough();
    case TRI6:
    case TRI7:
      return ((o+1)*(o+2)/2);
    case TET4:
      libmesh_assert_less (o, 2);
      libmesh_fallthrough();
    case TET10:
    case TET14:
      libmesh_assert_less (o, 3);
      return ((o+1)*(o+2)*(o+3)/6);
    case INVALID_ELEM:
      return 0;
    default:
      libmesh_error_msg("ERROR: Invalid ElemType " << Utility::enum_to_string(t) << " selected for BERNSTEIN FE family!");
    }
} // BERNSTEIN_n_dofs()



unsigned int BERNSTEIN_n_dofs(const Elem * e, const Order o)
{
  libmesh_assert(e);
  return BERNSTEIN_n_dofs(e->type(), o);
}



unsigned int BERNSTEIN_n_dofs_at_node(const ElemType t,
                                      const Order o,
                                      const unsigned int n)
{
  switch (t)
    {
    case NODEELEM:
      return 1;

    case EDGE2:
    case EDGE3:
      switch (n)
        {
        case 0:
        case 1:
          return 1;
        case 2:
          libmesh_assert (o>1);
          return (o-1);
        default:
          libmesh_error_msg("ERROR: Invalid node ID " << n << " selected for EDGE2/3!");
        }
    case TRI3:
      libmesh_assert_less (n, 3);
      libmesh_assert_less (o, 2);
      libmesh_fallthrough();
    case TRI6:
      // Internal DoFs are associated with the elem on a Tri6, or node 6 on a Tri7
      libmesh_assert_less (n, 6);
      libmesh_fallthrough();
    case TRI7:
      switch (n)
        {
        case 0:
        case 1:
        case 2:
          return 1;

        case 3:
        case 4:
        case 5:
          return (o-1);

        case 6:
          return ((o-1)*(o-2)/2);
        default:
          libmesh_error_msg("ERROR: Invalid node ID " << n << " selected for TRI!");
        }
    case QUAD4:
      libmesh_assert_less (n, 4);
      libmesh_assert_less (o, 2);
      libmesh_fallthrough();
    case QUAD8:
      libmesh_assert_less (n, 8);
      libmesh_assert_less (o, 3);
      libmesh_fallthrough();
    case QUAD9:
      {
        switch (n)
          {
          case 0:
          case 1:
          case 2:
          case 3:
            return 1;

          case 4:
          case 5:
          case 6:
          case 7:
            return (o-1);

          case 8:
            return ((o-1)*(o-1));

          default:
            libmesh_error_msg("ERROR: Invalid node ID " << n << " selected for QUAD9!");
          }
      }
    case HEX8:
      libmesh_assert_less (n, 8);
      libmesh_assert_less (o, 2);
      libmesh_fallthrough();
    case HEX20:
      libmesh_assert_less (n, 20);
      libmesh_assert_less (o, 3);
      libmesh_fallthrough();
    case HEX27:
      switch (n)
        {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
          return 1;

        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
          return (o-1);

        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
          return ((o-1)*(o-1));

        case 26:
          return ((o-1)*(o-1)*(o-1));

        default:
          libmesh_error_msg("ERROR: Invalid node ID " << n << " selected for HEX27!");
        }
    case TET4:
      libmesh_assert_less (n, 4);
      libmesh_assert_less (o, 2);
      libmesh_fallthrough();
    case TET10:
      libmesh_assert_less (n, 10);
      libmesh_fallthrough();
    case TET14:
      libmesh_assert_less (o, 3);
      switch (n)
        {
        case 0:
        case 1:
        case 2:
        case 3:
          return 1;

        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
          return (o-1);

        case 10:
        case 11:
        case 12:
        case 13:
          return 0;

        default:
          libmesh_error_msg("ERROR: Invalid node ID " << n << " selected for TET4/10/14!");
        }
    case INVALID_ELEM:
      return 0;
    default:
      libmesh_error_msg("ERROR: Invalid ElemType " << Utility::enum_to_string(t) << " selected for BERNSTEIN FE family!");
    }
} // BERNSTEIN_n_dofs_at_node()



unsigned int BERNSTEIN_n_dofs_at_node(const Elem & e,
                                      const Order o,
                                      const unsigned int n)
{
  return BERNSTEIN_n_dofs_at_node(e.type(), o, n);
}



unsigned int BERNSTEIN_n_dofs_per_elem(const ElemType t, const Order o)
{
  switch (t)
    {
    case NODEELEM:
    case EDGE2:
    case EDGE3:
      return 0;
    case TRI3:
    case TRISHELL3:
    case QUAD4:
    case QUADSHELL4:
      return 0;
    case TRI6:
      return ((o-1)*(o-2)/2);
    case TRI7:
      return 0;
    case QUAD8:
    case QUADSHELL8:
      if (o <= 2)
        return 0;
      libmesh_fallthrough();
    case QUAD9:
    case QUADSHELL9:
      return 0;
    case HEX8:
      libmesh_assert_less (o, 2);
      return 0;
    case HEX20:
      libmesh_assert_less (o, 3);
      return 0;
    case HEX27:
      return 0;
    case TET4:
      libmesh_assert_less (o, 2);
      libmesh_fallthrough();
    case TET10:
    case TET14:
      libmesh_assert_less (o, 3);
      return 0;
    case INVALID_ELEM:
      return 0;
    default:
      libmesh_error_msg("ERROR: Invalid ElemType " << Utility::enum_to_string(t) << " selected for BERNSTEIN FE family!");
    }
} // BERNSTEIN_n_dofs_per_elem



unsigned int BERNSTEIN_n_dofs_per_elem(const Elem & e, const Order o)
{
  return BERNSTEIN_n_dofs_per_elem(e.type(), o);
}

} // anonymous namespace


// Instantiate (side_) nodal_soln() function for every dimension
LIBMESH_FE_NODAL_SOLN(BERNSTEIN, bernstein_nodal_soln)
LIBMESH_FE_SIDE_NODAL_SOLN(BERNSTEIN)

// Instantiate n_dofs*() functions for every dimension
LIBMESH_DEFAULT_NDOFS(BERNSTEIN)

// Bernstein FEMs are C^0 continuous
template <> FEContinuity FE<0,BERNSTEIN>::get_continuity() const { return C_ZERO; }
template <> FEContinuity FE<1,BERNSTEIN>::get_continuity() const { return C_ZERO; }
template <> FEContinuity FE<2,BERNSTEIN>::get_continuity() const { return C_ZERO; }
template <> FEContinuity FE<3,BERNSTEIN>::get_continuity() const { return C_ZERO; }

// Bernstein FEMs are not hierarchic
template <> bool FE<0,BERNSTEIN>::is_hierarchic() const { return false; }
template <> bool FE<1,BERNSTEIN>::is_hierarchic() const { return false; }
template <> bool FE<2,BERNSTEIN>::is_hierarchic() const { return false; }
template <> bool FE<3,BERNSTEIN>::is_hierarchic() const { return false; }

#ifdef LIBMESH_ENABLE_AMR
// compute_constraints() specializations are only needed for 2 and 3D
template <>
void FE<2,BERNSTEIN>::compute_constraints (DofConstraints & constraints,
                                           DofMap & dof_map,
                                           const unsigned int variable_number,
                                           const Elem * elem)
{ compute_proj_constraints(constraints, dof_map, variable_number, elem); }

template <>
void FE<3,BERNSTEIN>::compute_constraints (DofConstraints & constraints,
                                           DofMap & dof_map,
                                           const unsigned int variable_number,
                                           const Elem * elem)
{ compute_proj_constraints(constraints, dof_map, variable_number, elem); }
#endif // #ifdef LIBMESH_ENABLE_AMR

// Bernstein shapes need reinit only for approximation orders >= 3,
// but we might reach that with p refinement
template <> bool FE<0,BERNSTEIN>::shapes_need_reinit() const { return true; }
template <> bool FE<1,BERNSTEIN>::shapes_need_reinit() const { return true; }
template <> bool FE<2,BERNSTEIN>::shapes_need_reinit() const { return true; }
template <> bool FE<3,BERNSTEIN>::shapes_need_reinit() const { return true; }

} // namespace libMesh

#endif //LIBMESH_ENABLE_HIGHER_ORDER_SHAPES
