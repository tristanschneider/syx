#include "Precompile.h"
#include "SyxDetectionTests.h"
#include "SyxTestHelpers.h"
#include "SyxModel.h"
#include "SyxTransform.h"
#include "SyxSimplex.h"
#include "SyxNarrowphase.h"
#include "SyxSpace.h"
#include "SyxPhysicsSystem.h"
#include "SyxMaterialRepository.h"
#include "SyxIPhysicsObject.h"

namespace Syx {
  extern bool TEST_FAILED;
  bool testAllDetection(void) {
    testModel();
    if(TEST_FAILED)
      return TEST_FAILED;
    testTransform();
    return TEST_FAILED;
  }

#ifndef SENABLED
  bool testModel(void) { return false; }
  bool testTransform(void) { return false; }
  bool testSimplex(void) { return false; }
#else
  bool testModel(void) {
    Vec3Vec modelPoints;
    SVec3Vec sPoints;
    for(int i = 0; i < 1000; ++i) {
      Vec3 v = vecRand(-10000, 10000);
      modelPoints.push_back(v);
      sPoints.push_back(sLoadFloats(v.x, v.y, v.z, 0.0f));
    }

    Model model(modelPoints, modelPoints, false);
    for(size_t i = 0; i < sPoints.size(); ++i) {
      SFloats sSupport = model.sGetSupport(sPoints[i]);
      SAlign Vec3 support;
      SVec3::store(sSupport, support);

      float bestDot = support.dot(modelPoints[i]);
      for(const Vec3& testPoint : modelPoints)
        if(testPoint.dot(modelPoints[i]) > bestDot + SYX_EPSILON)
          failTest();
    }
    return TEST_FAILED;
  }

  bool testTransform(void) {
    Transform parent;
    parent.mPos = Vec3(1.0f, 2.0f, 3.0f);
    parent.mRot = Quat::axisAngle(Vec3(1.0f, 4.0f, -1.0f).normalized(), 1.0f);
    parent.mScale = Vec3(0.5f, 3.0f, 0.9f);

    Transform child;
    child.mPos = Vec3(4.0f, -2.0f, -1.0f);
    child.mRot = Quat::axisAngle(Vec3(-1.0f, -1.0f, -1.0f).normalized(), 0.3f);
    child.mScale = Vec3(2.5f, 1.0f, 1.9f);

    Transformer childToWorlder = child.getModelToWorld();
    Transformer worldToChilder = child.getWorldToModel();
    Transformer childToParentToWorlder = parent.getModelToWorld(child);
    Transformer worldToParentToChilder = parent.getWorldToModel(child);

    Vec3 testPoint(0.5f, 4.0f, 2.0f);
    //Verified by hand with a calculator
    Vec3 childToWorld(5.36f, 2.40f, 2.28f);
    Vec3 worldToChild(-1.51f, 4.66f, 2.42f);
    Vec3 childToParentToWorld(6.27f, 7.96f, 2.69f);
    Vec3 worldToParentToChild(-1.50f, 1.95f, 0.16f);
    //Really high epsilon since I didn't use much precision for the calculations
    float epsilon = 0.1f;

    checkResult(child.modelToWorld(testPoint), childToWorld, epsilon);
    checkResult(child.worldToModel(testPoint), worldToChild, epsilon);
    checkResult(child.worldToModel(child.modelToWorld(testPoint)), Vec3(testPoint[0], testPoint[1], testPoint[2]), epsilon);
    checkResult(childToWorlder.transformPoint(testPoint), childToWorld, epsilon);
    checkResult(worldToChilder.transformPoint(testPoint), worldToChild, epsilon);
    checkResult(worldToChilder.transformPoint(childToWorlder.transformPoint(testPoint)), Vec3(testPoint[0], testPoint[1], testPoint[2]), epsilon);
    checkResult(childToParentToWorlder.transformPoint(testPoint), childToParentToWorld, epsilon);
    checkResult(worldToParentToChilder.transformPoint(testPoint), worldToParentToChild, epsilon);
    return TEST_FAILED;
  }

