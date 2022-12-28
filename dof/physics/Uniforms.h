#pragma once

struct UniformVec2 {
  uniform float* x;
  uniform float* y;
};

struct UniformConstVec2 {
  uniform const float* x;
  uniform const float* y;
};

struct UniformContact {
  uniform float* x;
  uniform float* y;
  //Goes negative to indicate distance if not colliding
  uniform float* overlap;
};

struct UniformRotation {
  uniform float* cosAngle;
  uniform float* sinAngle;
};

struct UniformConstRotation {
  uniform const float* cosAngle;
  uniform const float* sinAngle;
};

export uniform int getTargetWidth() {
  return TARGET_WIDTH;
}

enum SyncType {
  NoSync,
  SyncToIndexA,
  SyncToIndexB
};

export void exportHack(uniform SyncType) {};

struct UniformConstraintObject {
  uniform float* linVelX;
  uniform float* linVelY;
  uniform float* angVel;
  uniform int* syncIndex;
  uniform int* syncType;
};

struct UniformConstraintData {
  //Linear axis the constraint is limiting motion on. Same for both objects but flipped
  uniform float* linearAxisX;
  uniform float* linearAxisY;
  //Angular axis the constraint is limiting motion on, different for A and B
  uniform float* angularAxisA;
  uniform float* angularAxisB;
  uniform float* constraintMass;
  //linear axis multiplied by inverse mass, used to apply the impulses
  //Since the mass of both is the same only one premultiplied vector is needed
  uniform float* linearImpulseX;
  uniform float* linearImpulseY;
  //angular axis multiplied by inertia, used to apply the impulses. The inertia for both objects is the same but the vector isn't
  uniform float* angularImpulseA;
  uniform float* angularImpulseB;
  uniform float* bias;
};