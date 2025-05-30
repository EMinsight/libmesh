// libmesh includes
#include "libmesh/elem.h"
#include "libmesh/equation_systems.h"
#include "libmesh/mesh.h"
#include "libmesh/mesh_generation.h"
#include "libmesh/numeric_vector.h"
#include "libmesh/parsed_fem_function.h"
#include "libmesh/system.h"

#ifdef LIBMESH_HAVE_FPARSER

// test includes
#include "test_comm.h"
#include "libmesh_cppunit.h"

// C++ includes
#include <memory>


using namespace libMesh;

class ParsedFEMFunctionTest : public CppUnit::TestCase
{
public:
  void setUp() {
#if LIBMESH_DIM > 2
    mesh = std::make_unique<Mesh>(*TestCommWorld);
    MeshTools::Generation::build_cube(*mesh, 1, 1, 1);
    es = std::make_unique<EquationSystems>(*mesh);
    sys = &(es->add_system<System> ("SimpleSystem"));
    sys->add_variable("x2");
    sys->add_variable("x3");
    sys->add_variable("c05");
    sys->add_variable("y4");
    sys->add_variable("xy");
    sys->add_variable("yz");
    sys->add_variable("xyz");

    es->init();

    NumericVector<Number> & sol = *sys->solution;
    Elem *elem = mesh->query_elem_ptr(0);

    if (elem && elem->processor_id() == TestCommWorld->rank())
      {
        // Set x2 = 2*x
        sol.set(elem->node_ref(1).dof_number(0,0,0), 2);
        sol.set(elem->node_ref(2).dof_number(0,0,0), 2);
        sol.set(elem->node_ref(5).dof_number(0,0,0), 2);
        sol.set(elem->node_ref(6).dof_number(0,0,0), 2);

        // Set x3 = 3*x
        sol.set(elem->node_ref(1).dof_number(0,1,0), 3);
        sol.set(elem->node_ref(2).dof_number(0,1,0), 3);
        sol.set(elem->node_ref(5).dof_number(0,1,0), 3);
        sol.set(elem->node_ref(6).dof_number(0,1,0), 3);

        // Set c05 = 0.5
        sol.set(elem->node_ref(0).dof_number(0,2,0), 0.5);
        sol.set(elem->node_ref(1).dof_number(0,2,0), 0.5);
        sol.set(elem->node_ref(2).dof_number(0,2,0), 0.5);
        sol.set(elem->node_ref(3).dof_number(0,2,0), 0.5);
        sol.set(elem->node_ref(4).dof_number(0,2,0), 0.5);
        sol.set(elem->node_ref(5).dof_number(0,2,0), 0.5);
        sol.set(elem->node_ref(6).dof_number(0,2,0), 0.5);
        sol.set(elem->node_ref(7).dof_number(0,2,0), 0.5);

        // Set y4 = 4*y
        sol.set(elem->node_ref(2).dof_number(0,3,0), 4);
        sol.set(elem->node_ref(3).dof_number(0,3,0), 4);
        sol.set(elem->node_ref(6).dof_number(0,3,0), 4);
        sol.set(elem->node_ref(7).dof_number(0,3,0), 4);

        // Set xy = x*y
        sol.set(elem->node_ref(2).dof_number(0,4,0), 1);
        sol.set(elem->node_ref(6).dof_number(0,4,0), 1);

        // Set yz = y*z
        sol.set(elem->node_ref(6).dof_number(0,5,0), 1);
        sol.set(elem->node_ref(7).dof_number(0,5,0), 1);

        // Set xyz = x*y*z
        sol.set(elem->node_ref(6).dof_number(0,6,0), 1);
      }

    sol.close();
    sys->update();

    c = std::make_unique<FEMContext>(*sys);
    s = std::make_unique<FEMContext>(*sys);
    if (elem && elem->processor_id() == TestCommWorld->rank())
      {
        c->get_element_fe(0)->get_phi();
        c->get_element_fe(0)->get_dphi();
#ifdef LIBMESH_ENABLE_SECOND_DERIVATIVES
        c->get_element_fe(0)->get_d2phi();
#endif
        c->pre_fe_reinit(*sys, elem);
        c->elem_fe_reinit();

        s->get_side_fe(0)->get_normals(); // Prerequest
        s->pre_fe_reinit(*sys, elem);
        s->side = 3;
        s->side_fe_reinit();
      }
#endif
  }

  void tearDown() {
    c.reset();
    s.reset();
    es.reset();
    mesh.reset();
  }

  LIBMESH_CPPUNIT_TEST_SUITE(ParsedFEMFunctionTest);

#if LIBMESH_DIM > 2
  CPPUNIT_TEST(testValues);
  CPPUNIT_TEST(testGradients);
#ifdef LIBMESH_ENABLE_SECOND_DERIVATIVES
  CPPUNIT_TEST(testHessians);
#endif
  CPPUNIT_TEST(testInlineGetter);
  CPPUNIT_TEST(testInlineSetter);
  CPPUNIT_TEST(testNormals);
#endif

  CPPUNIT_TEST_SUITE_END();


private:
  std::unique_ptr<UnstructuredMesh> mesh;
  std::unique_ptr<EquationSystems> es;
  System * sys;
  std::unique_ptr<FEMContext> c, s;