  template <typename Sim, typename Vec, typename Support>
  void performLineTest(const Vec& a, const Vec& b, Sim& simplex, size_t expectedSize) {
    simplex.initialize();
    simplex.add(Support(a), false);
    simplex.add(Support(b), false);

    Vec simplexResult = simplex.solve();
    Vec3 baseResult = -closestOnLine(Vec3::Zero, toVec3(a), toVec3(b));
    checkResult(simplexResult, baseResult);
    checkResult(simplex.size(), expectedSize);
  }

  template <typename Sim, typename Vec, typename Support>
  void testSimplexLine(void) {
    Sim simplex;
    //Simplex doesn't check for behind a because it shouldn't happen in GJK, so don't test for that
    Vec a, b, simplexResult;

    //Between a and b
    a = Vec(-1.0, -1.0f, -1.0f);
    b = Vec(2.0f, 1.0f, 2.0f);
    performLineTest<Sim, Vec, Support>(a, b, simplex, 2);

    //In front of b
    b = Vec(1.0f, 1.0f, 1.0f);
    a = Vec(2.0f, 2.0f, 2.0f);
    performLineTest<Sim, Vec, Support>(a, b, simplex, 1);
    checkResult(simplex.get(SupportID::A), b);
  }

  template <typename Sim, typename Vec, typename Support>
  void performTriangleTest(const Vec& a, const Vec& b, const Vec& c, Sim& simplex, size_t expectedSize) {
    simplex.initialize();
    simplex.add(Support(a), false);
    simplex.add(Support(b), false);
    simplex.add(Support(c), false);

    Vec simplexResult = simplex.solve();
    Vec3 baseResult = -closestOnTri(Vec3::Zero, toVec3(a), toVec3(b), toVec3(c));
    checkResult(simplexResult.normalized(), baseResult.normalized());
    checkResult(simplex.size(), expectedSize);
  }

  template <typename Sim, typename Vec, typename Support>
  void testSimplexTriangle(void) {
    //Simplex doesn't check for behind ab, so don't test for that
    Vec a, b, c;
    Sim simplex;

    //in front of c
    c = Vec(1.0f, 1.0f, 0.0f);
    b = Vec(2.0f, 1.0f, 0.0f);
    a = Vec(1.0f, 2.0f, 0.0f);
    performTriangleTest<Sim, Vec, Support>(a, b, c, simplex, 1);
    checkResult(simplex.get(0), c);

    //in front of bc
    a = Vec(0.5f, -2.0f, 0.0f);
    b = Vec(1.0f, -1.0f, 0.0f);
    c = Vec(-1.0f, -1.0f, 0.0f);
    performTriangleTest<Sim, Vec, Support>(a, b, c, simplex, 2);
    checkResult(simplex.get(SupportID::A), b);
    checkResult(simplex.get(SupportID::B), c);

    //in front of ac
    b = Vec(0.5f, -2.0f, 0.0f);
    a = Vec(1.0f, -1.0f, 0.0f);
    c = Vec(-1.0f, -1.0f, 0.0f);
    performTriangleTest<Sim, Vec, Support>(a, b, c, simplex, 2);
    checkResult(simplex.get(SupportID::A), a);
    checkResult(simplex.get(SupportID::B), c);

    //within triangle above
    a = Vec(-1.0f, -1.0f, -1.0f);
    b = Vec(1.0f, -1.0f, -1.0f);
    c = Vec(0.0f, 1.0f, -1.0f);
    performTriangleTest<Sim, Vec, Support>(a, b, c, simplex, 3);
    checkResult(Vec(simplex.solve().dot(-a))[0] > 0.0f);

    //within triangle below
    a = Vec(-1.0f, -1.0f, 1.0f);
    b = Vec(1.0f, -1.0f, 1.0f);
    c = Vec(0.0f, 1.0f, 1.0f);
    performTriangleTest<Sim, Vec, Support>(a, b, c, simplex, 3);
    checkResult(Vec(simplex.solve().dot(-a))[0] > 0.0f);
  }

