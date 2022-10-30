#pragma once

// Multiply two quaternions, imaginary element is last
float<4> quat_mul_one(float<4> l, float<4> r) {
  float<4> result = {
    l.x*r.w + l.w*r.x + l.y*r.z - l.z*r.y,
    l.w*r.y - l.x*r.z + l.y*r.w + l.z*r.x,
    l.w*r.z + l.x*r.y - l.y*r.x + l.z*r.w,
    l.w*r.w - l.x*r.x - l.y*r.y - l.z*r.z
  };
  return result;
}

// Multiply two quaternions but the imaginary component of l is zero
float<4> quat_mul_one_no_w_l(float<3> l, float<4> r) {
  float<4> result = {
    l.x*r.w +         + l.y*r.z - l.z*r.y,
            - l.x*r.z + l.y*r.w + l.z*r.x,
              l.x*r.y - l.y*r.x + l.z*r.w,
            - l.x*r.x - l.y*r.y - l.z*r.z
  };
  return result;
}

float quat_length(float<4> q) {
  return sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
}
