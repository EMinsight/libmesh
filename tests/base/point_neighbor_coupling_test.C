#include <libmesh/equation_systems.h>
#include <libmesh/mesh.h>
#include <libmesh/mesh_generation.h>
#include <libmesh/numeric_vector.h>
#include <libmesh/dof_map.h>
#include <libmesh/elem.h>
#include <libmesh/default_coupling.h>
#include <libmesh/point_neighbor_coupling.h>

#include "test_comm.h"
#include "libmesh_cppunit.h"


using namespace libMesh;



Number cubic_point_neighbor_coupling_test (const Point& p,
                                           const Parameters&,
                                           const std::string&,
                                           const std::string&)
{
  const Real & x = p(0);
  const Real & y = LIBMESH_DIM > 1 ? p(1) : 0;
  const Real & z = LIBMESH_DIM > 2 ? p(2) : 0;

  return x*(1-x)*(1-x) + x*x*(1-y) + x*(1-y)*(1-z) + y*(1-y)*z + z*(1-z)*(1-z);
}



class PointNeighborCouplingTest : public CppUnit::TestCase {
public:
  LIBMESH_CPPUNIT_TEST_SUITE( PointNeighborCouplingTest );

  CPPUNIT_TEST( testCouplingOnEdge3 );
#if LIBMESH_DIM > 1
  CPPUNIT_TEST( testCouplingOnQuad9 );
  CPPUNIT_TEST( testCouplingOnTri6 );
#endif
#if LIBMESH_DIM > 2
  CPPUNIT_TEST( testCouplingOnHex27 );
#endif

  CPPUNIT_TEST_SUITE_END();

private:

public:
  void setUp()
  {}

  void tearDown()
  {}

  void testCoupling(const ElemType elem_type)
  {
    Mesh mesh(*TestCommWorld);

    EquationSystems es(mesh);
    System &sys = es.add_system<System> ("SimpleSystem");
    sys.add_variable("u", THIRD, HIERARCHIC);

    // Remove the default DoF ghosting functors
    sys.get_dof_map().remove_coupling_functor
      (sys.get_dof_map().default_coupling());
    sys.get_dof_map().remove_algebraic_ghosting_functor
      (sys.get_dof_map().default_algebraic_ghosting());

    // Create a replacement functor
    PointNeighborCoupling point_neighbor_coupling;

    // This just re-sets the default; real users may want a real
    // coupling matrix instead.
    point_neighbor_coupling.set_dof_coupling(nullptr);

    point_neighbor_coupling.set_n_levels(3);

    sys.get_dof_map().add_algebraic_ghosting_functor
      (point_neighbor_coupling);

    const unsigned int n_elem_per_side = 5;
    const std::unique_ptr<Elem> test_elem = Elem::build(elem_type);
    const unsigned int ymax = test_elem->dim() > 1;
    const unsigned int zmax = test_elem->dim() > 2;
    const unsigned int ny = ymax * n_elem_per_side;
    const unsigned int nz = zmax * n_elem_per_side;

    MeshTools::Generation::build_cube (mesh,
                                       n_elem_per_side,
                                       ny,
                                       nz,
                                       0., 1.,
                                       0., ymax,
                                       0., zmax,
                                       elem_type);

    es.init();
    sys.project_solution(cubic_point_neighbor_coupling_test, nullptr, es.parameters);

    for (const auto & elem : mesh.active_local_element_ptr_range())
      for (unsigned int s1=0; s1 != elem->n_neighbors(); ++s1)
        {
          const Elem * n1 = elem->neighbor_ptr(s1);
          if (!n1)
            continue;

          libmesh_assert(sys.get_dof_map().is_evaluable(*n1, 0));

          // Let's speed up this test by only checking the ghosted
          // elements which are most likely to break.
          if (n1->processor_id() == mesh.processor_id())
            continue;

          for (unsigned int s2=0; s2 != elem->n_neighbors(); ++s2)
            {
              const Elem * n2 = elem->neighbor_ptr(s2);
              if (!n2 ||
                  n2->processor_id() == mesh.processor_id())
                continue;

              libmesh_assert(sys.get_dof_map().is_evaluable(*n2, 0));

              for (unsigned int s3=0; s3 != elem->n_neighbors(); ++s3)
                {
                  const Elem * n3 = elem->neighbor_ptr(s3);
                  if (!n3 ||
                      n3->processor_id() == mesh.processor_id())
                    continue;

                  libmesh_assert(sys.get_dof_map().is_evaluable(*n3, 0));

                  Point p = n3->vertex_average();

                  LIBMESH_ASSERT_NUMBERS_EQUAL(sys.point_value(0,p,n3),
                                              cubic_point_neighbor_coupling_test(p,es.parameters,"",""),
                                              TOLERANCE*TOLERANCE);
                }
            }
        }
  }



  void testCouplingOnEdge3() { LOG_UNIT_TEST; testCoupling(EDGE3); }
  void testCouplingOnQuad9() { LOG_UNIT_TEST; testCoupling(QUAD9); }
  void testCouplingOnTri6()  { LOG_UNIT_TEST; testCoupling(TRI6); }
  void testCouplingOnHex27() { LOG_UNIT_TEST; testCoupling(HEX27); }

};

CPPUNIT_TEST_SUITE_REGISTRATION( PointNeighborCouplingTest );
