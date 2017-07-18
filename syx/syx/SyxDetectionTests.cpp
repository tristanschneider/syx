#include "Precompile.h"
#include "SyxDetectionTests.h"
#include "SyxTestHelpers.h"
#include "SyxModel.h"
#include "SyxTransform.h"
#include "SyxSimplex.h"
#include "SyxNarrowphase.h"
#include "SyxSpace.h"
#include "SyxPhysicsSystem.h"

namespace Syx {
  extern bool TEST_FAILED;
  bool TestAllDetection(void) {
    TestModel();
    if(TEST_FAILED)
      return TEST_FAILED;
    TestTransform();
    return TEST_FAILED;
  }

#ifndef SENABLED
  bool TestModel(void) { return false; }
  bool TestTransform(void) { return false; }
  bool TestSimplex(void) { return false; }
#else
  bool TestModel(void) {
    Vec3Vec modelPoints;
    SVec3Vec sPoints;
    for(int i = 0; i < 1000; ++i) {
      Vec3 v = VecRand(-10000, 10000);
      modelPoints.push_back(v);
      sPoints.push_back(SLoadFloats(v.x, v.y, v.z, 0.0f));
    }

    Model model(modelPoints, modelPoints, false);
    for(size_t i = 0; i < sPoints.size(); ++i) {
      SFloats sSupport = model.SGetSupport(sPoints[i]);
      SAlign Vec3 support;
      SVec3::Store(sSupport, support);

      float bestDot = support.Dot(modelPoints[i]);
      for(const Vec3& testPoint : modelPoints)
        if(testPoint.Dot(modelPoints[i]) > bestDot + SYX_EPSILON)
          FailTest();
    }
    return TEST_FAILED;
  }

  bool TestTransform(void) {
    Transform parent;
    parent.mPos = Vec3(1.0f, 2.0f, 3.0f);
    parent.mRot = Quat::AxisAngle(Vec3(1.0f, 4.0f, -1.0f).Normalized(), 1.0f);
    parent.mScale = Vec3(0.5f, 3.0f, 0.9f);

    Transform child;
    child.mPos = Vec3(4.0f, -2.0f, -1.0f);
    child.mRot = Quat::AxisAngle(Vec3(-1.0f, -1.0f, -1.0f).Normalized(), 0.3f);
    child.mScale = Vec3(2.5f, 1.0f, 1.9f);

    Transformer childToWorlder = child.GetModelToWorld();
    Transformer worldToChilder = child.GetWorldToModel();
    Transformer childToParentToWorlder = parent.GetModelToWorld(child);
    Transformer worldToParentToChilder = parent.GetWorldToModel(child);

    Vec3 testPoint(0.5f, 4.0f, 2.0f);
    //Verified by hand with a calculator
    Vec3 childToWorld(5.36f, 2.40f, 2.28f);
    Vec3 worldToChild(-1.51f, 4.66f, 2.42f);
    Vec3 childToParentToWorld(6.27f, 7.96f, 2.69f);
    Vec3 worldToParentToChild(-1.50f, 1.95f, 0.16f);
    //Really high epsilon since I didn't use much precision for the calculations
    float epsilon = 0.1f;

    CheckResult(child.ModelToWorld(testPoint), childToWorld, epsilon);
    CheckResult(child.WorldToModel(testPoint), worldToChild, epsilon);
    CheckResult(child.WorldToModel(child.ModelToWorld(testPoint)), Vec3(testPoint[0], testPoint[1], testPoint[2]), epsilon);
    CheckResult(childToWorlder.TransformPoint(testPoint), childToWorld, epsilon);
    CheckResult(worldToChilder.TransformPoint(testPoint), worldToChild, epsilon);
    CheckResult(worldToChilder.TransformPoint(childToWorlder.TransformPoint(testPoint)), Vec3(testPoint[0], testPoint[1], testPoint[2]), epsilon);
    CheckResult(childToParentToWorlder.TransformPoint(testPoint), childToParentToWorld, epsilon);
    CheckResult(worldToParentToChilder.TransformPoint(testPoint), worldToParentToChild, epsilon);
    return TEST_FAILED;
  }

