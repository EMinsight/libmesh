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
#include "libmesh/fe.h"
#include "libmesh/elem.h"
#include "libmesh/utility.h"


// Anonymous namespace for functions shared by HIERARCHIC and
// L2_HIERARCHIC implementations. Implementations appear at the bottom
// of this file.
namespace
{
using namespace libMesh;

Real fe_hierarchic_1D_shape(const ElemType,
                            const Order libmesh_dbg_var(order),
                            const unsigned int i,
                            const Point & p);

Real fe_hierarchic_1D_shape_deriv(const ElemType,
                                  const Order libmesh_dbg_var(order),
                                  const unsigned int i,
                                  const unsigned int libmesh_dbg_var(j),
                                  const Point & p);

#ifdef LIBMESH_ENABLE_SECOND_DERIVATIVES

Real fe_hierarchic_1D_shape_second_deriv(const ElemType,
                                         const Order libmesh_dbg_var(order),
                                         const unsigned int i,
                                         const unsigned int libmesh_dbg_var(j),
                                         const Point & p);

#endif // LIBMESH_ENABLE_SECOND_DERIVATIVES

} // anonymous namespace



namespace libMesh
{


LIBMESH_DEFAULT_VECTORIZED_FE(1,HIERARCHIC)
LIBMESH_DEFAULT_VECTORIZED_FE(1,L2_HIERARCHIC)
LIBMESH_DEFAULT_VECTORIZED_FE(1,SIDE_HIERARCHIC)


template <>
Real FE<1,HIERARCHIC>::shape(const ElemType elem_type,
                             const Order order,
                             const unsigned int i,
                             const Point & p)
{
  return fe_hierarchic_1D_shape(elem_type, order, i, p);
}



template <>
Real FE<1,L2_HIERARCHIC>::shape(const ElemType elem_type,
                                const Order order,
                                const unsigned int i,
                                const Point & p)
{
  return fe_hierarchic_1D_shape(elem_type, order, i, p);
}



template <>
Real FE<1,SIDE_HIERARCHIC>::shape(const ElemType,
                                  const Order,
                                  const unsigned int i,
                                  const Point & p)
{
  unsigned int right_side = p(0) > 0; // 0 false, 1 true
  return (right_side == i);
}



template <>
Real FE<1,HIERARCHIC>::shape(const Elem * elem,
                             const Order order,
                             const unsigned int i,
                             const Point & p,
                             const bool add_p_level)
{
  libmesh_assert(elem);

  return fe_hierarchic_1D_shape(elem->type(), order + add_p_level*elem->p_level(), i, p);
}



template <>
Real FE<1,HIERARCHIC>::shape(const FEType fet,
                             const Elem * elem,
                             const unsigned int i,
                             const Point & p,
                             const bool add_p_level)
{
  libmesh_assert(elem);
  return fe_hierarchic_1D_shape(elem->type(), fet.order + add_p_level*elem->p_level(), i, p);
}





template <>
Real FE<1,L2_HIERARCHIC>::shape(const Elem * elem,
                                const Order order,
                                const unsigned int i,
                                const Point & p,
                                const bool add_p_level)
{
  libmesh_assert(elem);

  return fe_hierarchic_1D_shape(elem->type(), order + add_p_level*elem->p_level(), i, p);
}

template <>
Real FE<1,L2_HIERARCHIC>::shape(const FEType fet,
                                const Elem * elem,
                                const unsigned int i,
                                const Point & p,
                                const bool add_p_level)
{
  libmesh_assert(elem);
  return fe_hierarchic_1D_shape(elem->type(), fet.order + add_p_level*elem->p_level(), i, p);
}



template <>
Real FE<1,SIDE_HIERARCHIC>::shape(const Elem *,
                                  const Order,
                                  const unsigned int i,
                                  const Point & p,
                                  const bool)
{
  unsigned int right_side = p(0) > 0; // 0 false, 1 true
  return (right_side == i);
}

template <>
Real FE<1,SIDE_HIERARCHIC>::shape(const FEType,
                                  const Elem *,
                                  const unsigned int i,
                                  const Point & p,
                                  const bool)
{
  unsigned int right_side = p(0) > 0; // 0 false, 1 true
  return (right_side == i);
}


template <>
Real FE<1,HIERARCHIC>::shape_deriv(const ElemType elem_type,
                                   const Order order,
                                   const unsigned int i,
                                   const unsigned int j,
                                   const Point & p)
{
  return fe_hierarchic_1D_shape_deriv(elem_type, order, i, j, p);
}



template <>
Real FE<1,L2_HIERARCHIC>::shape_deriv(const ElemType elem_type,
                                      const Order order,
                                      const unsigned int i,
                                      const unsigned int j,
                                      const Point & p)
{
  return fe_hierarchic_1D_shape_deriv(elem_type, order, i, j, p);
}



template <>
Real FE<1,SIDE_HIERARCHIC>::shape_deriv(const ElemType,
                                        const Order,
                                        const unsigned int,
                                        const unsigned int,
                                        const Point &)
{
  return 0;
}



template <>
Real FE<1,HIERARCHIC>::shape_deriv(const Elem * elem,
                                   const Order order,
                                   const unsigned int i,
                                   const unsigned int j,
                                   const Point & p,
                                   const bool add_p_level)
{
  libmesh_assert(elem);

  return fe_hierarchic_1D_shape_deriv(elem->type(),
                                      order + add_p_level*elem->p_level(), i, j, p);
}



template <>
Real FE<1,HIERARCHIC>::shape_deriv(const FEType fet,
                                   const Elem * elem,
                                   const unsigned int i,
                                   const unsigned int j,
                                   const Point & p,
                                   const bool add_p_level)
{
  libmesh_assert(elem);
  return fe_hierarchic_1D_shape_deriv(elem->type(), fet.order + add_p_level*elem->p_level(), i, j, p);
}




template <>
Real FE<1,L2_HIERARCHIC>::shape_deriv(const Elem * elem,
                                      const Order order,
                                      const unsigned int i,
                                      const unsigned int j,
                                      const Point & p,
                                      const bool add_p_level)
{
  libmesh_assert(elem);

  return fe_hierarchic_1D_shape_deriv(elem->type(),
                                      order + add_p_level*elem->p_level(), i, j, p);
}



template <>
Real FE<1,L2_HIERARCHIC>::shape_deriv(const FEType fet,
                                      const Elem * elem,
                                      const unsigned int i,
                                      const unsigned int j,
                                      const Point & p,
                                      const bool add_p_level)
{
  libmesh_assert(elem);
  return fe_hierarchic_1D_shape_deriv(elem->type(), fet.order + add_p_level*elem->p_level(), i, j, p);
}



template <>
Real FE<1,SIDE_HIERARCHIC>::shape_deriv(const Elem *,
                                        const Order,
                                        const unsigned int,
                                        const unsigned int,
                                        const Point &,
                                        const bool)
{
  return 0;
}



template <>
Real FE<1,SIDE_HIERARCHIC>::shape_deriv(const FEType,
                                        const Elem *,
                                        const unsigned int,
                                        const unsigned int,
                                        const Point &,
                                        const bool)
{
  return 0;
}


#ifdef LIBMESH_ENABLE_SECOND_DERIVATIVES

template <>
Real FE<1,HIERARCHIC>::shape_second_deriv(const ElemType elem_type,
                                          const Order order,
                                          const unsigned int i,
                                          const unsigned int j,
                                          const Point & p)
{
  return fe_hierarchic_1D_shape_second_deriv(elem_type, order, i, j, p);
}




template <>
Real FE<1,L2_HIERARCHIC>::shape_second_deriv(const ElemType elem_type,
                                             const Order order,
                                             const unsigned int i,
                                             const unsigned int j,
                                             const Point & p)
{
  return fe_hierarchic_1D_shape_second_deriv(elem_type, order, i, j, p);
}



template <>
Real FE<1,SIDE_HIERARCHIC>::shape_second_deriv(const ElemType,
                                               const Order,
                                               const unsigned int,
                                               const unsigned int,
                                               const Point &)
{
  return 0;
}



template <>
Real FE<1,HIERARCHIC>::shape_second_deriv(const Elem * elem,
                                          const Order order,
                                          const unsigned int i,
                                          const unsigned int j,
                                          const Point & p,
                                          const bool add_p_level)
{
  libmesh_assert(elem);

  return fe_hierarchic_1D_shape_second_deriv(elem->type(),
                                             order + add_p_level*elem->p_level(), i, j, p);
}



template <>
Real FE<1,HIERARCHIC>::shape_second_deriv(const FEType fet,
                                          const Elem * elem,
                                          const unsigned int i,
                                          const unsigned int j,
                                          const Point & p,
                                          const bool add_p_level)
{
  libmesh_assert(elem);
  return fe_hierarchic_1D_shape_second_deriv(elem->type(),
                                             fet.order + add_p_level*elem->p_level(), i, j, p);
}


template <>
Real FE<1,L2_HIERARCHIC>::shape_second_deriv(const Elem * elem,
                                             const Order order,
                                             const unsigned int i,
                                             const unsigned int j,
                                             const Point & p,
                                             const bool add_p_level)
{
  libmesh_assert(elem);

  return fe_hierarchic_1D_shape_second_deriv(elem->type(),
                                             order + add_p_level*elem->p_level(), i, j, p);
}


template <>
Real FE<1,L2_HIERARCHIC>::shape_second_deriv(const FEType fet,
                                             const Elem * elem,
                                             const unsigned int i,
                                             const unsigned int j,
                                             const Point & p,
                                             const bool add_p_level)
{
  libmesh_assert(elem);
  return fe_hierarchic_1D_shape_second_deriv(elem->type(),
                                             fet.order + add_p_level*elem->p_level(), i, j, p);
}


template <>
Real FE<1,SIDE_HIERARCHIC>::shape_second_deriv(const Elem *,
                                               const Order,
                                               const unsigned int,
                                               const unsigned int,
                                               const Point &,
                                               const bool)
{
  return 0.;
}


template <>
Real FE<1,SIDE_HIERARCHIC>::shape_second_deriv(const FEType,
                                               const Elem *,
                                               const unsigned int,
                                               const unsigned int,
                                               const Point &,
                                               const bool)
{
  return 0.;
}

#endif

} // namespace libMesh



