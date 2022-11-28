#include "Quaternion.ispc"
#include "Uniforms.ispc"

//Special case for constant accelleration because the most likely use case is gravity which is constant for all instances
export void integrateLinearVelocityGlobalAcceleration(uniform float velocity[], uniform float accelleration, uniform float dt, uniform uint32 count) {
  const float scaledAccelleration = accelleration*dt;
  foreach(i = 0 ... count) {
    velocity[i] += scaledAccelleration;
  }
}

export void integrateLinearPosition(uniform float position[],
  uniform const float velocity[],
  uniform float dt,
  uniform uint32 count) {
  foreach(i = 0 ... count) {
    position[i] += velocity[i]*dt;
  }
}

//First order integration (Taylor series expansion) of quaternions assuming angular velocity in world space
//W is the imaginary component
export void integrateRotation(
  uniform UniformQuat& orientation,
  uniform const UniformConstVec3& angVel,
  uniform float dt,
  uniform uint32 count) {
  // Integration is:
  // orientation = normalized(orientation + 0.5*angVel*orientation*dt)
  const float scalar = 0.5*dt;
  foreach(i = 0 ... count) {
    //angVel*orientation part
    const float<4> origOrientation = { orientation.i[i], orientation.j[i], orientation.k[i], orientation.w[i] };
    const float<3> angVel4 = { angVel.x[i], angVel.y[i], angVel.z[i] };
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
    orientation.i[i] = result.x*inverseLength;
    orientation.j[i] = result.y*inverseLength;
    orientation.k[i] = result.z*inverseLength;
    orientation.w[i] = result.w*inverseLength;
  }
}
