#include "IspcMath.ispc"
#include "Uniforms.ispc"

//Given overlap `o` between spheres of radius `a` and `b` separated by distance `d`:
//d = a + b - o;
//Distance between input positions gives us, d, we have radius a and b, so we want to solve for overlap o
// o = a + b - d
//Contant point is the tip of sphere A in direction of B
export void computeContactSphereSphere(
  const uniform UniformVec3& posA,
  const uniform UniformVec3& posB,
  const uniform float radiusA[],
  const uniform float radiusB[],
  uniform UniformVec3& resultContactPoint,
  //Going from B to A, normalized
  uniform UniformVec3& resultContactNormal,
  //Amount the shapes are overlapping, negative if there is no collision
  uniform float resultOverlap[],
  uniform uint32 count
) {
  foreach(i = 0 ... count) {
    float normalX = posA.x[i] - posB.x[i];
    float normalY = posA.y[i] - posB.y[i];
    float normalZ = posA.z[i] - posB.z[i];
    float distance = sqrt(normalX*normalX + normalY*normalY + normalZ*normalZ);

    //May be negative which is no collision
    resultOverlap[i] = radiusA[i] + radiusB[i] - distance;
    //It seems like continue should be the same as computing the values since the gang must continue but performance tests show it is faster to early out when there are no collisions
    if(resultOverlap[i] <= 0) {
      continue;
    }

    //If the distance is zero the spheres are centered on each-other, choose an arbitrary normal
    if(abs(distance) <= EPSILON) {
      normalX = 1;
      normalY = 0;
      normalZ = 0;
      distance = 1;
    }

    //Normalize
    normalX /= distance;
    normalY /= distance;
    normalZ /= distance;

    resultContactNormal.x[i] = normalX;
    resultContactNormal.y[i] = normalY;
    resultContactNormal.z[i] = normalZ;

    //Normal is extent of A in the direction of B, normal is towards A, so subtract A's radius along normal
    resultContactPoint.x[i] = posA.x[i] - normalX*radiusA[i];
    resultContactPoint.y[i] = posA.y[i] - normalY*radiusA[i];
    resultContactPoint.z[i] = posA.z[i] - normalZ*radiusA[i];
  }
}