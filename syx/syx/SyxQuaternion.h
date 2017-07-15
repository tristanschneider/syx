#pragma once
#include "SyxVector3.h"

namespace Syx {
  struct Vector3;
  struct Matrix3;

  struct Quat {
    Quat(const Vector3& ijk, float w);
    Quat(float i, float j, float k, float w);
    Quat(void) {};

    Quat operator*(const Quat& rhs) const;
    Quat& operator*=(const Quat& rhs);
    Quat operator*(float rhs) const;
    Quat& operator*=(float rhs);
    Vector3 operator*(const Vector3& rhs) const;
    Quat operator+(const Quat& rhs) const;
    Quat& operator+=(const Quat& rhs);
    Quat operator/(float rhs) const;
    Quat& operator/=(float rhs);
    Quat operator-(void) const;

    Vector3 GetUp() const;
    Vector3 GetRight() const;
    Vector3 GetForward() const;
    //Normalized quaternion is like axis angle. This gets said angle
    float GetAngle() const;
    Vec3 GetAxis() const;

    float Length() const;
    float Length2() const;

    Quat Normalized() const;
    void Normalize();

    Quat Inversed() const;
    void Inverse();

    Matrix3 ToMatrix() const;

    static Quat AxisAngle(const Vector3& axis, float angle);
    //Assumes normalized input
    static Quat LookAt(const Vector3& axis);
    //Assumes orthonormal inputs
    static Quat LookAt(const Vector3& axis, const Vector3& up);
    static Quat LookAt(const Vector3& forward, const Vector3& up, const Vector3& right);
    static Quat GetRotation(const Vec3& from, const Vec3& to);

    static const Quat Zero;
    static const Quat Identity;

    Vector3 mV;
  };

  Quat operator*(float lhs, const Quat& rhs);
}