namespace
{
using namespace libMesh;

Real fe_hierarchic_1D_shape(const ElemType,
                            const Order order,
                            const unsigned int i,
                            const Point & p)
{
  libmesh_assert_less (i, order+1u);

  // If we were to define p=0 here, it wouldn't be hierarchic
  libmesh_error_msg_if (order <= 0,
                        "HIERARCHIC FE families do not support p=0");

  // Declare that we are using our own special power function
  // from the Utility namespace.  This saves typing later.
  using Utility::pow;

  const Real xi = p(0);

  Real returnval = 1.;

  switch (i)
    {
    case 0:
      returnval = .5*(1. - xi);
      break;
    case 1:
      returnval = .5*(1.  + xi);
      break;
      // All even-terms have the same form.
      // (xi^p - 1.)/p!
    case 2:
      returnval = (xi*xi - 1.)/2.;
      break;
    case 4:
      returnval = (pow<4>(xi) - 1.)/24.;
      break;
    case 6:
      returnval = (pow<6>(xi) - 1.)/720.;
      break;

      // All odd-terms have the same form.
      // (xi^p - xi)/p!
    case 3:
      returnval = (xi*xi*xi - xi)/6.;
      break;
    case 5:
      returnval = (pow<5>(xi) - xi)/120.;
      break;
    case 7:
      returnval = (pow<7>(xi) - xi)/5040.;
      break;
    default:
      Real denominator = 1.;
      for (unsigned int n=1; n <= i; ++n)
        {
          returnval *= xi;
          denominator *= n;
        }
      // Odd:
      if (i % 2)
        returnval = (returnval - xi)/denominator;
      // Even:
      else
        returnval = (returnval - 1.)/denominator;
      break;
    }

  return returnval;
}



Real fe_hierarchic_1D_shape_deriv(const ElemType,
                                  const Order order,
                                  const unsigned int i,
                                  const unsigned int libmesh_dbg_var(j),
                                  const Point & p)
{
  // only d()/dxi in 1D!
  libmesh_assert_equal_to (j, 0);
  libmesh_assert_less (i, order+1u);

  // If we were to define p=0 here, it wouldn't be hierarchic
  libmesh_error_msg_if (order <= 0,
                        "HIERARCHIC FE families do not support p=0");

  // Declare that we are using our own special power function
  // from the Utility namespace.  This saves typing later.
  using Utility::pow;

  const Real xi = p(0);

  Real returnval = 1.;

  switch (i)
    {
    case 0:
      returnval = -.5;
      break;
    case 1:
      returnval =  .5;
      break;
      // All even-terms have the same form.
      // xi^(p-1)/(p-1)!
    case 2:
      returnval = xi;
      break;
    case 4:
      returnval = pow<3>(xi)/6.;
      break;
    case 6:
      returnval = pow<5>(xi)/120.;
      break;
      // All odd-terms have the same form.
      // (p*xi^(p-1) - 1.)/p!
    case 3:
      returnval = (3*xi*xi - 1.)/6.;
      break;
    case 5:
      returnval = (5.*pow<4>(xi) - 1.)/120.;
      break;
    case 7:
      returnval = (7.*pow<6>(xi) - 1.)/5040.;
      break;
    default:
      Real denominator = 1.;
      for (unsigned int n=1; n != i; ++n)
        {
          returnval *= xi;
          denominator *= n;
        }
      // Odd:
      if (i % 2)
        returnval = (i * returnval - 1.)/denominator/i;
      // Even:
      else
        returnval = returnval/denominator;
      break;
    }

  return returnval;
}



#ifdef LIBMESH_ENABLE_SECOND_DERIVATIVES

Real fe_hierarchic_1D_shape_second_deriv(const ElemType,
                                         const Order order,
                                         const unsigned int i,
                                         const unsigned int libmesh_dbg_var(j),
                                         const Point & p)
{
  // only d2()/d2xi in 1D!
  libmesh_assert_equal_to (j, 0);
  libmesh_assert_less (i, order+1u);

  // If we were to define p=0 here, it wouldn't be hierarchic
  libmesh_error_msg_if (order <= 0,
                        "HIERARCHIC FE families do not support p=0");

  // Declare that we are using our own special power function
  // from the Utility namespace.  This saves typing later.
  using Utility::pow;

  const Real xi = p(0);

  Real returnval = 1.;

  switch (i)
    {
    case 0:
    case 1:
      returnval = 0;
      break;
      // All terms have the same form.
      // xi^(p-2)/(p-2)!
    case 2:
      returnval = 1;
      break;
    case 3:
      returnval = xi;
      break;
    case 4:
      returnval = pow<2>(xi)/2.;
      break;
    case 5:
      returnval = pow<3>(xi)/6.;
      break;
    case 6:
      returnval = pow<4>(xi)/24.;
      break;
    case 7:
      returnval = pow<5>(xi)/120.;
      break;

    default:
      Real denominator = 1.;
      for (unsigned int n=1; n != i; ++n)
        {
          returnval *= xi;
          denominator *= n;
        }
      // Odd:
      if (i % 2)
        returnval = (i * returnval - 1.)/denominator/i;
      // Even:
      else
        returnval = returnval/denominator;
      break;
    }

  return returnval;
}

#endif // LIBMESH_ENABLE_SECOND_DERIVATIVES

} // anonymous namespace
