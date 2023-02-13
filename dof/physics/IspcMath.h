#pragma once
#include "MacrosIspc.h"

float vec2Length(FLOAT2 v) {
  return sqrt(v.x*v.x + v.y*v.y);
}

FLOAT2 vec2Negate(FLOAT2 v) {
  FLOAT2 result = { -v.x, -v.y };
  return result;
}

FLOAT2 vec2Sub(FLOAT2 l, FLOAT2 r) {
  FLOAT2 result = { l.x - r.x, l.y - r.y };
  return result;
}

FLOAT2 vec2Add(FLOAT2 l, FLOAT2 r) {
  FLOAT2 result = { l.x + r.x, l.y + r.y };
  return result;
}

FLOAT2 vec2Multiply(FLOAT2 v, float scalar) {
  FLOAT2 result = { v.x*scalar, v.y*scalar };
  return result;
}

float safeDivide(float num, float denom) {
  if(abs(denom) > 0.00001f) {
    return num/denom;
  }
  return 0.0f;
}

float crossProduct(float ax, float ay, float bx, float by) {
  //[ax] x [bx] = [ax*by - ay*bx]
  //[ay]   [by]
  return ax*by - ay*bx;
}

FLOAT2 orthogonal(float x, float y) {
  //Cross product with unit Z since everything in 2D is orthogonal to Z
  //[x] [0] [ y]
  //[y]x[0]=[-x]
  //[0] [1] [ 0]
  FLOAT2 result = { y, -x };
  return result;
}

float dotProduct(float ax, float ay, float bx, float by) {
  return ax*bx + ay*by;
}

float dotProduct2(FLOAT2 l, FLOAT2 r) {
  return l.x*r.x + l.y*r.y;
}

FLOAT2 transposeRotation(float cosAngle, float sinAngle) {
  //Rotation matrix is
  //[cos(x), -sin(x)]
  //[sin(x), cos(x)]
  //So the transpose given cos and sin is negating sin
  FLOAT2 result = { cosAngle, -sinAngle };
  return result;
}

//Multiply rotation matrices A*B represented by cos and sin since they're symmetric
FLOAT2 multiplyRotationMatrices(float cosAngleA, float sinAngleA, float cosAngleB, float sinAngleB) {
  //[cosAngleA, -sinAngleA]*[cosAngleB, -sinAngleB] = [cosAngleA*cosAngleB - sinAngleA*sinAngleB, ...]
  //[sinAngleA, cosAngleA]  [sinAngleB, cosAngleB]    [sinAngleA*cosAngleB + cosAngleA*sinAngleB, ...]
  FLOAT2 result = { cosAngleA*cosAngleB - sinAngleA*sinAngleB, sinAngleA*cosAngleB + cosAngleA*sinAngleB };
  return result;
}

//Multiply M*V where M is the rotation matrix represented by cos/sinangle and V is a vector
FLOAT2 multiplyVec2ByRotation(float cosAngle, float sinAngle, float vx, float vy) {
  //[cosAngle, -sinAngle]*[vx] = [cosAngle*vx - sinAngle*vy]
  //[sinAngle,  cosAngle] [vy]   [sinAngle*vx + cosAngle*vy]
  FLOAT2 result = { cosAngle*vx - sinAngle*vy, sinAngle*vx + cosAngle*vy };
  return result;
}

//Get the relative right represented by this rotation matrix, in other words the first basis vector (first column of matrix)
FLOAT2 getRightFromRotation(float cosAngle, float sinAngle) {
  //It already is the first column
  FLOAT2 result = { cosAngle, sinAngle };
  return result;
}

//Get the second basis vector (column)
FLOAT2 getUpFromRotation(float cosAngle, float sinAngle) {
  //[cosAngle, -sinAngle]
  //[sinAngle,  cosAngle]
  FLOAT2 result = { -sinAngle, cosAngle };
  return result;
}