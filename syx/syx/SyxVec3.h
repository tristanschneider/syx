#pragma once
#include <cmath>
#include "SyxSIMD.h"

namespace Syx {
  struct Vec3 {
    Vec3(float x_, float y_, float z_): x(x_), y(y_), z(z_), w(0.0f) {}
    explicit Vec3(float splat): x(splat), y(splat), z(splat), w(0.0f) {}
    Vec3(float x_, float y_, float z_, float w_): x(x_), y(y_), z(z_), w(w_) {}
    Vec3(const Vec3& v, float w): x(v.x), y(v.y), z(v.z), w(w) {}
    Vec3(void): x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

    Vec3 operator+(const Vec3& rhs) const;
    Vec3 operator-(const Vec3& rhs) const;
    Vec3 operator-(void) const;
    Vec3 operator*(float rhs) const;
    Vec3 operator/(float rhs) const;
    const float& operator[](int index) const;
    float& operator[](int index);

    Vec3& operator+=(const Vec3& rhs);
    Vec3& operator-=(const Vec3& rhs);
    Vec3& operator*=(float rhs);
    Vec3& operator/=(float rhs);

    bool operator==(const Vec3& rhs) const;
    bool operator!=(const Vec3& rhs) const;

    bool Equal(const Vec3& rhs, float epsilon) const;
    void GetBasis(Vec3& resultA, Vec3& resultB) const;
    Vec3 GetOrthogonal() const;
    float Length(void) const;
    float Length2(void) const;
    float Distance(const Vec3& other) const;
    float Distance2(const Vec3& other) const;
    float Dot(const Vec3& other) const;
    //Dot this is a 4d vector and other as a 3d homogeneous point
    float Dot4(const Vec3& other) const;
    Vec3 Cross(const Vec3& other) const;
    //Returns index of least significant, so x = 0, y = 1, etc.
    int LeastSignificantAxis(void) const;
    int MostSignificantAxis(void) const;
    Vec3 Normalized(void) const;
    Vec3 SafeNormalized(void) const;

    void SafeDivide(float rhs);
    void Scale(const Vec3& rhs);
    void Lerp(const Vec3& end, float t);
    void Abs(void);
    void ProjVec(const Vec3& onto);
    void PointPlaneProj(const Vec3& normal, const Vec3& onPlane);
    void Normalize(void);
    void SafeNormalize(void);
    Vec3 Reciprocal(void) const;
    Vec3 Reciprocal4(void) const;
    Vec3 Mat2Inversed() const;
    Vec3 Mat2Mul(const Vec3& v) const;
    Vec3 Mat2MatMul(const Vec3& mat) const;

    static Vec3 SafeDivide(const Vec3& vec, float div);
    static Vec3 Scale(const Vec3& lhs, const Vec3& rhs);
    static Vec3 Lerp(const Vec3& start, const Vec3& end, float t);
    static Vec3 Abs(const Vec3& in);
    // Finds the distance to the unlimited line
    static float PointLineDistanceSQ(const Vec3& point, const Vec3& start, const Vec3& end);
    static Vec3 CCWTriangleNormal(const Vec3& a, const Vec3& b, const Vec3& c);
    static Vec3 PerpendicularLineToPoint(const Vec3& line, const Vec3& lineToPoint);
    static Vec3 BarycentricToPoint(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& barycentric);
    static Vec3 ProjVec(const Vec3& vec, const Vec3& onto);
    static float ProjVecScalar(const Vec3& vec, const Vec3& onto);
    static Vec3 PointPlaneProj(const Vec3& point, const Vec3& normal, const Vec3& onPlane);
    static float GetScalarT(const Vec3& start, const Vec3& end, const Vec3& pointOnLine);
    static float Length(const Vec3& in);
    static float Length2(const Vec3& in);
    static float Distance(const Vec3& lhs, const Vec3& rhs);
    static float Distance2(const Vec3& lhs, const Vec3& rhs);
    static float Dot(const Vec3& lhs, const Vec3& rhs);
    static float Dot4(const Vec3& v4, const Vec3& v3);
    static Vec3 Cross(const Vec3& lhs, const Vec3& rhs);
    static int LeastSignificantAxis(const Vec3& in);
    static int MostSignificantAxis(const Vec3& in);

    SAlign const static Vec3 UnitX;
    SAlign const static Vec3 UnitY;
    SAlign const static Vec3 UnitZ;
    SAlign const static Vec3 Zero;
    SAlign const static Vec3 Identity;

    float x;
    float y;
    float z;
    //Padding so this can loaded into SVec3 without needing to copy to a 4 float buffer first
    float w;
  };

  Vec3 operator*(float lhs, const Vec3& rhs);
  Vec3& operator*=(float lhs, Vec3& rhs);

  struct Vec3Hash {
    size_t operator()(const Vec3& val);
  };

  typedef Vec3 Vec3;
}