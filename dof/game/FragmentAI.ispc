#include "Uniforms.h"

//Repel as a spring
//F = kx
//Where F is the force of the spring, k is the spring constant, and x is the distance from a relaxed position
//ma = kx
//m*(dv/dt) = kx
//dv = (kx*dt)/m
//dv is the change in velocity we want for this time slice dt, which we'll assume is 1 so it can be ignored
//This one also assumes uniform mass of all passed in objects, at which point dv = x
export void repelWorldBoundary(uniform float position[], uniform float velocity[], uniform float boundaryMin, uniform float boundaryMax, uniform float springConstant, uniform uint32 count) {
  foreach(i = 0 ... count) {
    const float pos = position[i];
    float withinBoundary = clamp(pos, boundaryMin, boundaryMax);
    velocity[i] += (withinBoundary - pos)*springConstant;
  }
}

//Sets the goalFound bit to 1 if the fragment is within minDistance of their goal,
//calculated in one dimension twice since accuracy here isn't particularly important
export void checkFragmentGoals(
  uniform UniformConstVec2& position,
  uniform UniformConstVec2& goal,
  uniform uint8 goalFound[],
  uniform float minDistance,
  uniform uint32 count) {
  foreach(i = 0 ... count) {
    if(abs(goal.x[i] - position.x[i]) < minDistance && abs(goal.y[i] - position.y[i]) < minDistance) {
      goalFound[i] = 1;
    }
  }
}