  template <typename Sim, typename Vec, typename Support>
  void performTetrahedronTest(const Vec& a, const Vec& b, const Vec& c, const Vec& d, Sim& simplex, size_t expectedSize) {
    //Verify winding of input tetrahedron, otherwise it's an unfair test
    Vec mid = Vec::scale(a + b + c + d, Vec(0.25f));
    Vec abc = triangleNormal(a, b, c);
    Vec bdc = triangleNormal(b, d, c);
    Vec dac = triangleNormal(d, a, c);
    Vec adb = triangleNormal(a, d, b);
    Vec abcDir = Vec((mid - a).dot(abc));
    Vec bdcDir = Vec((mid - b).dot(bdc));
    Vec dacDir = Vec((mid - d).dot(dac));
    Vec adbDir = Vec((mid - a).dot(adb));
    SyxAssertError(abcDir[0] < 0.0f && bdcDir[0] < 0.0f && dacDir[0] < 0.0f && adbDir[0] < 0.0f,
      "Improperly wound input tetrahedron.");

    simplex.initialize();
    simplex.add(Support(a), false);
    simplex.add(Support(b), false);
    simplex.add(Support(c), false);
    simplex.add(Support(d), false);

    Vec simplexResult = simplex.solve();
    Vec3 baseResult = -closestOnTetrahedron(toVec3(a), toVec3(b), toVec3(c), toVec3(d), Vec3::Zero);
    checkResult(simplexResult.safeNormalized(), baseResult.safeNormalized());
    checkResult(simplex.size(), expectedSize);
  }

  template <typename Sim, typename Vec, typename Support>
  void testSimplexTetrahedron(void) {
    //Simplex doesn't check for in front of abc so don't test for that
    Sim simplex;
    Vec a, b, c, d;

    //In front of dba
    a = Vec(1.0f, 0.0f, -2.0f);
    b = Vec(-1.0f, 0.0f, -2.0f);
    c = Vec(-0.2f, 0.3f, -2.0f);
    d = Vec(0.0f, 0.5f, 1.0f);
    performTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 3);
    checkResult(simplex.contains(a));
    checkResult(simplex.contains(b));
    checkResult(simplex.contains(d));

