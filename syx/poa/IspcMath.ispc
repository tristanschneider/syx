//Arbitrary, maybe should be configurable?
static const float EPSILON = 0.0000001f;

static float safeDivide(float num, float denom) {
  uniform float test = 0;
  if(abs(denom) <= EPSILON) {
    return 0.0f;
  }
  return num/denom;
}
