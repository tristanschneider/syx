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

export void applyDampingMultiplier(uniform float velocity[], uniform float multiplier, uniform uint32 count) {
  foreach(i = 0 ... count) {
     velocity[i] *= multiplier;
  }
}