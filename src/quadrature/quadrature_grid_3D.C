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
#include "libmesh/quadrature_grid.h"
#include "libmesh/enum_to_string.h"

namespace libMesh
{


void QGrid::init_3D()
{
#if LIBMESH_DIM == 3

  switch (_type)
    {
      //---------------------------------------------
      // Hex quadrature rules
    case HEX8:
    case HEX20:
    case HEX27:
      {
        // We compute the 3D quadrature rule as a tensor
        // product of the 1D quadrature rule.
        //
        // We ignore p_level in QGrid, since the "order" here is a
        // user-requested number of grid points, nothing to do with
        // the order of exactly-integrated polynomial spaces
        QGrid q1D(1,_order);

        tensor_product_hex( q1D );

        return;
      }



      //---------------------------------------------
      // Tetrahedral quadrature rules
    case TET4:
    case TET10:
    case TET14:
      {
        const unsigned int np = (_order+1)*(_order+2)*(_order+3)/6;
        // Master tet has 1x1 triangle base, height 1, so volume = 1/6
        const Real weight = Real(1)/Real(6)/np;
        const Real dx = Real(1)/(_order+1);
        _points.resize(np);
        _weights.resize(np);

        unsigned int pt = 0;
        for (int i = 0; i != _order + 1; ++i)
          {
            for (int j = 0; j != _order + 1 - i; ++j)
              {
                for (int k = 0; k != _order + 1 - i - j; ++k)
                  {
                    _points[pt](0) = (i+0.5)*dx;
                    _points[pt](1) = (j+0.5)*dx;
                    _points[pt](2) = (k+0.5)*dx;
                    _weights[pt] = weight;
                    pt++;
                  }
              }
          }
        return;
      }


      // Prism quadrature rules
    case PRISM6:
    case PRISM15:
    case PRISM18:
    case PRISM20:
    case PRISM21:
      {
        // We compute the 3D quadrature rule as a tensor
        // product of the 1D quadrature rule and a 2D
        // triangle quadrature rule
        //
        // We ignore p_level in QGrid, since the "order" here is a
        // user-requested number of grid points, nothing to do with
        // the order of exactly-integrated polynomial spaces
        QGrid q1D(1,_order);
        QGrid q2D(2,_order);

        // Initialize the 2D rule (1D is pre-initialized)
        q2D.init(TRI3, 0, /*simple_type_only=*/true);

        tensor_product_prism(q1D, q2D);

        return;
      }



      //---------------------------------------------
      // Pyramid
    case PYRAMID5:
    case PYRAMID13:
    case PYRAMID14:
    case PYRAMID18:
      {
        const unsigned int np = (_order+1)*(_order+2)*(_order+3)/6;
        _points.resize(np);
        _weights.resize(np);
        // Master pyramid has 2x2 base, height 1, so volume = 4/3
        const Real weight = Real(4)/Real(3)/np;
        const Real dx = Real(2)/(_order+1);
        const Real dz = Real(1)/(_order+1);

        unsigned int pt = 0;
        for (int k = 0; k != _order + 1; ++k)
          {
            for (int i = 0; i != _order + 1 - k; ++i)
              {
                for (int j = 0; j != _order + 1 - k - i; ++j)
                  {
                    _points[pt](0) = (i+0.5)*dx-1.0 +
                      (k+0.5)*dz;
                    _points[pt](1) = (j+0.5)*dx-1.0 +
                      (k+0.5)*dz;
                    _points[pt](2) = (k+0.5)*dz;
                    _weights[pt] = weight;
                    pt++;
                  }
              }
          }
        return;
      }



      //---------------------------------------------
      // Unsupported type
    default:
      libmesh_error_msg("ERROR: Unsupported type: " << Utility::enum_to_string(_type));
    }
#endif
}

} // namespace libMesh
