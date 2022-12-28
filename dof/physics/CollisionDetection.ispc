#include "IspcMath.h"
#include "Uniforms.h"

export void generateUnitSphereSphereContacts(
  uniform UniformConstVec2& positionsA,
  uniform UniformConstVec2& positionsB,
  uniform UniformVec2& resultNormals,
  uniform UniformContact& resultContact,
  uniform uint32 count
) {
  const float radius = 0.5;
  const float combinedABRadius = radius*2;
  foreach(i = 0 ... count) {
    const float<2> posA = { positionsA.x[i], positionsA.y[i] };
    const float<2> posB = { positionsB.x[i], positionsB.y[i] };
    const float<2> bToA = vec2Sub(posA, posB);
    const float bToALen = vec2Length(bToA);
    resultContact.overlap[i] = combinedABRadius - bToALen;

    if(abs(bToALen) > 0.00001f) {
      //Normalize
      resultNormals.x[i] = bToA.x / bToALen;
      resultNormals.y[i] = bToA.y / bToALen;
    }
    else {
      //Objects are perfectly overlapping, pick an arbitrary axis
      resultNormals.x[i] = 1.0f;
      resultNormals.y[i] = 0.0f;
    }

    resultContact.x[i] = posA.x - resultNormals.x[i]*radius;
    resultContact.y[i] = posA.y - resultNormals.y[i]*radius;
  }
}

//Turn contact point into vector from object to contact
export void turnContactsToRVectors(
  uniform float posA[],
  uniform float posB[],
  uniform const float contact[],
  uniform float rA[],
  uniform float rB[],
  uniform uint32 count
) {
  foreach(i = 0 ... count) {
    rA[i] = contact[i] - posA[i];
    rB[i] = contact[i] - posB[i];
  }
}