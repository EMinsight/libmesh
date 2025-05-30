#ifndef TYPE_VECTOR_TEST_H
#define TYPE_VECTOR_TEST_H

#include "libmesh_cppunit.h"

#include <libmesh/fuzzy_equals.h>
#include <libmesh/libmesh.h>
#include <libmesh/type_vector.h>

#define TYPEVECTORTEST                          \
  CPPUNIT_TEST( testNorm );                     \
  CPPUNIT_TEST( testNormSq );                   \
                                                \
  CPPUNIT_TEST( testValue );                    \
  CPPUNIT_TEST( testZero );                     \
                                                \
  CPPUNIT_TEST( testEquality );                 \
  CPPUNIT_TEST( testInEquality );               \
  CPPUNIT_TEST( testAssignment );               \
                                                \
  CPPUNIT_TEST( testScalarMult );               \
  CPPUNIT_TEST( testScalarDiv );                \
  CPPUNIT_TEST( testScalarMultAssign );         \
  CPPUNIT_TEST( testScalarDivAssign );          \
                                                \
  CPPUNIT_TEST( testVectorAdd );                \
  CPPUNIT_TEST( testVectorAddScaled );          \
  CPPUNIT_TEST( testVectorSub );                \
  CPPUNIT_TEST( testVectorMult );               \
  CPPUNIT_TEST( testVectorAddAssign );          \
  CPPUNIT_TEST( testVectorSubAssign );          \
                                                \
  CPPUNIT_TEST( testNormBase );                 \
  CPPUNIT_TEST( testNormSqBase );               \
  CPPUNIT_TEST( testValueBase );                \
  CPPUNIT_TEST( testZeroBase );                 \
  CPPUNIT_TEST( testIsZero );                   \
  CPPUNIT_TEST( testSolidAngle );               \
                                                \
  CPPUNIT_TEST( testEqualityBase );             \
  CPPUNIT_TEST( testInEqualityBase );           \
  CPPUNIT_TEST( testAssignmentBase );           \
                                                \
  CPPUNIT_TEST( testScalarMultBase );           \
  CPPUNIT_TEST( testScalarDivBase );            \
  CPPUNIT_TEST( testScalarMultAssignBase );     \
  CPPUNIT_TEST( testScalarDivAssignBase );      \
                                                \
  CPPUNIT_TEST( testVectorAddBase );            \
  CPPUNIT_TEST( testVectorAddScaledBase );      \
  CPPUNIT_TEST( testVectorSubBase );            \
  CPPUNIT_TEST( testVectorMultBase );           \
  CPPUNIT_TEST( testVectorAddAssignBase );      \
  CPPUNIT_TEST( testVectorSubAssignBase );      \
  CPPUNIT_TEST( testReplaceAlgebraicType );


using namespace libMesh;

template <class DerivedClass>
class TypeVectorTestBase : public CppUnit::TestCase {

protected:
  typedef typename DerivedClass::value_type T;

  std::string libmesh_suite_name;

private:
  std::unique_ptr<DerivedClass> m_1_1_1;
  std::unique_ptr<DerivedClass> m_n1_1_n1;
  TypeVector<T>   *basem_1_1_1, *basem_n1_1_n1;

public:
  virtual void setUp()
  {
    m_1_1_1 = std::make_unique<DerivedClass>(1);
    m_n1_1_n1 = std::make_unique<DerivedClass>(-1);

#if LIBMESH_DIM > 1
    (*m_1_1_1)(1) = 1;
    (*m_n1_1_n1)(1) = 1;
#endif
#if LIBMESH_DIM > 2
    (*m_1_1_1)(2) = 1;
    (*m_n1_1_n1)(2) = -1;
#endif


    basem_1_1_1 = m_1_1_1.get();
    basem_n1_1_n1 = m_n1_1_n1.get();
  }

  virtual void tearDown() {}

  void testValue()
  {
    LOG_UNIT_TEST;

    CPPUNIT_ASSERT_EQUAL( T(1), (*m_1_1_1)(0));
    CPPUNIT_ASSERT_EQUAL( T(-1), (*m_n1_1_n1)(0));

#if LIBMESH_DIM > 1
    CPPUNIT_ASSERT_EQUAL( T(1), (*m_1_1_1)(1));
    CPPUNIT_ASSERT_EQUAL( T(1) , (*m_n1_1_n1)(1));
#endif

#if LIBMESH_DIM > 2
    CPPUNIT_ASSERT_EQUAL( T(1), (*m_1_1_1)(2));
    CPPUNIT_ASSERT_EQUAL( T(-1), (*m_n1_1_n1)(2));
#endif
  }

