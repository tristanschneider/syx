#pragma once

float vec2Length(float<2> v) {
  return sqrt(v.x*v.x + v.y*v.y);
}

float<2> vec2Sub(float<2> l, float<2> r) {
  float<2> result = { l.x - r.x, l.y - r.y };
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