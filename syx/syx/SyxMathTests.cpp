#include "Precompile.h"
#include "SyxMathTests.h"
#include "SyxTestHelpers.h"

namespace Syx
{
  bool TEST_FAILED = false;

  bool TestAllMath(void)
  {
    TEST_FAILED = false;
    TestSVec3();
    if(TEST_FAILED) return TEST_FAILED;
    TestSMat3();
    if(TEST_FAILED) return TEST_FAILED;
    TestSQuat();
    if(TEST_FAILED) return TEST_FAILED;
    TestVector2();
    if(TEST_FAILED) return TEST_FAILED;
    TestMatrix2();
    return TEST_FAILED;
  }

#ifndef SENABLED
  bool TestSFloats(void) { return false; }
  bool TestSMat3(void) { return false; }
  bool TestSQuat(void) { return false; }
  bool TestVector2(void) { return false; }
  bool TestMatrix2(void) { return false; }
#else
  bool TestSVec3(void)
  {
    TEST_FAILED = false;
    Vec3 va(1.0f, 2.0f, 3.0f);
    Vec3 vb(2.0f, -2.0f, 1.0f);
    SFloats sa = SLoadFloats(1.0f, 2.0f, 3.0f);
    SFloats sb = SLoadFloats(2.0f, -2.0f, 1.0f);
    SFloats s4 = SLoadFloats(1.0f, 2.0f, 3.0f, 4.0f);

    CheckResult(sa, va);
    CheckResult(SLoadSplatFloats(1.0f), Vec3(1.0f));
    CheckResult(SVec3::Add(sa, sb), va + vb);
    CheckResult(SVec3::Sub(sa, sb), va - vb);
    CheckResult(SVec3::Neg(sa), -va);
    CheckResult(SVec3::Mul(sa, SLoadSplatFloats(2.0f)), va*2.0f);
    CheckResult(SVec3::Div(sa, SLoadSplatFloats(2.0f)), va/2.0f);
    CheckResult(SAnd(SVec3::Equal(SLoadSplatFloats(2.0f), SLoadSplatFloats(3.0f)), SVec3::Identity), Vec3(0.0f));
    CheckResult(SAnd(SVec3::Equal(SLoadSplatFloats(2.0f), SLoadSplatFloats(2.0f)), SVec3::Identity), Vec3(1.0f));
    CheckResult(SAnd(SVec3::NotEqual(SLoadSplatFloats(2.0f), SLoadSplatFloats(3.0f)), SVec3::Identity), Vec3(1.0f));
    CheckResult(SAnd(SVec3::NotEqual(SLoadSplatFloats(2.0f), SLoadSplatFloats(2.0f)), SVec3::Identity), Vec3(0.0f));
    CheckResult(SAnd(SVec3::Equal(sa, SLoadFloats(1.0f, 2.0f, 4.0f)), SVec3::Identity), Vec3(0.0f));
    CheckResult(SVec3::Length(sa), va.Length());
    CheckResult(SVec3::Length2(sa), va.Length2());
    CheckResult(SVec3::Distance(sa, sb), va.Distance(vb));
    CheckResult(SVec3::Distance2(sa, sb), va.Distance2(vb));
    CheckResult(SVec3::Dot(sa, sb), va.Dot(vb));
    CheckResult(SVec3::Dot4(s4, s4), Vec3(30.0f));
    CheckResult(SVec3::Cross(sa, sb), va.Cross(vb));
    CheckResult(SAnd(SVec3::LeastSignificantAxis(sa), SVec3::Identity), Vec3(1.0f, 0.0f, 0.0f));
    CheckResult(SAnd(SVec3::MostSignificantAxis(sa), SVec3::Identity), Vec3(0.0f, 0.0f, 1.0f));
    CheckResult(SVec3::Normalized(sa), va.Normalized());
    CheckResult(SVec3::SafeNormalized(SVec3::Zero), Vec3::Zero.SafeNormalized());
    CheckResult(sa*sb, Vec3::Scale(va, vb));
    return TEST_FAILED;
  }

  bool TestSMat3(void)
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
    SFloats sv = SLoadFloats(x, y, z);

    Mat3 ma(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
    SMat3 sa(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
    Mat3 mb(r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]);
    SMat3 sb(r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]);