  template <typename Sim, typename Vec, typename Support>
  void PerformLineTest(const Vec& a, const Vec& b, Sim& simplex, size_t expectedSize) {
    simplex.Initialize();
    simplex.Add(Support(a), false);
    simplex.Add(Support(b), false);

    Vec simplexResult = simplex.Solve();
    Vec3 baseResult = -ClosestOnLine(Vec3::Zero, ToVec3(a), ToVec3(b));
    CheckResult(simplexResult, baseResult);
    CheckResult(simplex.Size(), expectedSize);
  }

  template <typename Sim, typename Vec, typename Support>
  void TestSimplexLine(void) {
    Sim simplex;
    //Simplex doesn't check for behind a because it shouldn't happen in GJK, so don't test for that
    Vec a, b, simplexResult;

    //Between a and b
    a = Vec(-1.0, -1.0f, -1.0f);
    b = Vec(2.0f, 1.0f, 2.0f);
    PerformLineTest<Sim, Vec, Support>(a, b, simplex, 2);

    //In front of b
    b = Vec(1.0f, 1.0f, 1.0f);
    a = Vec(2.0f, 2.0f, 2.0f);
    PerformLineTest<Sim, Vec, Support>(a, b, simplex, 1);
    CheckResult(simplex.Get(SupportID::A), b);
  }

  template <typename Sim, typename Vec, typename Support>
  void PerformTriangleTest(const Vec& a, const Vec& b, const Vec& c, Sim& simplex, size_t expectedSize) {
    simplex.Initialize();
    simplex.Add(Support(a), false);
    simplex.Add(Support(b), false);
    simplex.Add(Support(c), false);

    Vec simplexResult = simplex.Solve();
    Vec3 baseResult = -ClosestOnTri(Vec3::Zero, ToVec3(a), ToVec3(b), ToVec3(c));
    CheckResult(simplexResult.Normalized(), baseResult.Normalized());
    CheckResult(simplex.Size(), expectedSize);
  }

  template <typename Sim, typename Vec, typename Support>
  void TestSimplexTriangle(void) {
    //Simplex doesn't check for behind ab, so don't test for that
    Vec a, b, c;
    Sim simplex;

    //in front of c
    c = Vec(1.0f, 1.0f, 0.0f);
    b = Vec(2.0f, 1.0f, 0.0f);
    a = Vec(1.0f, 2.0f, 0.0f);
    PerformTriangleTest<Sim, Vec, Support>(a, b, c, simplex, 1);
    CheckResult(simplex.Get(0), c);

    //in front of bc
    a = Vec(0.5f, -2.0f, 0.0f);
    b = Vec(1.0f, -1.0f, 0.0f);
    c = Vec(-1.0f, -1.0f, 0.0f);
    PerformTriangleTest<Sim, Vec, Support>(a, b, c, simplex, 2);
    CheckResult(simplex.Get(SupportID::A), b);
    CheckResult(simplex.Get(SupportID::B), c);

    //in front of ac
    b = Vec(0.5f, -2.0f, 0.0f);
    a = Vec(1.0f, -1.0f, 0.0f);
    c = Vec(-1.0f, -1.0f, 0.0f);
    PerformTriangleTest<Sim, Vec, Support>(a, b, c, simplex, 2);
    CheckResult(simplex.Get(SupportID::A), a);
    CheckResult(simplex.Get(SupportID::B), c);

    //within triangle above
    a = Vec(-1.0f, -1.0f, -1.0f);
    b = Vec(1.0f, -1.0f, -1.0f);
    c = Vec(0.0f, 1.0f, -1.0f);
    PerformTriangleTest<Sim, Vec, Support>(a, b, c, simplex, 3);
    CheckResult(Vec(simplex.Solve().Dot(-a))[0] > 0.0f);

    //within triangle below
    a = Vec(-1.0f, -1.0f, 1.0f);
    b = Vec(1.0f, -1.0f, 1.0f);
    c = Vec(0.0f, 1.0f, 1.0f);
    PerformTriangleTest<Sim, Vec, Support>(a, b, c, simplex, 3);
    CheckResult(Vec(simplex.Solve().Dot(-a))[0] > 0.0f);
  }

