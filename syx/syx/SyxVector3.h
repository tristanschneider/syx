#pragma once
#include <cmath>
#include "SyxSIMD.h"

namespace Syx {
  struct Vector3 {
    Vector3(float x_, float y_, float z_): x(x_), y(y_), z(z_), w(0.0f) {}
    explicit Vector3(float splat): x(splat), y(splat), z(splat), w(0.0f) {}
    Vector3(float x_, float y_, float z_, float w_): x(x_), y(y_), z(z_), w(w_) {}
    Vector3(const Vector3& v, float w): x(v.x), y(v.y), z(v.z), w(w) {}
    Vector3(void): x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

    Vector3 operator+(const Vector3& rhs) const;
    Vector3 operator-(const Vector3& rhs) const;
    Vector3 operator-(void) const;
    Vector3 operator*(float rhs) const;
    Vector3 operator/(float rhs) const;
    const float& operator[](int index) const;
    float& operator[](int index);

    Vector3& operator+=(const Vector3& rhs);
    Vector3& operator-=(const Vector3& rhs);
    Vector3& operator*=(float rhs);
    Vector3& operator/=(float rhs);

    bool operator==(const Vector3& rhs) const;
    bool operator!=(const Vector3& rhs) const;

    bool Equal(const Vector3& rhs, float epsilon) const;
    void GetBasis(Vector3& resultA, Vector3& resultB) const;
    Vector3 GetOrthogonal() const;
    float Length(void) const;
    float Length2(void) const;
    float Distance(const Vector3& other) const;
    float Distance2(const Vector3& other) const;
    float Dot(const Vector3& other) const;
    //Dot this is a 4d vector and other as a 3d homogeneous point
    float Dot4(const Vector3& other) const;
    Vector3 Cross(const Vector3& other) const;
    //Returns index of least significant, so x = 0, y = 1, etc.
    int LeastSignificantAxis(void) const;
    int MostSignificantAxis(void) const;
    Vector3 Normalized(void) const;
    Vector3 SafeNormalized(void) const;

    void SafeDivide(float rhs);
    void Scale(const Vector3& rhs);
    void Lerp(const Vector3& end, float t);
    void Abs(void);
    void ProjVec(const Vector3& onto);
    void PointPlaneProj(const Vector3& normal, const Vector3& onPlane);
    void Normalize(void);
    void SafeNormalize(void);
    Vector3 Reciprocal(void) const;
    Vector3 Reciprocal4(void) const;
    Vector3 Mat2Inversed() const;
    Vector3 Mat2Mul(const Vector3& v) const;
    Vector3 Mat2MatMul(const Vector3& mat) const;

    static Vector3 SafeDivide(const Vector3& vec, float div);
    static Vector3 Scale(const Vector3& lhs, const Vector3& rhs);
    static Vector3 Lerp(const Vector3& start, const Vector3& end, float t);
    static Vector3 Abs(const Vector3& in);
    // Finds the distance to the unlimited line
    static float PointLineDistanceSQ(const Vector3& point, const Vector3& start, const Vector3& end);
    static Vector3 CCWTriangleNormal(const Vector3& a, const Vector3& b, const Vector3& c);
    static Vector3 PerpendicularLineToPoint(const Vector3& line, const Vector3& lineToPoint);
    static Vector3 BarycentricToPoint(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& barycentric);
    static Vector3 ProjVec(const Vector3& vec, const Vector3& onto);
    static float ProjVecScalar(const Vector3& vec, const Vector3& onto);
    static Vector3 PointPlaneProj(const Vector3& point, const Vector3& normal, const Vector3& onPlane);
    static float GetScalarT(const Vector3& start, const Vector3& end, const Vector3& pointOnLine);
    static float Length(const Vector3& in);
    static float Length2(const Vector3& in);
    static float Distance(const Vector3& lhs, const Vector3& rhs);
    static float Distance2(const Vector3& lhs, const Vector3& rhs);
    static float Dot(const Vector3& lhs, const Vector3& rhs);
    static float Dot4(const Vector3& v4, const Vector3& v3);
    static Vector3 Cross(const Vector3& lhs, const Vector3& rhs);
    static int LeastSignificantAxis(const Vector3& in);
    static int MostSignificantAxis(const Vector3& in);

    SAlign const static Vector3 UnitX;
    SAlign const static Vector3 UnitY;
    SAlign const static Vector3 UnitZ;
    SAlign const static Vector3 Zero;
    SAlign const static Vector3 Identity;

    float x;
    float y;
    float z;
    //Padding so this can loaded into SVector3 without needing to copy to a 4 float buffer first
    float w;
  };

  Vector3 operator*(float lhs, const Vector3& rhs);
  Vector3& operator*=(float lhs, Vector3& rhs);

  struct Vector3Hash {
    size_t operator()(const Vector3& val);
  };

  typedef Vector3 Vec3;
}