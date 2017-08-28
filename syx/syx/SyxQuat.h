#pragma once

namespace Syx {
  struct Vec3;
  struct Mat3;

  struct Quat {
    Quat(const Vec3& ijk, float w);
    Quat(float i, float j, float k, float w);
    Quat(void) {};

    Quat operator*(const Quat& rhs) const;
    Quat& operator*=(const Quat& rhs);
    Quat operator*(float rhs) const;
    Quat& operator*=(float rhs);
    Vec3 operator*(const Vec3& rhs) const;
    Quat operator+(const Quat& rhs) const;
    Quat& operator+=(const Quat& rhs);
    Quat operator/(float rhs) const;
    Quat& operator/=(float rhs);
    Quat operator-(void) const;

    Vec3 getUp() const;
    Vec3 getRight() const;
    Vec3 getForward() const;
    //Normalized quaternion is like axis angle. This gets said angle
    float getAngle() const;
    Vec3 getAxis() const;

    float length() const;
    float length2() const;

    Quat normalized() const;
    void normalize();

    Quat inversed() const;
    void inverse();

    Mat3 toMatrix() const;

    static Quat axisAngle(const Vec3& axis, float angle);
    //Assumes normalized input
    static Quat lookAt(const Vec3& axis);
    //Assumes orthonormal inputs
    static Quat lookAt(const Vec3& axis, const Vec3& up);
    static Quat lookAt(const Vec3& forward, const Vec3& up, const Vec3& right);
    static Quat getRotation(const Vec3& from, const Vec3& to);

    static const Quat Zero;
    static const Quat Identity;

    Vec3 mV;
  };

  Quat operator*(float lhs, const Quat& rhs);
}