
struct UniformSymmetricMatrix {
  uniform float* a; uniform float* b; uniform float* c;
                    uniform float* d; uniform float* e;
                                      uniform float* f;
};

struct UniformVec3 {
 uniform float* x;
 uniform float* y;
 uniform float* z;
};

struct UniformQuat {
  uniform float* i;
  uniform float* j;
  uniform float* k;
  uniform float* w;
};

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

struct SymmetricMatrix {
  float a, b, c,
           d, e,
              f;
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
//[dx, ey, fz] * [b, e, h] = [adx + bey + cfz, ddx + eey + ffz, dgx + ehy + fiz]
//[gx, hy, iz]   [c, f, i]   [agx + bhy + ciz, dgx + ehy + fiz, ggx + hhy + iiz]
// Result is symmetric
//[ax, by, cz]   [a, d, g]   [aax + bby + ccz, adx + bey + cfz, agx + bhy + ciz]
//[dx, ey, fz] * [b, e, h] = [               , ddx + eey + ffz, dgx + ehy + fiz]
//[gx, hy, iz]   [c, f, i]   [               ,                , ggx + hhy + iiz]
export void recomputeInertiaTensor(
  const uniform UniformQuat& orientation,
  const uniform UniformVec3& localInertia,
  uniform UniformSymmetricMatrix& inertia,
  uniform uint32 count) {
  uniform int columnCount = 3;
  foreach(i = 0 ... count) {
    const float<4> o = { orientation.i[i], orientation.j[i], orientation.k[i], orientation.w[i] };
    //Convert rotation to matrix
    Matrix rot = quat_to_matrix(o);
    //Compute rot.scaled(localInertia) * rot.transposed()
    const float x = localInertia.x[i];
    const float y = localInertia.y[i];
    const float z = localInertia.z[i];
    inertia.a[i] = rot.a*rot.a*x + rot.b*rot.b*y + rot.c*rot.c*z; inertia.b[i] = rot.a*rot.d*x + rot.b*rot.e*y + rot.c*rot.f*z; inertia.c[i] = rot.a*rot.g*x + rot.b*rot.h*y + rot.c*rot.i*z;
                                                                  inertia.d[i] = rot.d*rot.d*x + rot.e*rot.e*y + rot.f*rot.f*z; inertia.e[i] = rot.d*rot.g*x + rot.e*rot.h*y + rot.f*rot.i*z;
                                                                                                                                inertia.f[i] = rot.g*rot.g*x + rot.h*rot.h*y + rot.i*rot.i*z;
  }
}