    //In front of dcb
    a = Vec(1.28f, -0.37f, -1.16f);
    b = Vec(-0.72f, -0.42f, -1.08f);
    c = Vec(0.147f, -0.072f, -1.08f);
    d = Vec(0.347f, 0.128f, 1.922f);
    performTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 3);
    checkResult(simplex.contains(b));
    checkResult(simplex.contains(c));
    checkResult(simplex.contains(d));

    //In front of dac
    a = Vec(0.68f, -0.6f, -1.85f);
    b = Vec(-1.3f, 0.0f, 0.0f);
    c = Vec(-0.45f, -0.3f, -1.77f);
    d = Vec(-0.25f, -0.11f, 1.23f);
    performTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 3);
    checkResult(simplex.contains(a));
    checkResult(simplex.contains(c));
    checkResult(simplex.contains(d));

    //In front of da
    a = Vec(-0.188f, -0.33f, -1.163f);
    b = Vec(-2.188f, -0.381f, -1.145f);
    c = Vec(-1.322f, -0.03f, -1.018f);
    d = Vec(-1.122f, 0.17f, 1.922f);
    performTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 2);
    checkResult(simplex.contains(a));
    checkResult(simplex.contains(d));

    //In front of db
    a = Vec(1.935f, -0.276f, -1.163f);
    b = Vec(-0.163f, -0.145f, -1.215f);
    c = Vec(0.8f, 0.024f, -1.02f);
    d = Vec(1.0f, 0.224f, 1.922f);
    performTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 2);
    checkResult(simplex.contains(b));
    checkResult(simplex.contains(d));

    //In front of dc
    a = Vec(1.051f, -0.749f, -1.163f);
    b = Vec(-0.948f, -0.8f, -1.145f);
    c = Vec(-0.083f, -0.449f, -1.018f);
    d = Vec(0.117f, -0.249f, 1.922f);
    performTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 2);
    checkResult(simplex.contains(c));
    checkResult(simplex.contains(d));

    //In front of d
    a = Vec(1.092f, -0.33f, -3.571f);
    b = Vec(-0.907f, -0.381f, -3.553f);
    c = Vec(-0.042f, -0.03f, -3.426f);
    d = Vec(0.158f, 0.17f, -0.486f);
    performTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 1);
    checkResult(simplex.contains(d));

    //Inside tetrahedron
    a = Vec(1.092f, -0.16f, -0.24f);
    b = Vec(-0.907f, -0.211f, -0.221f);
    c = Vec(-0.042f, 0.14f, -0.094f);
    d = Vec(0.158f, 0.34f, 2.845f);
    performTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 4);
  }

  bool testSimplex(void) {
    TEST_FAILED = false;
    //TestSimplexLine<SSimplex, SVec3, SSupportPoint>();
    //TestSimplexTriangle<SSimplex, SVec3, SSupportPoint>();
    //TestSimplexTetrahedron<SSimplex, SVec3, SSupportPoint>();

    testSimplexLine<Simplex, Vec3, SupportPoint>();
    testSimplexTriangle<Simplex, Vec3, SupportPoint>();
    testSimplexTetrahedron<Simplex, Vec3, SupportPoint>();
    return TEST_FAILED;
  }

  bool NarrowphaseTest::run(void) {
    PhysicsSystem system;
    auto spacePtr = system.createSpace();
    const Handle space = spacePtr->_getHandle();
    auto mat = system.getMaterialRepository().addMaterial({});
    auto sphere = std::make_shared<Model>(ModelType::Sphere);

    auto a = spacePtr->addPhysicsObject(true, true, *mat, sphere);
    auto b = spacePtr->addPhysicsObject(true, true, *mat, sphere);
    a->tryGetCollider()->setModel(sphere);
    b->tryGetCollider()->setModel(sphere);

    auto s = *system._getSpace(space);
    PhysicsObject& objA = *std::find_if(s.mObjects.begin(), s.mObjects.end(), [h(a->getHandle())](auto&& o) { return o.getHandle() == h; });
    PhysicsObject& objB = *std::find_if(s.mObjects.begin(), s.mObjects.end(), [h(b->getHandle())](auto&& o) { return o.getHandle() == h; });
    Narrowphase& narrow = system._getSpace(space)->mNarrowphase;
    narrow.mA = &objA;
    narrow.mB = &objB;
    narrow.mInstA = &objA.getCollider()->getModelInstance();
    narrow.mInstB = &objB.getCollider()->getModelInstance();
    float diameter = 2.0f;
    system.setPosition(space, a->getHandle(), Vec3(0.0f));
    int numTests = 100000;

    //Non colliding tests
    for(int i = 0; i < numTests; ++i) {
      float randRadius = randFloat(diameter + SYX_EPSILON, 1000.0f);
      Vec3 randSphere = randOnSphere();
      Vec3 randPos = randSphere*randRadius;
      system.setPosition(space, b->getHandle(), randPos);
      //CheckResult(narrow.SGJK() == false);
      checkResult(narrow._gjk() == false);
    }

    //Colliding tests
    for(int i = 0; i < numTests; ++i) {
      float randRadius = randFloat(0.0f, diameter);
      Vec3 randSphere = randOnSphere();
      Vec3 randPos = randSphere*randRadius;
      system.setPosition(space, b->getHandle(), randPos);
      //CheckResult(narrow.SGJK() == true);
      checkResult(narrow._gjk() == true);
    }

    return TEST_FAILED;
  }
#endif
}