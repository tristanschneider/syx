#include "IspcMath.h"
#include "Uniforms.h"

//Since these are unit squares the intersect time is also the distance along the line
//since any edge would be of length one
//line is the location of the line on the perpendicular axis, so if intersecting with the X axis it's the Y position of the line along the X axis
//Given start of segment 's' and end 'e' with edge position E, this computes the intersection time t such that the intersection point I is:
//I = (s + (e-s)*t)
//Using the X axis as an example, we already know the y coordinate of I is E, since that's where the edge is
//Iy = (sy + (ey - sy)*t
//Iy = E
//Substitute E for Iy and rearrange to solve for T
//t = (E - sy)/(ey - sy)
//The other probably easier way to think about that is it's the distance from the start to the intersection divided by the total distance
float<2> getSegmentLineIntersectTimeAndOverlapAlongAxis1D(float line, float segmentBegin, float segmentEnd) {
  const float length = segmentEnd - segmentBegin;
  float t = -1.0f;
  //If this is zero the line is parallel to axis, no intersection
  if(abs(length) > 0.00001f) {
    t = (line - segmentBegin)/length;
  }
  //distance from end of the line to the intersection point on the line
  float overlap = abs(length)*(1.0f - t);
  float<2> result = { t, overlap };
  return result;
}

int addIndexWrapped(int index, int toAdd, int size) {
  int result = index + toAdd;
  if(result >= size) {
    return 0;
  }
  if(result < 0) {
    //Fine because this is always called with non-zero size
    return size - 1;
  }
  return result;
}