  void testZero()
  {
    LOG_UNIT_TEST;

#if LIBMESH_DIM > 2
    DerivedClass avector(1,1,1);
#elif LIBMESH_DIM > 1
    DerivedClass avector(1,1);
#else
    DerivedClass avector(1);
#endif
    avector.zero();

    for (int i = 0; i != LIBMESH_DIM; ++i)
      CPPUNIT_ASSERT_EQUAL( T(0), avector(i));
  }

  void testNorm()
  {
    LOG_UNIT_TEST;

    LIBMESH_ASSERT_FP_EQUAL(m_1_1_1->norm() ,  std::sqrt(Real(LIBMESH_DIM)) , TOLERANCE*TOLERANCE );
  }

  void testNormSq()
  {
    LOG_UNIT_TEST;

    LIBMESH_ASSERT_FP_EQUAL(m_1_1_1->norm_sq() ,  Real(LIBMESH_DIM) , TOLERANCE*TOLERANCE );
  }

  void testEquality()
  {
    LOG_UNIT_TEST;

    CPPUNIT_ASSERT( (*m_1_1_1) == (*m_1_1_1) );
#if LIBMESH_DIM > 1
    CPPUNIT_ASSERT( !((*m_1_1_1) == (*m_n1_1_n1)) );
#endif
  }

  void testInEquality()
  {
    LOG_UNIT_TEST;

    CPPUNIT_ASSERT( !((*m_1_1_1) != (*m_1_1_1)) );
#if LIBMESH_DIM > 1
    CPPUNIT_ASSERT( (*m_1_1_1) != (*m_n1_1_n1) );
#endif
  }

  void testAssignment()
  {
    LOG_UNIT_TEST;

    DerivedClass avector {*basem_1_1_1};

    for (int i = 0; i != LIBMESH_DIM; ++i)
      CPPUNIT_ASSERT_EQUAL( T(1), (avector)(i) );
  }

  void testScalarInit()  // Not valid for all derived classes!
  {
    LOG_UNIT_TEST;

    DerivedClass avector = 0;
    for (int i = 0; i != LIBMESH_DIM; ++i)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 0.0 , avector(i) , TOLERANCE*TOLERANCE );

