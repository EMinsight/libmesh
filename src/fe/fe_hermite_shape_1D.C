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
#include "libmesh/fe_interface.h"
#include "libmesh/utility.h"
#include "libmesh/enum_to_string.h"

namespace
{
using namespace libMesh;

// Compute the static coefficients for an element
void hermite_compute_coefs(const Elem * elem, Real & d1xd1x, Real & d2xd2x)
{
  const FEFamily mapping_family = FEMap::map_fe_type(*elem);
  const FEType map_fe_type(elem->default_order(), mapping_family);

  // Note: we explicitly don't consider the elem->p_level() when
  // computing the number of mapping shape functions.
  const int n_mapping_shape_functions =
    FEInterface::n_shape_functions (map_fe_type, /*extra_order=*/0, elem);

  // Degrees of freedom are at vertices and edge midpoints
  static const Point dofpt[2] = {Point(-1), Point(1)};

  // Mapping functions - first derivatives at each dofpt
  std::vector<Real> dxdxi(2);
  std::vector<Real> dxidx(2);

  FEInterface::shape_deriv_ptr shape_deriv_ptr =
    FEInterface::shape_deriv_function(map_fe_type, elem);

  for (int p = 0; p != 2; ++p)
    {
      dxdxi[p] = 0;
      for (int i = 0; i != n_mapping_shape_functions; ++i)
        {
          const Real ddxi = shape_deriv_ptr
            (map_fe_type, elem, i, 0, dofpt[p], /*add_p_level=*/false);
          dxdxi[p] += elem->point(i)(0) * ddxi;
        }
    }

  // Calculate derivative scaling factors
  d1xd1x = dxdxi[0];
  d2xd2x = dxdxi[1];
}


} // end anonymous namespace