export void generateUnitCubeCubeContacts(
  uniform UniformConstVec2& positionsA,
  uniform UniformRotation& rotationsA,
  uniform UniformConstVec2& positionsB,
  uniform UniformRotation& rotationsB,
  uniform UniformVec2& resultNormals,
  uniform UniformContact& resultContactOne,
  uniform UniformContact& resultContactTwo,
  uniform uint32 count
) {
  uniform const float<2> aNormals[4] = { { 0.0f, 1.0f }, { 0.0f, -1.0f }, { 1.0f, 0.0f }, { -1.0f, 0.0f } };
  uniform const float aEdges[4] = { 0.5f, -0.5f, 0.5f, -0.5f };
  uniform const float aNormals1D[4] = { 1.0f, -1.0f, 1.0f, -1.0f };

  foreach(i = 0 ... count) {
    //Transpose to undo the rotation of A
    const float<2> rotA = { rotationsA.cosAngle[i], rotationsA.sinAngle[i] };
    const float<2> rotAInverse = transposeRotation(rotA.x, rotA.y);
    const float<2> posA = { positionsA.x[i], positionsA.y[i] };

    const float<2> rotB = { rotationsB.cosAngle[i], rotationsB.sinAngle[i] };
    const float<2> posB = { positionsB.x[i], positionsB.y[i] };
    //B's rotation in A's local space. Transforming to local space A allows this to be solved as computing
    //contacts between an AABB and an OBB instead of OBB to OBB
    const float<2> rotBInA = multiplyRotationMatrices(rotB.x, rotB.y, rotAInverse.x, rotAInverse.y);
    //Get basis vectors with the lengths of B so that they go from the center to the extents
    const float<2> upB = vec2Multiply(getUpFromRotation(rotBInA.x, rotBInA.y), 0.5f);
    const float<2> rightB = vec2Multiply(getRightFromRotation(rotBInA.x, rotBInA.y), 0.5f);
    float<2> posBInA = vec2Sub(posB, posA);
    posBInA = multiplyVec2ByRotation(rotAInverse.x, rotAInverse.y, posBInA.x, posBInA.y);

    const float extentAX = 0.5f;
    const float extentAY = 0.5f;
    //Sutherland hodgman clipping of B in the space of A, meaning all the clipping planes are cardinal axes
    int outputCount = 0;
    //8 should be the maximum amount of points that can result from clipping a square against another, which is when they are inside each-other and all corners of one intersect the edges of the other
    float<2> outputPoints[8];

    //Upper right, lower right, lower left, upper left
    outputPoints[0] = vec2Add(vec2Add(posBInA, upB), rightB);
    outputPoints[1] = vec2Add(vec2Sub(posBInA, upB), rightB);
    outputPoints[2] = vec2Sub(vec2Sub(posBInA, upB), rightB);
    outputPoints[3] = vec2Sub(vec2Add(posBInA, upB), rightB);
    outputCount = 4;

    //ispc prefers single floats even here to avoid "gather required to load value" performance warnings
    float inputPointsX[8];
    float inputPointsY[8];
    int inputCount = 0;
    float bestOverlap = 99999.0f;
    float secondBestOverlap = bestOverlap;
    float<2> bestNormal = aNormals[0];
    float<2> bestPoint = { 0.0f, 0.0f };
    float<2> secondBestPoint = { 0.0f, 0.0f };
    float<2> lastPoint = outputPoints[3];
    bool lastInside = false;
    float lastOverlap = 0.0f;

    bool allPointsInside = true;

    for(uniform int edgeA = 0; edgeA < 4; ++edgeA) {
      //Copy previous output to current input
      inputCount = outputCount;
      for(int j = 0; j < inputCount; ++j) {
        inputPointsX[j] = outputPoints[j].x;
        inputPointsY[j] = outputPoints[j].y;
      }
      //This will happen as soon as a separating axis is found as all points will land outside the clip edge and get discarded
      if(!outputCount) {
        break;
      }
      outputCount = 0;

      //ispc doesn't like reading varying from array by index
      uniform float aEdge = aEdges[edgeA];
      uniform float aNormal1D = aNormals1D[edgeA];

      //Last inside is invalidated when the edges change since it's inside relative to a given edge
      lastOverlap = (aEdge - (edgeA < 2 ? lastPoint.y : lastPoint.x))*aNormal1D;
      lastInside = lastOverlap >= 0.0f;

      for(int j = 0; j < inputCount; ++j) {
        const float<2> currentPoint = { inputPointsX[j], inputPointsY[j] };
        //TODO: wasted subtraction for the last iteration that did this already
        const float currentOnNormal = edgeA < 2 ? currentPoint.y : currentPoint.x;
        const float lastOnNormal = edgeA < 2 ? lastPoint.y : lastPoint.x;
        //Subtraction with a point on the plane then dot product with the normal
        //TODO: use overlaps to compute intersection instead of an unrelated computation
        const float currentOverlap = (aEdge - currentOnNormal)*aNormal1D;
        const bool currentInside = currentOverlap >= 0;

        float overlapThisEdge = 99999.9f;
        //TODO: re-use subtraction above in intersect calculation below
        float<2> currentIntersectPoint;
        if(currentInside) {
          allPointsInside = false;
          if(!lastInside) {
            //Went from outside to inside, add intersect
            const float<2> intersect = getSegmentLineIntersectTimeAndOverlapAlongAxis1D(aEdge, lastOnNormal, currentOnNormal);
            overlapThisEdge = intersect.y;
            currentIntersectPoint = vec2Add(lastPoint, vec2Multiply(vec2Sub(currentPoint, lastPoint), intersect.x));
            #pragma ignore warning(perf)
            outputPoints[outputCount] = currentIntersectPoint;
            ++outputCount;
          }
          //Is inside, add current
          #pragma ignore warning(perf)
          outputPoints[outputCount] = currentPoint;
          ++outputCount;
        }
        else if(lastInside) {
          //Went from inside to outside, add intersect. Compute it backwards so that the line is always going into the shape, which makes the overlap computation simpler
          const float<2> intersect = getSegmentLineIntersectTimeAndOverlapAlongAxis1D(aEdge, currentOnNormal, lastOnNormal);
          overlapThisEdge = intersect.y;
          currentIntersectPoint = vec2Add(currentPoint, vec2Multiply(vec2Sub(lastPoint, currentPoint), intersect.x));
          #pragma ignore warning(perf)
          outputPoints[outputCount] = currentIntersectPoint;
          ++outputCount;
        }

        //Keep track of the least positive overlap for the final results
        if(overlapThisEdge >= 0.0f && overlapThisEdge < bestOverlap) {
          secondBestPoint = bestPoint;
          secondBestOverlap = bestOverlap;

          bestOverlap = overlapThisEdge;
          bestNormal = aNormals[edgeA];
          bestPoint = currentIntersectPoint;
        }
        lastPoint = currentPoint;
        lastInside = currentInside;
      }
    }

    if(outputCount == 0) {
      //No collision, store negative overlap to indicate this
      resultContactOne.overlap[i] = -1.0f;
      resultContactTwo.overlap[i] = -1.0f;
    }
    else if(allPointsInside) {
      //Niche case where one shape is entirely inside another. Overlap is only determined
      //for intersect points which is fine for all cases except this one
      //Return arbitrary contacts here. Not too worried about accuracy because collision resolution has broken down if this happened
      resultContactOne.overlap[i] = 0.5f;
      resultContactOne.x[i] = posB.x;
      resultContactOne.y[i] = posB.y;
      resultContactTwo.overlap[i] = -1.0f;
      resultNormals.x[i] = 1.0f;
      resultNormals.x[i] = 0.0f;
    }
    else {
      const float<2> bestNormalInA = bestNormal;
      float<2> resultNormal = bestNormalInA;
      //Transform normal to world
      bestNormal = multiplyVec2ByRotation(rotA.x, rotA.y, resultNormal.x, resultNormal.y);
      //Flip from being a face on A to going towards A
      resultNormals.x[i] = -resultNormal.x;
      resultNormals.y[i] = -resultNormal.y;

      //Contacts are the two most overlapping points along the normal axis
      float<2> contactOne = bestPoint;
      float<2> contactTwo = secondBestPoint;
      float contactTwoOverlap = secondBestOverlap;
      float contactOneOverlap = bestOverlap;

      //Transform the contacts back to world
      contactOne = vec2Add(posA, multiplyVec2ByRotation(rotA.x, rotA.y, contactOne.x, contactOne.y));
      contactTwo = vec2Add(posA, multiplyVec2ByRotation(rotA.x, rotA.y, contactTwo.x, contactTwo.y));

      //Store the final results
      resultContactOne.x[i] = contactOne.x;
      resultContactOne.y[i] = contactOne.y;
      resultContactOne.overlap[i] = contactOneOverlap;

      resultContactTwo.x[i] = contactTwo.x;
      resultContactTwo.y[i] = contactTwo.y;
      resultContactTwo.overlap[i] = contactTwoOverlap;
    }
  }
}

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