  template <typename Sim, typename Vec, typename Support>
  void PerformTetrahedronTest(const Vec& a, const Vec& b, const Vec& c, const Vec& d, Sim& simplex, size_t expectedSize) {
    //Verify winding of input tetrahedron, otherwise it's an unfair test
    Vec mid = Vec::Scale(a + b + c + d, Vec(0.25f));
    Vec abc = TriangleNormal(a, b, c);
    Vec bdc = TriangleNormal(b, d, c);
    Vec dac = TriangleNormal(d, a, c);
    Vec adb = TriangleNormal(a, d, b);
    Vec abcDir = Vec((mid - a).Dot(abc));
    Vec bdcDir = Vec((mid - b).Dot(bdc));
    Vec dacDir = Vec((mid - d).Dot(dac));
    Vec adbDir = Vec((mid - a).Dot(adb));
    SyxAssertError(abcDir[0] < 0.0f && bdcDir[0] < 0.0f && dacDir[0] < 0.0f && adbDir[0] < 0.0f,
      "Improperly wound input tetrahedron.");

    simplex.Initialize();
    simplex.Add(Support(a), false);
    simplex.Add(Support(b), false);
    simplex.Add(Support(c), false);
    simplex.Add(Support(d), false);

    Vec simplexResult = simplex.Solve();
    Vec3 baseResult = -ClosestOnTetrahedron(ToVec3(a), ToVec3(b), ToVec3(c), ToVec3(d), Vec3::Zero);
    CheckResult(simplexResult.SafeNormalized(), baseResult.SafeNormalized());
    CheckResult(simplex.Size(), expectedSize);
  }

  template <typename Sim, typename Vec, typename Support>
  void TestSimplexTetrahedron(void) {
    //Simplex doesn't check for in front of abc so don't test for that
    Sim simplex;
    Vec a, b, c, d;

    //In front of dba
    a = Vec(1.0f, 0.0f, -2.0f);
    b = Vec(-1.0f, 0.0f, -2.0f);
    c = Vec(-0.2f, 0.3f, -2.0f);
    d = Vec(0.0f, 0.5f, 1.0f);
    PerformTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 3);
    CheckResult(simplex.Contains(a));
    CheckResult(simplex.Contains(b));
    CheckResult(simplex.Contains(d));

