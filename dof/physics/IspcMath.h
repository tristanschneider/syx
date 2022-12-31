#pragma once

float vec2Length(float<2> v) {
  return sqrt(v.x*v.x + v.y*v.y);
}

float<2> vec2Sub(float<2> l, float<2> r) {
  float<2> result = { l.x - r.x, l.y - r.y };
  return result;
}

float<2> vec2Add(float<2> l, float<2> r) {
  float<2> result = { l.x + r.x, l.y + r.y };
  return result;
}

float<2> vec2Multiply(float<2> v, float scalar) {
  float<2> result = { v.x*scalar, v.y*scalar };
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

float dotProduct(float ax, float ay, float bx, float by) {
  return ax*bx + ay*by;
}

float dotProduct2(float<2> l, float<2> r) {
  return l.x*r.x + l.y*r.y;
}

float<2> transposeRotation(float cosAngle, float sinAngle) {
  //Rotation matrix is
  //[cos(x), -sin(x)]
  //[sin(x), cos(x)]
  //So the transpose given cos and sin is negating sin
  float<2> result = { cosAngle, -sinAngle };
  return result;
}

//Multiply rotation matrices A*B represented by cos and sin since they're symmetric
float<2> multiplyRotationMatrices(float cosAngleA, float sinAngleA, float cosAngleB, float sinAngleB) {
  //[cosAngleA, -sinAngleA]*[cosAngleB, -sinAngleB] = [cosAngleA*cosAngleB - sinAngleA*sinAngleB, ...]
  //[sinAngleA, cosAngleA]  [sinAngleB, cosAngleB]    [sinAngleA*cosAngleB + cosAngleA*sinAngleB, ...]
  float<2> result = { cosAngleA*cosAngleB - sinAngleA*sinAngleB, sinAngleA*cosAngleB + cosAngleA*sinAngleB };
  return result;
}

//Multiply M*V where M is the rotation matrix represented by cos/sinangle and V is a vector
float<2> multiplyVec2ByRotation(float cosAngle, float sinAngle, float vx, float vy) {
  //[cosAngle, -sinAngle]*[vx] = [cosAngle*vx - sinAngle*vy]
  //[sinAngle,  cosAngle] [vy]   [sinAngle*vx + cosAngle*vy]
  float<2> result = { cosAngle*vx - sinAngle*vy, sinAngle*vx + cosAngle*vy };
  return result;
}

//Get the relative right represented by this rotation matrix, in other words the first basis vector (first column of matrix)
float<2> getRightFromRotation(float cosAngle, float sinAngle) {
  //It already is the first column
  float<2> result = { cosAngle, sinAngle };
  return result;
}

//Get the second basis vector (column)
float<2> getUpFromRotation(float cosAngle, float sinAngle) {
  //[cosAngle, -sinAngle]
  //[sinAngle,  cosAngle]
  float<2> result = { -sinAngle, cosAngle };
  return result;
}