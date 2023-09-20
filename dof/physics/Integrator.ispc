export void integratePosition(uniform float position[], uniform const float velocity[], uniform uint32 count) {
  foreach(i = 0 ... count) {
     position[i] += velocity[i];
  }
}

export void integrateRotation(uniform float cosAngle[], uniform float sinAngle[], uniform const float angularVelocity[], uniform uint32 count) {
  foreach(i = 0 ... count) {
    const float sv = sin(angularVelocity[i]);
    const float cv = cos(angularVelocity[i]);
    //2D matrix multiplication of these matrices, storing the result in sinAngle and cosAngle
    //Since the input and output is symmetric only the two values need to be computed
    //[cv, -sv][cosAngle, -sinAngle] = [cv*cosAngle - sv*sinAngle, -cv*sinAngle - sv*cosAngle]
    //[sv,  cv][sinAngle,  cosAngle]   [sv*cosAngle + cv*sinAngle, -sv*sinANgle + cv*cosAngle]
    const float newCosAngle = cv*cosAngle[i] - sv*sinAngle[i];
    const float newSinAngle = sv*cosAngle[i] + cv*sinAngle[i];

    cosAngle[i] = newCosAngle;
    sinAngle[i] = newSinAngle;
  }
}