    DerivedClass bvector = 2.0;
    LIBMESH_ASSERT_NUMBERS_EQUAL( 2.0 , bvector(0) , TOLERANCE*TOLERANCE );
    for (int i = 1; i != LIBMESH_DIM; ++i)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 0.0 , bvector(i) , TOLERANCE*TOLERANCE );
  }

  void testScalarMult()
  {
    LOG_UNIT_TEST;

    for (int i = 0; i != LIBMESH_DIM; ++i)
    {
      LIBMESH_ASSERT_NUMBERS_EQUAL( 5.0 , ((*m_1_1_1)*5.0)(i) , TOLERANCE*TOLERANCE );
      LIBMESH_ASSERT_NUMBERS_EQUAL( 5.0 , outer_product(*m_1_1_1,5.0)(i) , TOLERANCE*TOLERANCE );
      LIBMESH_ASSERT_NUMBERS_EQUAL( 5.0 , outer_product(5.0,*m_1_1_1)(i) , TOLERANCE*TOLERANCE );
    }
  }

  void testScalarDiv()
  {
    LOG_UNIT_TEST;

    for (int i = 0; i != LIBMESH_DIM; ++i)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 1/Real(5) , ((*m_1_1_1)/5.0)(i) , TOLERANCE*TOLERANCE );
  }

  void testScalarMultAssign()
  {
    LOG_UNIT_TEST;

    DerivedClass avector {*basem_1_1_1};
    avector*=5.0;

    for (int i = 0; i != LIBMESH_DIM; ++i)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 5.0 , avector(i) , TOLERANCE*TOLERANCE );
  }

  void testScalarDivAssign()
  {
    LOG_UNIT_TEST;

    DerivedClass avector {*basem_1_1_1};
    avector/=5.0;

    for (int i = 0; i != LIBMESH_DIM; ++i)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 1/Real(5) , avector(i) , TOLERANCE*TOLERANCE );
  }

  void testVectorAdd()
  {
    LOG_UNIT_TEST;

    LIBMESH_ASSERT_NUMBERS_EQUAL( 0.0 , ((*m_1_1_1)+(*m_n1_1_n1))(0) , TOLERANCE*TOLERANCE );
    if (LIBMESH_DIM > 1)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 2.0 , ((*m_1_1_1)+(*m_n1_1_n1))(1) , TOLERANCE*TOLERANCE );
    if (LIBMESH_DIM > 2)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 0.0 , ((*m_1_1_1)+(*m_n1_1_n1))(2) , TOLERANCE*TOLERANCE );
  }

  void testVectorAddScaled()
  {
    LOG_UNIT_TEST;

    DerivedClass avector {*basem_1_1_1};
    avector.add_scaled((*m_1_1_1),0.5);

    for (int i = 0; i != LIBMESH_DIM; ++i)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 1.5 , avector(i) , TOLERANCE*TOLERANCE );
  }

  void testVectorSub()
  {
    LOG_UNIT_TEST;

    LIBMESH_ASSERT_NUMBERS_EQUAL( 2.0 , ((*m_1_1_1)-(*m_n1_1_n1))(0) , TOLERANCE*TOLERANCE );
    if (LIBMESH_DIM > 1)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 0.0 , ((*m_1_1_1)-(*m_n1_1_n1))(1) , TOLERANCE*TOLERANCE );
    if (LIBMESH_DIM > 2)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 2.0 , ((*m_1_1_1)-(*m_n1_1_n1))(2) , TOLERANCE*TOLERANCE );
  }

  void testVectorMult()
  {
    LOG_UNIT_TEST;

    if (LIBMESH_DIM == 2)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 0.0 , (*m_1_1_1)*(*m_n1_1_n1) , TOLERANCE*TOLERANCE );
    else
      LIBMESH_ASSERT_NUMBERS_EQUAL( -1.0 , (*m_1_1_1)*(*m_n1_1_n1) , TOLERANCE*TOLERANCE );
  }

  void testVectorAddAssign()
  {
    LOG_UNIT_TEST;

    DerivedClass avector {*basem_1_1_1};
    avector+=(*m_1_1_1);

    for (int i = 0; i != LIBMESH_DIM; ++i)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 2.0 , avector(i) , TOLERANCE*TOLERANCE );
  }

  void testVectorSubAssign()
  {
    LOG_UNIT_TEST;

    DerivedClass avector {*basem_1_1_1};
    avector-=(*m_n1_1_n1);

    LIBMESH_ASSERT_NUMBERS_EQUAL( 2.0 , avector(0) , TOLERANCE*TOLERANCE );
    if (LIBMESH_DIM > 1)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 0.0 , avector(1) , TOLERANCE*TOLERANCE );
    if (LIBMESH_DIM > 2)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 2.0 , avector(2) , TOLERANCE*TOLERANCE );
  }

  void testValueBase()
  {
    LOG_UNIT_TEST;

    for (int i = 0; i != LIBMESH_DIM; ++i)
      CPPUNIT_ASSERT_EQUAL( T(1), (*basem_1_1_1)(i));

    CPPUNIT_ASSERT_EQUAL( T(-1), (*basem_n1_1_n1)(0));
    if (LIBMESH_DIM > 1)
      CPPUNIT_ASSERT_EQUAL( T(1 ), (*basem_n1_1_n1)(1));
    if (LIBMESH_DIM > 2)
      CPPUNIT_ASSERT_EQUAL( T(-1), (*basem_n1_1_n1)(2));
  }

  void testZeroBase()
  {
    LOG_UNIT_TEST;

    TypeVector<T> avector((*basem_1_1_1));
    avector.zero();

    for (int i = 0; i != LIBMESH_DIM; ++i)
      CPPUNIT_ASSERT_EQUAL( T(0), avector(i));
  }

  void testIsZero()
  {
    LOG_UNIT_TEST;

    {
#if LIBMESH_DIM > 2
      DerivedClass avector(0,0,0);
#elif LIBMESH_DIM > 1
      DerivedClass avector(0,0);
#else
      DerivedClass avector(0);
#endif
      CPPUNIT_ASSERT(avector.is_zero());
    }
    {
#if LIBMESH_DIM > 2
      DerivedClass avector(1,1,1);
#elif LIBMESH_DIM > 1
      DerivedClass avector(1,1);
#else
      DerivedClass avector(1);
#endif
      CPPUNIT_ASSERT(!avector.is_zero());
    }
  }

  void testSolidAngle()
  {
    LOG_UNIT_TEST;

    const DerivedClass xvec(1.3);
    LIBMESH_ASSERT_NUMBERS_EQUAL( solid_angle(xvec, xvec, xvec), 0, TOLERANCE*TOLERANCE );

#if LIBMESH_DIM > 1
    // This is ambiguous with --enable-complex builds?  We really need
    // to make the vector-from-scalar constructor explicit.
    // const DerivedClass yvec(0.,2.7),
    //                    xydiag(3.1,3.1),
    //                    xypdiag(.8,-.8);

    DerivedClass yvec, xydiag, xypdiag;
    yvec(1) = 2.7;
    xydiag(0)  = 3.1; xydiag(1)  = 3.1;
    xypdiag(0) = 0.8; xypdiag(1) = -.8;

    // Yeah, nothing subtends a non-zero solid angle in 2D either.
    LIBMESH_ASSERT_NUMBERS_EQUAL( solid_angle(xvec, xvec, yvec), 0, TOLERANCE*TOLERANCE );
    LIBMESH_ASSERT_NUMBERS_EQUAL( solid_angle(xvec, yvec, xydiag), 0, TOLERANCE*TOLERANCE );
#endif

#if LIBMESH_DIM > 2
    const DerivedClass zvec(0.,0.,1.1),
                       xzdiag(0.,.7,.7);
    LIBMESH_ASSERT_NUMBERS_EQUAL(solid_angle(xydiag, yvec, zvec),
                                libMesh::pi/4, TOLERANCE*TOLERANCE);
    LIBMESH_ASSERT_NUMBERS_EQUAL(solid_angle(xvec, yvec, xzdiag),
                                libMesh::pi/4, TOLERANCE*TOLERANCE);
    LIBMESH_ASSERT_NUMBERS_EQUAL(solid_angle(xypdiag, xydiag, zvec),
                                libMesh::pi/2, TOLERANCE*TOLERANCE);

    // Icosahedron coordinates are a nice analytic test
    const Real golden = (1 + std::sqrt(Real(5)))/2;
    const DerivedClass icosa1(1., golden, 0.),
                       icosa2(-1., golden, 0.),
                       icosa3(0., 1., golden);
    LIBMESH_ASSERT_NUMBERS_EQUAL(solid_angle(icosa1, icosa2, icosa3),
                                libMesh::pi/5, TOLERANCE*TOLERANCE);
#endif
  }

  void testNormBase()
  {
    LOG_UNIT_TEST;

    LIBMESH_ASSERT_FP_EQUAL( std::sqrt(Real(LIBMESH_DIM)) , basem_1_1_1->norm() , TOLERANCE*TOLERANCE );
  }

  void testNormSqBase()
  {
    LOG_UNIT_TEST;

    LIBMESH_ASSERT_FP_EQUAL( Real(LIBMESH_DIM) , basem_1_1_1->norm_sq() , TOLERANCE*TOLERANCE );
  }

  void testEqualityBase()
  {
    LOG_UNIT_TEST;

    CPPUNIT_ASSERT( (*basem_1_1_1) == (*basem_1_1_1) );
    CPPUNIT_ASSERT( !((*basem_1_1_1) == (*basem_n1_1_n1)) );
    CPPUNIT_ASSERT( relative_fuzzy_equals(*basem_1_1_1, *basem_1_1_1) );
  }

  void testInEqualityBase()
  {
    LOG_UNIT_TEST;

    CPPUNIT_ASSERT( !((*basem_1_1_1) != (*basem_1_1_1)) );
    CPPUNIT_ASSERT( (*basem_1_1_1) != (*basem_n1_1_n1) );
    CPPUNIT_ASSERT( !relative_fuzzy_equals(*basem_1_1_1, *basem_n1_1_n1) );
  }

  void testAssignmentBase()
  {
    LOG_UNIT_TEST;

    TypeVector<T> avector = (*basem_1_1_1);

    for (int i = 0; i != LIBMESH_DIM; ++i)
      CPPUNIT_ASSERT_EQUAL( T(1), (avector)(i) );
  }

  void testScalarMultBase()
  {
    LOG_UNIT_TEST;

    for (int i = 0; i != LIBMESH_DIM; ++i)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 5.0 , ((*basem_1_1_1)*5.0)(i) , TOLERANCE*TOLERANCE );
  }

  void testScalarDivBase()
  {
    LOG_UNIT_TEST;

    for (int i = 0; i != LIBMESH_DIM; ++i)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 1/Real(5) , ((*basem_1_1_1)/5.0)(i) , TOLERANCE*TOLERANCE );
  }

  void testScalarMultAssignBase()
  {
    LOG_UNIT_TEST;

    TypeVector<T> avector(*m_1_1_1);
    avector*=5.0;

    for (int i = 0; i != LIBMESH_DIM; ++i)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 5.0 , avector(i) , TOLERANCE*TOLERANCE );
  }

  void testScalarDivAssignBase()
  {
    LOG_UNIT_TEST;

    TypeVector<T> avector(*m_1_1_1);
    avector/=5.0;

    for (int i = 0; i != LIBMESH_DIM; ++i)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 1/Real(5) , avector(i) , TOLERANCE*TOLERANCE );
  }

  void testVectorAddBase()
  {
    LOG_UNIT_TEST;

    LIBMESH_ASSERT_NUMBERS_EQUAL( 0.0 , ((*basem_1_1_1)+(*basem_n1_1_n1))(0) , TOLERANCE*TOLERANCE );
    if (LIBMESH_DIM > 1)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 2.0 , ((*basem_1_1_1)+(*basem_n1_1_n1))(1) , TOLERANCE*TOLERANCE );
    if (LIBMESH_DIM > 2)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 0.0 , ((*basem_1_1_1)+(*basem_n1_1_n1))(2) , TOLERANCE*TOLERANCE );
  }

  void testVectorAddScaledBase()
  {
    LOG_UNIT_TEST;

    TypeVector<T> avector(*m_1_1_1);
    avector.add_scaled((*basem_1_1_1),0.5);

    for (int i = 0; i != LIBMESH_DIM; ++i)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 1.5 , avector(i) , TOLERANCE*TOLERANCE );
  }

  void testVectorSubBase()
  {
    LOG_UNIT_TEST;

    LIBMESH_ASSERT_NUMBERS_EQUAL( 2.0 , ((*basem_1_1_1)-(*basem_n1_1_n1))(0) , TOLERANCE*TOLERANCE );
    if (LIBMESH_DIM > 1)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 0.0 , ((*basem_1_1_1)-(*basem_n1_1_n1))(1) , TOLERANCE*TOLERANCE );
    if (LIBMESH_DIM > 2)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 2.0 , ((*basem_1_1_1)-(*basem_n1_1_n1))(2) , TOLERANCE*TOLERANCE );
  }

  void testVectorMultBase()
  {
    LOG_UNIT_TEST;

    if (LIBMESH_DIM == 2)
      LIBMESH_ASSERT_NUMBERS_EQUAL(  0.0 , (*basem_1_1_1)*(*basem_n1_1_n1) , TOLERANCE*TOLERANCE );
    else
      LIBMESH_ASSERT_NUMBERS_EQUAL( -1.0 , (*basem_1_1_1)*(*basem_n1_1_n1) , TOLERANCE*TOLERANCE );
  }

  void testVectorAddAssignBase()
  {
    LOG_UNIT_TEST;

    TypeVector<T> avector(*m_1_1_1);
    avector+=(*basem_1_1_1);

    for (int i = 0; i != LIBMESH_DIM; ++i)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 2.0 , avector(i) , TOLERANCE*TOLERANCE );
  }

  void testVectorSubAssignBase()
  {
    LOG_UNIT_TEST;

    TypeVector<T> avector(*m_1_1_1);
    avector-=(*basem_n1_1_n1);

    LIBMESH_ASSERT_NUMBERS_EQUAL( 2.0 , avector(0) , TOLERANCE*TOLERANCE );
    if (LIBMESH_DIM > 1)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 0.0 , avector(1) , TOLERANCE*TOLERANCE );
    if (LIBMESH_DIM > 2)
      LIBMESH_ASSERT_NUMBERS_EQUAL( 2.0 , avector(2) , TOLERANCE*TOLERANCE );
  }

  void testReplaceAlgebraicType()
  {
#ifdef LIBMESH_HAVE_METAPHYSICL
    typedef typename MetaPhysicL::ReplaceAlgebraicType<
        std::vector<TypeVector<double>>,
        typename TensorTools::IncrementRank<
            typename MetaPhysicL::ValueType<std::vector<TypeVector<double>>>::type>::type>::type
        ReplacedType;
    constexpr bool assertion =
        std::is_same<ReplacedType, std::vector<TensorValue<double>>>::value;
    CPPUNIT_ASSERT(assertion);
#endif
  }
};

#endif
