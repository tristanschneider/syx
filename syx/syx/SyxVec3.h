#pragma once

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

    bool equal(const Vec3& rhs, float epsilon) const;
    void getBasis(Vec3& resultA, Vec3& resultB) const;
    Vec3 getOrthogonal() const;
    float length(void) const;
    float length2(void) const;
    float distance(const Vec3& other) const;
    float distance2(const Vec3& other) const;
    float dot(const Vec3& other) const;
    //Dot this is a 4d vector and other as a 3d homogeneous point
    float dot4(const Vec3& other) const;
    Vec3 cross(const Vec3& other) const;
    //Returns index of least significant, so x = 0, y = 1, etc.
    int leastSignificantAxis(void) const;
    int mostSignificantAxis(void) const;
    Vec3 normalized(void) const;
    Vec3 safeNormalized(void) const;

    void safeDivide(float rhs);
    void scale(const Vec3& rhs);
    void lerp(const Vec3& end, float t);
    void abs(void);
    void projVec(const Vec3& onto);
    void pointPlaneProj(const Vec3& normal, const Vec3& onPlane);
    void normalize(void);
    void safeNormalize(void);
    Vec3 reciprocal(void) const;
    Vec3 reciprocal4(void) const;
    Vec3 mat2Inversed() const;
    Vec3 mat2Mul(const Vec3& v) const;
    Vec3 mat2MatMul(const Vec3& mat) const;

    static Vec3 safeDivide(const Vec3& vec, float div);
    static Vec3 scale(const Vec3& lhs, const Vec3& rhs);
    static Vec3 lerp(const Vec3& start, const Vec3& end, float t);
    static Vec3 abs(const Vec3& in);
    // Finds the distance to the unlimited line
    static float pointLineDistanceSQ(const Vec3& point, const Vec3& start, const Vec3& end);
    static Vec3 ccwTriangleNormal(const Vec3& a, const Vec3& b, const Vec3& c);
    static Vec3 perpendicularLineToPoint(const Vec3& line, const Vec3& lineToPoint);
    static Vec3 barycentricToPoint(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& barycentric);
    static Vec3 projVec(const Vec3& vec, const Vec3& onto);
    static float projVecScalar(const Vec3& vec, const Vec3& onto);
    static Vec3 pointPlaneProj(const Vec3& point, const Vec3& normal, const Vec3& onPlane);
    static float getScalarT(const Vec3& start, const Vec3& end, const Vec3& pointOnLine);
    static float length(const Vec3& in);
    static float length2(const Vec3& in);
    static float distance(const Vec3& lhs, const Vec3& rhs);
    static float distance2(const Vec3& lhs, const Vec3& rhs);
    static float dot(const Vec3& lhs, const Vec3& rhs);
    static float dot4(const Vec3& v4, const Vec3& v3);
    static Vec3 cross(const Vec3& lhs, const Vec3& rhs);
    static int leastSignificantAxis(const Vec3& in);
    static int mostSignificantAxis(const Vec3& in);

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