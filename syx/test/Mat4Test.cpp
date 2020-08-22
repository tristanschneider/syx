#include "Precompile.h"
#include "CppUnitTest.h"

#include "SyxVec3.h"
#include "SyxMat4.h"
#include "SyxMat3.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Syx;

namespace MathTests {
  bool _equal(const Mat3& lhs, const Mat3& rhs) {
    const float epsilon = 0.0001f;
    return lhs.mbx.equal(rhs.mbx, epsilon)
      && lhs.mby.equal(rhs.mby, epsilon)
      && lhs.mbz.equal(rhs.mbz, epsilon);
  }

  TEST_CLASS(Mat4Tests) {
    static constexpr float EPSILON = 0.0001f;

  public:
    TEST_METHOD(Mat4_Construct_ColumnsMatch) {
      const Mat4 m(0, 1, 2, 3,
            4, 5, 6, 7,
            8, 9, 10, 11,
            12, 13, 14, 15);
      const Vec3 c1 = m.getCol(0);
      const Vec3 c2 = m.getCol(1);
      const Vec3 c3 = m.getCol(2);
      const Vec3 c4 = m.getCol(3);

      Assert::IsTrue(c1.equal(Vec3(0, 4, 8, 12), EPSILON), L"First column should match", LINE_INFO());
      Assert::IsTrue(c2.equal(Vec3(1, 5, 9, 13), EPSILON), L"Second column should match", LINE_INFO());
      Assert::IsTrue(c3.equal(Vec3(2, 6, 10, 14), EPSILON), L"Thid column should match", LINE_INFO());
      Assert::IsTrue(c4.equal(Vec3(3, 7, 11, 15), EPSILON), L"Fourth column should match", LINE_INFO());
    }

    TEST_METHOD(Mat4_Construct_SingleIndexMatches) {
      const Mat4 m(0, 4,  8, 12,
                   1, 5,  9, 13,
                   2, 6, 10, 14,
                   3, 7, 11, 15);

      for(int i = 0; i < 16; ++i) {
        Assert::AreEqual(m.mData[i], static_cast<float>(i), EPSILON, L"Value should index across rows", LINE_INFO());
      }
    }

    TEST_METHOD(Mat4_Construct_DoubleIndexMatches) {
      const Mat4 m(0, 1, 2, 3,
            4, 5, 6, 7,
            8, 9, 10, 11,
            12, 13, 14, 15);

      const float e = 0.0001f;
      Assert::AreEqual(m[0][0], 0.f, EPSILON);
      Assert::AreEqual(m[1][0], 1.f, EPSILON);
      Assert::AreEqual(m[2][0], 2.f, EPSILON);
      Assert::AreEqual(m[3][0], 3.f, EPSILON);

      Assert::AreEqual(m[0][1], 4.f, EPSILON);
      Assert::AreEqual(m[1][1], 5.f, EPSILON);
      Assert::AreEqual(m[2][1], 6.f, EPSILON);
      Assert::AreEqual(m[3][1], 7.f, EPSILON);

      Assert::AreEqual(m[0][2], 8.f, EPSILON);
      Assert::AreEqual(m[1][2], 9.f, EPSILON);
      Assert::AreEqual(m[2][2], 10.f, EPSILON);
      Assert::AreEqual(m[3][2], 11.f, EPSILON);

      Assert::AreEqual(m[0][3], 12.f, EPSILON);
      Assert::AreEqual(m[1][3], 13.f, EPSILON);
      Assert::AreEqual(m[2][3], 14.f, EPSILON);
      Assert::AreEqual(m[3][3], 15.f, EPSILON);
    }

    TEST_METHOD(Mat4_CreateTransformDecompose2Mat_MatchesOriginal) {
      const Vec3 translate(1, 2, 3);
      const Mat3 rotate(Mat3::axisAngle(Vec3::UnitY, 0.5f));
      const Mat4 transform = Mat4::transform(rotate, translate);

      Vec3 decomposedTranslate;
      Mat3 decomposedRotate;
      transform.decompose(decomposedRotate, decomposedTranslate);

      Assert::IsTrue(translate.equal(decomposedTranslate, EPSILON), L"Translate should match", LINE_INFO());
      Assert::IsTrue(_equal(rotate, decomposedRotate), L"Rotation should match", LINE_INFO());
    }

    TEST_METHOD(Mat4_CreateTransformDecompose3Mat_MatchesOriginal) {
      const Vec3 translate(1, 2, 3);
      const Vec3 scale(4, 5, 6);
      const Mat3 rotate(Mat3::axisAngle(Vec3::UnitY, 0.5f));
      const Mat4 transform = Mat4::transform(scale, rotate, translate);

      Vec3 decomposedTranslate, decomposedScale;
      Mat3 decomposedRotate;
      transform.decompose(decomposedScale, decomposedRotate, decomposedTranslate);

      Assert::IsTrue(translate.equal(decomposedTranslate, EPSILON), L"Translate should match", LINE_INFO());
      Assert::IsTrue(scale.equal(decomposedScale, EPSILON), L"Scale should match", LINE_INFO());
      Assert::IsTrue(_equal(rotate, decomposedRotate), L"Rotation should match", LINE_INFO());
    }
  };
}