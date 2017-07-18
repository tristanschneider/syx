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

    Vec3 GetUp() const;
    Vec3 GetRight() const;
    Vec3 GetForward() const;
    //Normalized quaternion is like axis angle. This gets said angle
    float GetAngle() const;
    Vec3 GetAxis() const;

    float Length() const;
    float Length2() const;

    Quat Normalized() const;
    void Normalize();

    Quat Inversed() const;
    void Inverse();

    Mat3 ToMatrix() const;

    static Quat AxisAngle(const Vec3& axis, float angle);
    //Assumes normalized input
    static Quat LookAt(const Vec3& axis);
    //Assumes orthonormal inputs
    static Quat LookAt(const Vec3& axis, const Vec3& up);
    static Quat LookAt(const Vec3& forward, const Vec3& up, const Vec3& right);
    static Quat GetRotation(const Vec3& from, const Vec3& to);

    static const Quat Zero;
    static const Quat Identity;

    Vec3 mV;
  };

  Quat operator*(float lhs, const Quat& rhs);
}