    CheckResult(sa, ma);
    CheckResult(sa.Transposed(), ma.Transposed());
    CheckResult(sa*sb, ma*mb);
    CheckResult(sa+sb, ma+mb);
    CheckResult(sa-sb, ma-mb);
    CheckResult(sa.TransposedMultiply(sb), ma.TransposedMultiply(mb));
    CheckResult(sa*sv, ma*v);
    CheckResult(sa.TransposedMultiply(sv), ma.TransposedMultiply(v));
    CheckResult(sb.Determinant(), mb.Determinant());
    CheckResult(sb.Inverse(sb.Determinant()), mb.Inverse(mb.Determinant()));
    CheckResult(sb.Inverse(), mb.Inverse());
    CheckResult(sa.ToQuat(), ma.ToQuat());
    return TEST_FAILED;
  }

  bool TestSQuat(void)
  {
    TEST_FAILED = false;
    SAlign Vec3 axisA(1.0f, 2.0f, 0.5f);
    axisA.Normalize();
    float angleA = 1.0f;
    SFloats sqA = SQuat::AxisAngle(SLoadFloats(axisA.x, axisA.y, axisA.z), angleA);
    SAlign Quat qA = Quat::AxisAngle(axisA, angleA);

    SAlign Vec3 axisB(0.5f, 2.5f, 1.5f);
    axisB.Normalize();
    float angleB = 0.343f;
    SFloats sqB = SQuat::AxisAngle(SLoadFloats(axisB.x, axisB.y, axisB.z), angleB);
    SAlign Quat qB = Quat::AxisAngle(axisB, angleB);

    CheckResult(sqA, qA);
    CheckResult(SQuat::Neg(sqA), -qA);
    CheckResult(SQuat::Add(sqA, sqB), qA + qB);
    CheckResult(SQuat::Div(sqA, SLoadSplatFloats(angleB)), qA/angleB);
    CheckResult(SQuat::MulQuatVec(sqA, SLoadSplatFloats(angleB)), qA*angleB);
    CheckResult(SQuat::Length2(sqA), Vec3(qA.Length2()));
    CheckResult(SQuat::Length(sqA), Vec3(qA.Length()));
    CheckResult(SQuat::Normalized(sqA), qA.Normalized());
    CheckResult(SQuat::MulQuat(sqA, sqB), qA*qB);
    CheckResult(SQuat::Rotate(sqA, ToSVec3(axisB)), qA*axisB);
    CheckResult(SQuat::ToMatrix(sqA), qA.ToMatrix());
    return TEST_FAILED;
  }

  bool TestVector2(void)
  {
    Vector2 x = Vector2::sUnitX;
    Vector2 y = Vector2::sUnitY;
    Vector2 i = Vector2::sIdentity;
    Vector2 z = Vector2::sZero;
    Vector2 a(1.4f, -8.1f);
    Vector2 b(-4.6f, 13.9f);

    CheckResult((x == x));
    CheckResult(!(x == y));
    CheckResult(x != y);
    CheckResult(x + y, i);
    CheckResult(i - x - y, z);
    CheckResult(3.0f*(x + 2.0f*y), Vector2(3.0f, 6.0f));
    CheckResult(abs(x.Dot(y)) < SYX_EPSILON);
    CheckResult(abs(x.Cross(y) - 1.0f) < SYX_EPSILON);
    CheckResult(abs(y.Cross(x) + 1.0f) < SYX_EPSILON);
    CheckResult(a.Lerp(b, 0.0f) == a);
    CheckResult(a.Lerp(b, 1.0f) == b);
    CheckResult(a.Lerp(b, 0.5f) == (a + b)/2.0f);
    CheckResult(x.Slerp(y, 1.0f) == y);
    CheckResult(x.Slerp(y, 0.0f) == x);
    CheckResult(x.Slerp(y, 0.5f) == Vector2(1.0f, 1.0f).Normalized());
    //Ambiguous case, but producing an orthogonal vector instead of zero is what we want
    CheckResult(abs((x.Slerp(-x, 0.5f)).Cross(x)) - 1.0f < SYX_EPSILON);
    CheckResult(x.Slerp(x, 0.5f) == x);
    //Use bigger epsilon because all the trig and normalization adds up to more error than SYX_EPSILON
    CheckResult(Vector2(-1, 1).Normalized().Slerp(Vector2(-1, -1).Normalized(), 0.5f).Equal(-x, 0.01f));
    CheckResult(Vector2(2.0f, 7.0f).Proj(3.0f*x) == 2.0f*x);
    CheckResult(y.Rotate(SYX_PI_4) == Vector2(-1.0f, 1.0f).Normalized());
    return TEST_FAILED;
  }

  bool TestMatrix2(void)
  {
    Matrix2 rot45 = Matrix2::Rotate(SYX_PI_4);
    Vector2 x = Vector2::sUnitX;
    Vector2 y = Vector2::sUnitY;

    CheckResult(Matrix2::sIdentity + Matrix2::sIdentity == 2.0f*Matrix2::sIdentity);
    CheckResult(rot45*rot45 == Matrix2::Rotate(SYX_PI_2));
    CheckResult(rot45.TransposedMultiply(rot45) == Matrix2::sIdentity);
    CheckResult(rot45*rot45.Transposed() == Matrix2::sIdentity);
    CheckResult(Matrix2::RotationFromUp(Vector2::sUnitY) == Matrix2::sIdentity);
    CheckResult(Matrix2::RotationFromRight(Vector2::sUnitX) == Matrix2::sIdentity);
    CheckResult(rot45*x == Vector2(1.0f, 1.0f).Normalized());
    CheckResult(rot45.TransposedMultiply(x) == Vector2(1.0f, -1.0f).Normalized());
    CheckResult(Matrix2::Rotate(x, y)*x == y);
    return TEST_FAILED;
  }
#endif
}