namespace libMesh
{


LIBMESH_DEFAULT_VECTORIZED_FE(1,HERMITE)


#ifdef LIBMESH_ENABLE_SECOND_DERIVATIVES

template<>
Real FEHermite<1>::hermite_raw_shape_second_deriv (const unsigned int i, const Real xi)
{
  using Utility::pow;

  switch (i)
    {
    case 0:
      return 1.5 * xi;
    case 1:
      return -1.5 * xi;
    case 2:
      return 0.5 * (-1. + 3.*xi);
    case 3:
      return 0.5 * (1. + 3.*xi);
    case 4:
      return (8.*xi*xi + 4.*(xi*xi-1.))/24.;
    case 5:
      return (8.*xi*xi*xi + 12.*xi*(xi*xi-1.))/120.;
      //      case 6:
      //        return (8.*pow<4>(xi) + 20.*xi*xi*(xi*xi-1.) +
      //          2.*(xi*xi-1)*(xi*xi-1))/720.;
    default:
      Real denominator = 720., xipower = 1.;
      for (unsigned n=6; n != i; ++n)
        {
          xipower *= xi;
          denominator *= (n+1);
        }
      return (8.*pow<4>(xi)*xipower +
              (8.*(i-4)+4.)*xi*xi*xipower*(xi*xi-1.) +
              (i-4)*(i-5)*xipower*(xi*xi-1.)*(xi*xi-1.))/denominator;
    }
}

#endif


template<>
Real FEHermite<1>::hermite_raw_shape_deriv(const unsigned int i, const Real xi)
{
  switch (i)
    {
    case 0:
      return 0.75 * (-1. + xi*xi);
    case 1:
      return 0.75 * (1. - xi*xi);
    case 2:
      return 0.25 * (-1. - 2.*xi + 3.*xi*xi);
    case 3:
      return 0.25 * (-1. + 2.*xi + 3.*xi*xi);
    case 4:
      return 4.*xi * (xi*xi-1.)/24.;
    case 5:
      return (4*xi*xi*(xi*xi-1.) + (xi*xi-1.)*(xi*xi-1.))/120.;
      //      case 6:
      //        return (4*xi*xi*xi*(xi*xi-1.) + 2*xi*(xi*xi-1.)*(xi*xi-1.))/720.;
    default:
      Real denominator = 720., xipower = 1.;
      for (unsigned n=6; n != i; ++n)
        {
          xipower *= xi;
          denominator *= (n+1);
        }
      return (4*xi*xi*xi*xipower*(xi*xi-1.) +
              (i-4)*xi*xipower*(xi*xi-1.)*(xi*xi-1.))/denominator;
    }
}

template<>
Real FEHermite<1>::hermite_raw_shape(const unsigned int i, const Real xi)
{
  switch (i)
    {
    case 0:
      return 0.25 * (2. - 3.*xi + xi*xi*xi);
    case 1:
      return 0.25 * (2. + 3.*xi - xi*xi*xi);
    case 2:
      return 0.25 * (1. - xi - xi*xi + xi*xi*xi);
    case 3:
      return 0.25 * (-1. - xi + xi*xi + xi*xi*xi);
      // All high order terms have the form x^(p-4)(x^2-1)^2/p!
    case 4:
      return (xi*xi-1.) * (xi*xi-1.)/24.;
    case 5:
      return xi * (xi*xi-1.) * (xi*xi-1.)/120.;
      //      case 6:
      //        return xi*xi * (xi*xi-1.) * (xi*xi-1.)/720.;
    default:
      Real denominator = 720., xipower = 1.;
      for (unsigned n=6; n != i; ++n)
        {
          xipower *= xi;
          denominator *= (n+1);
        }
      return (xi*xi*xipower*(xi*xi-1.)*(xi*xi-1.))/denominator;

    }
}


template <>
Real FE<1,HERMITE>::shape(const Elem * elem,
                          const Order libmesh_dbg_var(order),
                          const unsigned int i,
                          const Point & p,
                          const bool libmesh_dbg_var(add_p_level))
{
  libmesh_assert(elem);

  // Coefficient naming: d(1)d(2n) is the coefficient of the
  // global shape function corresponding to value 1 in terms of the
  // local shape function corresponding to normal derivative 2
  Real d1xd1x, d2xd2x;

  hermite_compute_coefs(elem, d1xd1x, d2xd2x);

  const ElemType type = elem->type();

#ifndef NDEBUG
  const unsigned int totalorder =
    order + add_p_level * elem->p_level();
#endif

  switch (type)
    {
      // C1 functions on the C1 cubic edge
    case EDGE2:
    case EDGE3:
      {
        libmesh_assert_less (i, totalorder+1);

        switch (i)
          {
          case 0:
            return FEHermite<1>::hermite_raw_shape(0, p(0));
          case 1:
            return d1xd1x * FEHermite<1>::hermite_raw_shape(2, p(0));
          case 2:
            return FEHermite<1>::hermite_raw_shape(1, p(0));
          case 3:
            return d2xd2x * FEHermite<1>::hermite_raw_shape(3, p(0));
          default:
            return FEHermite<1>::hermite_raw_shape(i, p(0));
          }
      }
    default:
      libmesh_error_msg("ERROR: Unsupported element type = " << Utility::enum_to_string(type));
    }
}



template <>
Real FE<1,HERMITE>::shape(const ElemType,
                          const Order,
                          const unsigned int,
                          const Point &)
{
  libmesh_error_msg("Hermite elements require the real element \nto construct gradient-based degrees of freedom.");
  return 0.;
}


template <>
Real FE<1,HERMITE>::shape(const FEType fet,
                          const Elem * elem,
                          const unsigned int i,
                          const Point & p,
                          const bool add_p_level)
{
  return FE<1,HERMITE>::shape(elem, fet.order, i, p, add_p_level);
}


template <>
Real FE<1,HERMITE>::shape_deriv(const Elem * elem,
                                const Order libmesh_dbg_var(order),
                                const unsigned int i,
                                const unsigned int,
                                const Point & p,
                                const bool libmesh_dbg_var(add_p_level))
{
  libmesh_assert(elem);

  // Coefficient naming: d(1)d(2n) is the coefficient of the
  // global shape function corresponding to value 1 in terms of the
  // local shape function corresponding to normal derivative 2
  Real d1xd1x, d2xd2x;

  hermite_compute_coefs(elem, d1xd1x, d2xd2x);

  const ElemType type = elem->type();

#ifndef NDEBUG
  const unsigned int totalorder =
    order + add_p_level * elem->p_level();
#endif

  switch (type)
    {
      // C1 functions on the C1 cubic edge
    case EDGE2:
    case EDGE3:
      {
        libmesh_assert_less (i, totalorder+1);

        switch (i)
          {
          case 0:
            return FEHermite<1>::hermite_raw_shape_deriv(0, p(0));
          case 1:
            return d1xd1x * FEHermite<1>::hermite_raw_shape_deriv(2, p(0));
          case 2:
            return FEHermite<1>::hermite_raw_shape_deriv(1, p(0));
          case 3:
            return d2xd2x * FEHermite<1>::hermite_raw_shape_deriv(3, p(0));
          default:
            return FEHermite<1>::hermite_raw_shape_deriv(i, p(0));
          }
      }
    default:
      libmesh_error_msg("ERROR: Unsupported element type = " << Utility::enum_to_string(type));
    }
}


template <>
Real FE<1,HERMITE>::shape_deriv(const ElemType,
                                const Order,
                                const unsigned int,
                                const unsigned int,
                                const Point &)
{
  libmesh_error_msg("Hermite elements require the real element \nto construct gradient-based degrees of freedom.");
  return 0.;
}

template <>
Real FE<1,HERMITE>::shape_deriv(const FEType fet,
                                const Elem * elem,
                                const unsigned int i,
                                const unsigned int j,
                                const Point & p,
                                const bool add_p_level)
{
  return FE<1,HERMITE>::shape_deriv(elem, fet.order, i, j, p, add_p_level);
}

#ifdef LIBMESH_ENABLE_SECOND_DERIVATIVES


template <>
Real FE<1,HERMITE>::shape_second_deriv(const Elem * elem,
                                       const Order libmesh_dbg_var(order),
                                       const unsigned int i,
                                       const unsigned int,
                                       const Point & p,
                                       const bool libmesh_dbg_var(add_p_level))
{
  libmesh_assert(elem);

  // Coefficient naming: d(1)d(2n) is the coefficient of the
  // global shape function corresponding to value 1 in terms of the
  // local shape function corresponding to normal derivative 2
  Real d1xd1x, d2xd2x;

  hermite_compute_coefs(elem, d1xd1x, d2xd2x);

  const ElemType type = elem->type();

#ifndef NDEBUG
  const unsigned int totalorder =
    order + add_p_level * elem->p_level();
#endif

  switch (type)
    {
      // C1 functions on the C1 cubic edge
    case EDGE2:
    case EDGE3:
      {
        libmesh_assert_less (i, totalorder+1);

        switch (i)
          {
          case 0:
            return FEHermite<1>::hermite_raw_shape_second_deriv(0, p(0));
          case 1:
            return d1xd1x * FEHermite<1>::hermite_raw_shape_second_deriv(2, p(0));
          case 2:
            return FEHermite<1>::hermite_raw_shape_second_deriv(1, p(0));
          case 3:
            return d2xd2x * FEHermite<1>::hermite_raw_shape_second_deriv(3, p(0));
          default:
            return FEHermite<1>::hermite_raw_shape_second_deriv(i, p(0));
          }
      }
    default:
      libmesh_error_msg("ERROR: Unsupported element type = " << Utility::enum_to_string(type));
    }
}


template <>
Real FE<1,HERMITE>::shape_second_deriv(const ElemType,
                                       const Order,
                                       const unsigned int,
                                       const unsigned int,
                                       const Point &)
{
  libmesh_error_msg("Hermite elements require the real element \nto construct gradient-based degrees of freedom.");
  return 0.;
}

template <>
Real FE<1,HERMITE>::shape_second_deriv(const FEType fet,
                                       const Elem * elem,
                                       const unsigned int i,
                                       const unsigned int j,
                                       const Point & p,
                                       const bool add_p_level)
{
  return FE<1,HERMITE>::shape_second_deriv(elem, fet.order, i, j, p, add_p_level);
}


#endif

} // namespace libMesh
