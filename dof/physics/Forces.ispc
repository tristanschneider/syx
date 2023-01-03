export void applyDampingMultiplier(uniform float velocity[], uniform float multiplier, uniform uint32 count) {
  foreach(i = 0 ... count) {
     velocity[i] *= multiplier;
  }
}