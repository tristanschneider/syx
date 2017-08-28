#include "Precompile.h"
#include "SyxMathTests.h"
#include "SyxTestHelpers.h"

namespace Syx
{
  bool TEST_FAILED = false;

  bool testAllMath(void)
  {
    TEST_FAILED = false;
    testSVec3();
    if(TEST_FAILED) return TEST_FAILED;
    testSMat3();
    if(TEST_FAILED) return TEST_FAILED;
    testSQuat();
    if(TEST_FAILED) return TEST_FAILED;
    testVector2();
    if(TEST_FAILED) return TEST_FAILED;
    testMatrix2();
    return TEST_FAILED;
  }

#ifndef SENABLED
  bool TestSFloats(void) { return false; }
  bool testSMat3(void) { return false; }
  bool testSQuat(void) { return false; }
  bool testVector2(void) { return false; }
  bool testMatrix2(void) { return false; }
#else
  bool testSVec3(void)
  {
    TEST_FAILED = false;
    Vec3 va(1.0f, 2.0f, 3.0f);
    Vec3 vb(2.0f, -2.0f, 1.0f);
    SFloats sa = sLoadFloats(1.0f, 2.0f, 3.0f);
    SFloats sb = sLoadFloats(2.0f, -2.0f, 1.0f);
    SFloats s4 = sLoadFloats(1.0f, 2.0f, 3.0f, 4.0f);

    checkResult(sa, va);
    checkResult(sLoadSplatFloats(1.0f), Vec3(1.0f));
    checkResult(SVec3::add(sa, sb), va + vb);
    checkResult(SVec3::sub(sa, sb), va - vb);
    checkResult(SVec3::neg(sa), -va);
    checkResult(SVec3::mul(sa, sLoadSplatFloats(2.0f)), va*2.0f);
    checkResult(SVec3::div(sa, sLoadSplatFloats(2.0f)), va/2.0f);
    checkResult(SAnd(SVec3::equal(sLoadSplatFloats(2.0f), sLoadSplatFloats(3.0f)), SVec3::Identity), Vec3(0.0f));
    checkResult(SAnd(SVec3::equal(sLoadSplatFloats(2.0f), sLoadSplatFloats(2.0f)), SVec3::Identity), Vec3(1.0f));
    checkResult(SAnd(SVec3::notEqual(sLoadSplatFloats(2.0f), sLoadSplatFloats(3.0f)), SVec3::Identity), Vec3(1.0f));
    checkResult(SAnd(SVec3::notEqual(sLoadSplatFloats(2.0f), sLoadSplatFloats(2.0f)), SVec3::Identity), Vec3(0.0f));
    checkResult(SAnd(SVec3::equal(sa, sLoadFloats(1.0f, 2.0f, 4.0f)), SVec3::Identity), Vec3(0.0f));
    checkResult(SVec3::length(sa), va.length());
    checkResult(SVec3::length2(sa), va.length2());
    checkResult(SVec3::distance(sa, sb), va.distance(vb));
    checkResult(SVec3::distance2(sa, sb), va.distance2(vb));
    checkResult(SVec3::dot(sa, sb), va.dot(vb));
    checkResult(SVec3::dot4(s4, s4), Vec3(30.0f));
    checkResult(SVec3::cross(sa, sb), va.cross(vb));
    checkResult(SAnd(SVec3::leastSignificantAxis(sa), SVec3::Identity), Vec3(1.0f, 0.0f, 0.0f));
    checkResult(SAnd(SVec3::mostSignificantAxis(sa), SVec3::Identity), Vec3(0.0f, 0.0f, 1.0f));
    checkResult(SVec3::normalized(sa), va.normalized());
    checkResult(SVec3::safeNormalized(SVec3::Zero), Vec3::Zero.safeNormalized());
    checkResult(sa*sb, Vec3::scale(va, vb));
    return TEST_FAILED;
  }

  bool testSMat3(void)
  {
    TEST_FAILED = false;
    float m[9] = { 1.0f, 2.0f, 3.0f,
                  -1.0f, 3.0f, 5.0f,
                   0.5f, -2.0f, 6.0f };
    float r[9] = { 0.612361f, 0.729383f, 0.198896f,
                   0.779353f, 0.334213f, 0.569723f,
                   0.176786f, 0.126579f, 0.15549f };
    float x = 2.0f;
    float y = 5.5f;
    float z = 0.7f;

    Vec3 v(x, y, z);
    SFloats sv = sLoadFloats(x, y, z);

    Mat3 ma(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
    SMat3 sa(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
    Mat3 mb(r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]);
    SMat3 sb(r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]);

    checkResult(sa, ma);
    checkResult(sa.transposed(), ma.transposed());
    checkResult(sa*sb, ma*mb);
    checkResult(sa+sb, ma+mb);
    checkResult(sa-sb, ma-mb);
    checkResult(sa.transposedMultiply(sb), ma.transposedMultiply(mb));
    checkResult(sa*sv, ma*v);
    checkResult(sa.transposedMultiply(sv), ma.transposedMultiply(v));
    checkResult(sb.determinant(), mb.determinant());
    checkResult(sb.inverse(sb.determinant()), mb.inverse(mb.determinant()));
    checkResult(sb.inverse(), mb.inverse());
    checkResult(sa.toQuat(), ma.toQuat());
    return TEST_FAILED;
  }