  void testValues()
  {
    LOG_UNIT_TEST;

    if (c->has_elem() &&
        c->get_elem().processor_id() == TestCommWorld->rank())
      {
        // Test that we can copy these into vectors
        std::vector<ParsedFEMFunction<Number>> pfvec;

        {
          ParsedFEMFunction<Number> x2(*sys, "x2");
          ParsedFEMFunction<Number> xy8(*sys, "x2*y4");

          // Test that move constructor works
          ParsedFEMFunction<Number> xy8_stolen(std::move(xy8));

          pfvec.push_back(xy8_stolen);

          LIBMESH_ASSERT_NUMBERS_EQUAL
            (2.0, xy8_stolen(*c,Point(0.5,0.5,0.5)), TOLERANCE*TOLERANCE);
        }

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (2.0, pfvec[0](*c,Point(0.5,0.5,0.5)), TOLERANCE*TOLERANCE);
      }
  }


  void testGradients()
  {
    LOG_UNIT_TEST;

    if (c->has_elem() &&
        c->get_elem().processor_id() == TestCommWorld->rank())
      {
        ParsedFEMFunction<Number> c2(*sys, "grad_x_x2");

        // Test that copy/move assignment fails to compile. Note:
        // ParsedFEMFunction is neither move-assignable nor
        // copy-assignable because it contains a const reference.
        // ParsedFEMFunction<Number> c2_assigned(*sys, "grad_y_xyz");
        // c2_assigned = c2;

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (2.0, c2(*c,Point(0.35,0.45,0.55)), TOLERANCE*TOLERANCE);

        ParsedFEMFunction<Number> xz(*sys, "grad_y_xyz");

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (0.1875, xz(*c,Point(0.25,0.35,0.75)), TOLERANCE*TOLERANCE);

        ParsedFEMFunction<Number> xyz(*sys, "grad_y_xyz*grad_x_xy");

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (0.09375, xyz(*c,Point(0.25,0.5,0.75)), TOLERANCE*TOLERANCE);
      }
  }


  void testHessians()
  {
    LOG_UNIT_TEST;

    if (c->has_elem() &&
        c->get_elem().processor_id() == TestCommWorld->rank())
      {
        ParsedFEMFunction<Number> c1(*sys, "hess_xy_xy");

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (1.0, c1(*c,Point(0.35,0.45,0.55)), TOLERANCE*TOLERANCE);

        ParsedFEMFunction<Number> x(*sys, "hess_yz_xyz");

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (0.25, x(*c,Point(0.25,0.35,0.55)), TOLERANCE*TOLERANCE);

        ParsedFEMFunction<Number> xz(*sys, "hess_yz_xyz*grad_y_yz");

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (0.1875, xz(*c,Point(0.25,0.4,0.75)), TOLERANCE*TOLERANCE);
      }
  }

  void testInlineGetter()
  {
    LOG_UNIT_TEST;

    if (c->has_elem() &&
        c->get_elem().processor_id() == TestCommWorld->rank())
      {
        ParsedFEMFunction<Number> ax2(*sys, "a:=4.5;a*x2");

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (2.25, ax2(*c,Point(0.25,0.25,0.25)), TOLERANCE*TOLERANCE);

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (4.5, ax2.get_inline_value("a"), TOLERANCE*TOLERANCE);

        ParsedFEMFunction<Number> cxy8
          (*sys, "a := 4 ; b := a/2+1; c:=b-a+3.5; c*x2*y4");

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (5.0, cxy8(*c,Point(0.5,0.5,0.5)), TOLERANCE*TOLERANCE);

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (3.0, cxy8.get_inline_value("b"), TOLERANCE*TOLERANCE);

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (2.5, cxy8.get_inline_value("c"), TOLERANCE*TOLERANCE);
      }
  }

  void testInlineSetter()
  {
    LOG_UNIT_TEST;

    if (c->has_elem() &&
        c->get_elem().processor_id() == TestCommWorld->rank())
      {
        ParsedFEMFunction<Number> ax2(*sys, "a:=4.5;a*x2");
        ax2.set_inline_value("a", 2.5);

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (1.25, ax2(*c,Point(0.25,0.25,0.25)), TOLERANCE*TOLERANCE);

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (2.5, ax2.get_inline_value("a"), TOLERANCE*TOLERANCE);

        ParsedFEMFunction<Number> cxy8
          (*sys, "a := 4 ; b := a/2+1; c:=b-a+3.5; c*x2*y4");

        cxy8.set_inline_value("a", 2);

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (7.0, cxy8(*c,Point(0.5,0.5,0.5)), TOLERANCE*TOLERANCE);

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (2.0, cxy8.get_inline_value("b"), TOLERANCE*TOLERANCE);

        LIBMESH_ASSERT_NUMBERS_EQUAL
          (3.5, cxy8.get_inline_value("c"), TOLERANCE*TOLERANCE);

      }
  }

  void testNormals()
  {
    LOG_UNIT_TEST;

    if (s->has_elem() &&
        s->get_elem().processor_id() == TestCommWorld->rank())
      {
        ParsedFEMFunction<Number> nx(*sys, "n_x");

        ParsedFEMFunction<Number> ny(*sys, "n_y");

        ParsedFEMFunction<Number> nz(*sys, "n_z");

        const std::vector<Point> & xyz = s->get_side_fe(0)->get_xyz();

        // On side 3 of a hex the normal direction is +y
        for (std::size_t qp=0; qp != xyz.size(); ++qp)
          {
            LIBMESH_ASSERT_NUMBERS_EQUAL
              (0.0, nx(*s,xyz[qp]), TOLERANCE*TOLERANCE);
            LIBMESH_ASSERT_NUMBERS_EQUAL
              (1.0, ny(*s,xyz[qp]), TOLERANCE*TOLERANCE);
            LIBMESH_ASSERT_NUMBERS_EQUAL
              (0.0, nz(*s,xyz[qp]), TOLERANCE*TOLERANCE);
          }
      }
  }

};

CPPUNIT_TEST_SUITE_REGISTRATION(ParsedFEMFunctionTest);

#endif // #ifdef LIBMESH_HAVE_FPARSER
