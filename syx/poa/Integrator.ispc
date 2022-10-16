
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

//Special case for constant accelleration because the most likely use case is gravity which is constant for all instances
export void integrateLinearVelocityGlobalAccelleration(uniform float velocity[], uniform float accelleration, uniform float dt, uniform uint32 count) {
  const float scaledAccelleration = accelleration*dt;
  foreach(i = 0 ... count) {
    velocity[i] += scaledAccelleration;
  }
}

export void integrateLinearPosition(uniform float position[],
  uniform float velocity[],
  uniform float dt,
  uniform uint32 count) {
  foreach(i = 0 ... count) {
    position[i] += velocity[i]*dt;
  }
}

//First order integration (Taylor series expansion) of quaternions assuming angular velocity in world space
//W is the imaginary component
export void integrateRotation(
  uniform float orientationI[],
  uniform float orientationJ[],
  uniform float orientationK[],
  uniform float orientationW[],
  const uniform float angVelX[],
  const uniform float angVelY[],
  const uniform float angVelZ[],
  uniform float dt,
  uniform uint32 count) {
  // Integration is:
  // orientation = normalized(orientation + 0.5*angVel*orientation*dt)
  const float scalar = 0.5*dt;
  foreach(i = 0 ... count) {
    //angVel*orientation part
    const float<4> origOrientation = { orientationI[i], orientationJ[i], orientationK[i], orientationW[i] };
    const float<3> angVel4 = { angVelX[i], angVelY[i], angVelZ[i] };
    float<4> result = quat_mul_one_no_w_l(angVel4, origOrientation);

    //orientation + and 0.5*dt part
    result.x = origOrientation.x + scalar*result.x;
    result.y = origOrientation.y + scalar*result.y;
    result.z = origOrientation.z + scalar*result.z;
    result.w = origOrientation.w + scalar*result.w;

    //Compute normalization sacalar
    float inverseLength = quat_length(result);
    if(abs(inverseLength) > 0.0001f) {
      inverseLength = 1.0/inverseLength;
    }

    //Normalize and store result
    orientationI[i] = result.x*inverseLength;
    orientationJ[i] = result.y*inverseLength;
    orientationK[i] = result.z*inverseLength;
    orientationW[i] = result.w*inverseLength;
  }
}

//Matrix uses many values at once so having arrays single uniforms for each matrix element isn't that helpful
//Having arrays of rows allows for somewhat efficient multiplication with vectors since only one row is needed for one output element
struct MatrixRow {
  float x, y, z;
};

struct Matrix {
  float a, b, c,
        d, e, f,
        g, h, i;
};

Matrix quat_to_matrix(float<4> q) {
  Matrix result = {
    1.0f - 2.0f*q.y*q.y - 2.0f*q.z*q.z, 2.0f*q.x*q.y - 2.0f*q.z*q.w, 2.0f*q.x*q.z + 2.0f*q.y*q.w,
    2.0f*q.x*q.y + 2.0f*q.z*q.w, 1.0f - 2.0f*q.x*q.x - 2.0f*q.z*q.z, 2.0f*q.y*q.z - 2.0f*q.x*q.w,
    2.0f*q.x*q.z - 2.0f*q.y*q.w, 2.0f*q.y*q.z + 2.0f*q.x*q.w, 1.0f - 2.0f*q.x*q.x - 2.0f*q.y*q.y
  };
  return result;
}

//localinertia is a vec3 local space inverse inertia, rot is a rotation matrix, result is inertia tensor (matrix)
//newInertia = rot.scaled(localInertia) * rot.transposed();
//Scaled part
//[a, b, c]  [x, 0, 0] [ax, by, cz]
//[d, e, f]* [0, y, 0]=[dx, ey, fz]
//[g, h, i]  [0, 0, z] [gx, hy, iz]
// Multiply by transpose of itself part
//[ax, by, cz]   [a, d, g]   [aax + bby + ccz, adx + bey + cfz, agx + bhy + ciz]
//[dx, ey, fz] * [b, e, h] = [dax + eby + fcz, ddx + eey + ffz, dgx + ehy + fiz]
//[gx, hy, iz]   [c, f, i]   [gax + hby + icz, gdx + hey + ifz, ggx + hhy + iiz]
export void recomputeInertiaTensor(
  const uniform float orientationI[],
  const uniform float orientationJ[],
  const uniform float orientationK[],
  const uniform float orientationW[],
  const uniform float localInertiaX[],
  const uniform float localInertiaY[],
  const uniform float localInertiaZ[],
  uniform MatrixRow inertiaRA[],
  uniform MatrixRow inertiaRB[],
  uniform MatrixRow inertiaRC[],
  uniform uint32 count) {
  uniform int columnCount = 3;
  foreach(i = 0 ... count) {
    const float<4> orientation = { orientationI[i], orientationJ[i], orientationK[i], orientationW[i] };
    //Convert rotation to matrix
    Matrix rot = quat_to_matrix(orientation);
    //Compute rot.scaled(localInertia) * rot.transposed()
    //Do component by component, so all parts that need inertiaX, then inertiaY...
    Matrix result = { 0 };
    const float x = localInertiaX[i];
    result.a += rot.a*rot.a*x; result.b += rot.a*rot.d*x; result.c += rot.a*rot.g*x;
    result.d += rot.d*rot.a*x; result.e += rot.d*rot.d*x; result.f += rot.d*rot.g*x;
    result.g += rot.g*rot.a*x; result.h += rot.g*rot.d*x; result.i += rot.g*rot.g*x;

    const float y = localInertiaY[i];
    result.a += rot.b*rot.b*y; result.b += rot.b*rot.e*y; result.c += rot.b*rot.h*y;
    result.d += rot.e*rot.b*y; result.e += rot.e*rot.e*y; result.f += rot.e*rot.h*y;
    result.g += rot.h*rot.b*y; result.h += rot.h*rot.e*y; result.i += rot.h*rot.h*y;

    const float z = localInertiaZ[i];
    result.a += rot.c*rot.c*z; result.b += rot.c*rot.f*z; result.c += rot.c*rot.i*z;
    result.d += rot.f*rot.c*z; result.e += rot.f*rot.f*z; result.f += rot.f*rot.i*z;
    result.g += rot.i*rot.c*z; result.h += rot.i*rot.f*z; result.i += rot.i*rot.i*z;

    // TODO: figure out how to avoid "Performance Warning: Scatter required to store value."
    #pragma ignore warning(perf)
    inertiaRA[i].x = result.a; inertiaRA[i].y = result.b; inertiaRA[i].z = result.c;
    #pragma ignore warning(perf)
    inertiaRB[i].x = result.d; inertiaRB[i].y = result.e; inertiaRB[i].z = result.f;
    #pragma ignore warning(perf)
    inertiaRC[i].x = result.g; inertiaRC[i].y = result.h; inertiaRC[i].z = result.i;
  }
}