  bool testSQuat(void)
  {
    TEST_FAILED = false;
    SAlign Vec3 axisA(1.0f, 2.0f, 0.5f);
    axisA.normalize();
    float angleA = 1.0f;
    SFloats sqA = SQuat::axisAngle(sLoadFloats(axisA.x, axisA.y, axisA.z), angleA);
    SAlign Quat qA = Quat::axisAngle(axisA, angleA);

    SAlign Vec3 axisB(0.5f, 2.5f, 1.5f);
    axisB.normalize();
    float angleB = 0.343f;
    SFloats sqB = SQuat::axisAngle(sLoadFloats(axisB.x, axisB.y, axisB.z), angleB);
    SAlign Quat qB = Quat::axisAngle(axisB, angleB);

    checkResult(sqA, qA);
    checkResult(SQuat::neg(sqA), -qA);
    checkResult(SQuat::add(sqA, sqB), qA + qB);
    checkResult(SQuat::div(sqA, sLoadSplatFloats(angleB)), qA/angleB);
    checkResult(SQuat::mulQuatVec(sqA, sLoadSplatFloats(angleB)), qA*angleB);
    checkResult(SQuat::length2(sqA), Vec3(qA.length2()));
    checkResult(SQuat::length(sqA), Vec3(qA.length()));
    checkResult(SQuat::normalized(sqA), qA.normalized());
    checkResult(SQuat::mulQuat(sqA, sqB), qA*qB);
    checkResult(SQuat::rotate(sqA, toSVec3(axisB)), qA*axisB);
    checkResult(SQuat::toMatrix(sqA), qA.toMatrix());
    return TEST_FAILED;
  }

  bool testVector2(void)
  {
    Vector2 x = Vector2::sUnitX;
    Vector2 y = Vector2::sUnitY;
    Vector2 i = Vector2::sIdentity;
    Vector2 z = Vector2::sZero;
    Vector2 a(1.4f, -8.1f);
    Vector2 b(-4.6f, 13.9f);

    checkResult((x == x));
    checkResult(!(x == y));
    checkResult(x != y);
    checkResult(x + y, i);
    checkResult(i - x - y, z);
    checkResult(3.0f*(x + 2.0f*y), Vector2(3.0f, 6.0f));
    checkResult(abs(x.dot(y)) < SYX_EPSILON);
    checkResult(abs(x.cross(y) - 1.0f) < SYX_EPSILON);
    checkResult(abs(y.cross(x) + 1.0f) < SYX_EPSILON);
    checkResult(a.lerp(b, 0.0f) == a);
    checkResult(a.lerp(b, 1.0f) == b);
    checkResult(a.lerp(b, 0.5f) == (a + b)/2.0f);
    checkResult(x.slerp(y, 1.0f) == y);
    checkResult(x.slerp(y, 0.0f) == x);
    checkResult(x.slerp(y, 0.5f) == Vector2(1.0f, 1.0f).normalized());
    //Ambiguous case, but producing an orthogonal vector instead of zero is what we want
    checkResult(abs((x.slerp(-x, 0.5f)).cross(x)) - 1.0f < SYX_EPSILON);
    checkResult(x.slerp(x, 0.5f) == x);
    //Use bigger epsilon because all the trig and normalization adds up to more error than SYX_EPSILON
    checkResult(Vector2(-1, 1).normalized().slerp(Vector2(-1, -1).normalized(), 0.5f).equal(-x, 0.01f));
    checkResult(Vector2(2.0f, 7.0f).proj(3.0f*x) == 2.0f*x);
    checkResult(y.rotate(SYX_PI_4) == Vector2(-1.0f, 1.0f).normalized());
    return TEST_FAILED;
  }

  bool testMatrix2(void)
  {
    Matrix2 rot45 = Matrix2::rotate(SYX_PI_4);
    Vector2 x = Vector2::sUnitX;
    Vector2 y = Vector2::sUnitY;

    checkResult(Matrix2::sIdentity + Matrix2::sIdentity == 2.0f*Matrix2::sIdentity);
    checkResult(rot45*rot45 == Matrix2::rotate(SYX_PI_2));
    checkResult(rot45.transposedMultiply(rot45) == Matrix2::sIdentity);
    checkResult(rot45*rot45.transposed() == Matrix2::sIdentity);
    checkResult(Matrix2::rotationFromUp(Vector2::sUnitY) == Matrix2::sIdentity);
    checkResult(Matrix2::rotationFromRight(Vector2::sUnitX) == Matrix2::sIdentity);
    checkResult(rot45*x == Vector2(1.0f, 1.0f).normalized());
    checkResult(rot45.transposedMultiply(x) == Vector2(1.0f, -1.0f).normalized());
    checkResult(Matrix2::rotate(x, y)*x == y);
    return TEST_FAILED;
  }
#endif
}