    //In front of dcb
    a = Vec(1.28f, -0.37f, -1.16f);
    b = Vec(-0.72f, -0.42f, -1.08f);
    c = Vec(0.147f, -0.072f, -1.08f);
    d = Vec(0.347f, 0.128f, 1.922f);
    PerformTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 3);
    CheckResult(simplex.Contains(b));
    CheckResult(simplex.Contains(c));
    CheckResult(simplex.Contains(d));

    //In front of dac
    a = Vec(0.68f, -0.6f, -1.85f);
    b = Vec(-1.3f, 0.0f, 0.0f);
    c = Vec(-0.45f, -0.3f, -1.77f);
    d = Vec(-0.25f, -0.11f, 1.23f);
    PerformTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 3);
    CheckResult(simplex.Contains(a));
    CheckResult(simplex.Contains(c));
    CheckResult(simplex.Contains(d));

    //In front of da
    a = Vec(-0.188f, -0.33f, -1.163f);
    b = Vec(-2.188f, -0.381f, -1.145f);
    c = Vec(-1.322f, -0.03f, -1.018f);
    d = Vec(-1.122f, 0.17f, 1.922f);
    PerformTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 2);
    CheckResult(simplex.Contains(a));
    CheckResult(simplex.Contains(d));

    //In front of db
    a = Vec(1.935f, -0.276f, -1.163f);
    b = Vec(-0.163f, -0.145f, -1.215f);
    c = Vec(0.8f, 0.024f, -1.02f);
    d = Vec(1.0f, 0.224f, 1.922f);
    PerformTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 2);
    CheckResult(simplex.Contains(b));
    CheckResult(simplex.Contains(d));

    //In front of dc
    a = Vec(1.051f, -0.749f, -1.163f);
    b = Vec(-0.948f, -0.8f, -1.145f);
    c = Vec(-0.083f, -0.449f, -1.018f);
    d = Vec(0.117f, -0.249f, 1.922f);
    PerformTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 2);
    CheckResult(simplex.Contains(c));
    CheckResult(simplex.Contains(d));

    //In front of d
    a = Vec(1.092f, -0.33f, -3.571f);
    b = Vec(-0.907f, -0.381f, -3.553f);
    c = Vec(-0.042f, -0.03f, -3.426f);
    d = Vec(0.158f, 0.17f, -0.486f);
    PerformTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 1);
    CheckResult(simplex.Contains(d));

    //Inside tetrahedron
    a = Vec(1.092f, -0.16f, -0.24f);
    b = Vec(-0.907f, -0.211f, -0.221f);
    c = Vec(-0.042f, 0.14f, -0.094f);
    d = Vec(0.158f, 0.34f, 2.845f);
    PerformTetrahedronTest<Sim, Vec, Support>(a, b, c, d, simplex, 4);
  }

  bool TestSimplex(void) {
    TEST_FAILED = false;
    //TestSimplexLine<SSimplex, SVec3, SSupportPoint>();
    //TestSimplexTriangle<SSimplex, SVec3, SSupportPoint>();
    //TestSimplexTetrahedron<SSimplex, SVec3, SSupportPoint>();

    TestSimplexLine<Simplex, Vec3, SupportPoint>();
    TestSimplexTriangle<Simplex, Vec3, SupportPoint>();
    TestSimplexTetrahedron<Simplex, Vec3, SupportPoint>();
    return TEST_FAILED;
  }

  bool NarrowphaseTest::Run(void) {
    PhysicsSystem system;
    Handle space = system.AddSpace();
    Handle a = system.AddPhysicsObject(true, true, space);
    Handle b = system.AddPhysicsObject(true, true, space);
    Handle sphere = system.GetSphere();
    system.SetObjectModel(space, a, sphere);
    system.SetObjectModel(space, b, sphere);

    Space& s = *system.mSpaces.Get(space);
    PhysicsObject& objA = *s.mObjects.Get(a);
    PhysicsObject& objB = *s.mObjects.Get(b);
    Narrowphase& narrow = system.mSpaces.Get(space)->mNarrowphase;
    narrow.mA = &objA;
    narrow.mB = &objB;
    narrow.mInstA = &objA.GetCollider()->GetModelInstance();
    narrow.mInstB = &objB.GetCollider()->GetModelInstance();
    float diameter = 2.0f;
    system.SetPosition(space, a, Vec3(0.0f));
    int numTests = 100000;

    //Non colliding tests
    for(int i = 0; i < numTests; ++i) {
      float randRadius = RandFloat(diameter + SYX_EPSILON, 1000.0f);
      Vec3 randSphere = RandOnSphere();
      Vec3 randPos = randSphere*randRadius;
      system.SetPosition(space, b, randPos);
      //CheckResult(narrow.SGJK() == false);
      CheckResult(narrow.GJK() == false);
    }

    //Colliding tests
    for(int i = 0; i < numTests; ++i) {
      float randRadius = RandFloat(0.0f, diameter);
      Vec3 randSphere = RandOnSphere();
      Vec3 randPos = randSphere*randRadius;
      system.SetPosition(space, b, randPos);
      //CheckResult(narrow.SGJK() == true);
      CheckResult(narrow.GJK() == true);
    }

    return TEST_FAILED;
  }
#endif
}