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

//TODO: this ended up pretty messy. It could be optimized in small ways by changing back to single-axis based computations
//It is likely possible to take better advantage of ispc by splitting each plane clipping phase into its own pass,
//like clipping all points across all collision pairs along the x axes
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
    float bestOverlap = 999.9f;
    float<2> bestNormal = aNormals[0];

    bool allPointsInside = true;
      uniform int di = 0;

    for(uniform int edgeA = 0; edgeA < 4; ++edgeA) {
      //Copy previous output to current input
      inputCount = outputCount;
      float<2> lastPoint;
      for(int j = 0; j < inputCount; ++j) {
        lastPoint.x = inputPointsX[j] = outputPoints[j].x;
        lastPoint.y = inputPointsY[j] = outputPoints[j].y;
      }
      //This will happen as soon as a separating axis is found as all points will land outside the clip edge and get discarded
      if(!outputCount) {
        break;
      }
      outputCount = 0;

      //ispc doesn't like reading varying from array by index
      uniform float<2> aNormal = aNormals[edgeA];

      //Last inside is invalidated when the edges change since it's inside relative to a given edge
      float lastOverlap = 0.5f - dotProduct(aNormal.x, aNormal.y, lastPoint.x, lastPoint.y);
      bool lastInside = lastOverlap >= 0.0f;

      float currentEdgeOverlap = 0.0f;
      for(int j = 0; j < inputCount; ++j) {
        const float<2> currentPoint = { inputPointsX[j], inputPointsY[j] };
        //(e-p).n
        const float currentOverlap = 0.5f - dotProduct(aNormal.x, aNormal.y, currentPoint.x, currentPoint.y);
        const bool currentInside = currentOverlap >= 0;
        const float<2> lastToCurrent = vec2Sub(currentPoint, lastPoint);
        //Might be division by zero but if so the intersect won't be used because currentInside would match lastInside
        //(e-p).n/(e-s).n
        const float t = 1.0 - (abs(currentOverlap)/abs(dotProduct(aNormal.x, aNormal.y, lastToCurrent.x, lastToCurrent.y)));
        const float<2> intersect = vec2Add(lastPoint, vec2Multiply(lastToCurrent, t));

        currentEdgeOverlap = max(currentOverlap, currentEdgeOverlap);

        //TODO: re-use subtraction above in intersect calculation below
        if(currentInside) {
          allPointsInside = false;
          if(!lastInside) {
            //Went from outside to inside, add intersect
            #pragma ignore warning(perf)
            outputPoints[outputCount] = intersect;
            ++outputCount;
          }
          //Is inside, add current
          #pragma ignore warning(perf)
          outputPoints[outputCount] = currentPoint;
          ++outputCount;
        }
        else if(lastInside) {
          //Went from inside to outside, add intersect.
          #pragma ignore warning(perf)
          outputPoints[outputCount] = intersect;
          ++outputCount;
        }

        lastPoint = currentPoint;
        lastInside = currentInside;
      }

      //Keep track of the least positive overlap for the final results
      if(currentEdgeOverlap < bestOverlap) {
        bestOverlap = currentEdgeOverlap;
        bestNormal = aNormal;
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
      //TODO: need to figure out a better way to do this
      //Also need to try the axes of B to see if they produce a better normal than what was found through clipping
      float<2> candidateNormals[3] = { bestNormal, getUpFromRotation(rotBInA.x, rotBInA.y), getRightFromRotation(rotBInA.x, rotBInA.y) };
      const float<2> originalBestNormal = bestNormal;
      float bestNormalDiff = 99.0f;
      //Figuring out the best normal has two parts:
      // - Determining which results  in the least overlap along the normal, which is the distance between the two extremes of projections of all clipped points onto normal
      // - Determine the sign of the normal
      // This loop will do the former by determining all the projections on the normal and picking the normal that has the greatest difference
      // Then the result can be flipped so it's pointing in the same direction as the original best
      // This is a hacky assumption based on that either the original best will be chosen or one not far off from it
      for(uniform int j = 0; j < 3; ++j) {
        float thisMin = 999.0f;
        float thisMax = -thisMin;
        float<2> normal = candidateNormals[j];
        for(uniform int k = 0; k < outputCount; ++k) {
          float thisOverlap = dotProduct2(normal, outputPoints[k]);
          thisMin = min(thisMin, thisOverlap);
          thisMax = max(thisMax, thisOverlap);
        }
        //This is the absolute value of the total amount of overlap along this axis, we're looking for the normal with the smallest overlap
        const float thisNormalDiff = thisMax - thisMin;
        if(thisNormalDiff < bestNormalDiff) {
          bestNormalDiff = thisNormalDiff;
          bestNormal = normal;
        }
      }

      //Now the best normal is known, make sure it's pointing in a similar direction to the original
      if(dotProduct2(originalBestNormal, bestNormal) < 0.0f) {
        bestNormal = vec2Negate(bestNormal);
      }

      //Now find the two best contact points. The normal is going away from A, so the smallest projection is the one with the most overlap, since it's going most against the normal
      //The overlap for any point is the distance of its projection from the greatest projection: the point furthest away from A
      float<2> bestPoint;
      float<2> secondBestPoint;
      float minProjection = 999.0f;
      float secondMinProjection = minProjection;
      float maxProjection = -1;
      for(uniform int j = 0; j < outputCount; ++j) {
        const float<2> thisPoint = outputPoints[j];
        const float thisProjection = dotProduct2(bestNormal, thisPoint);
        maxProjection = max(maxProjection, thisProjection);
        if(thisProjection < minProjection) {
          secondMinProjection = minProjection;
          secondBestPoint = bestPoint;

          minProjection = thisProjection;
          bestPoint = thisPoint;
        }
        else if(thisProjection < secondMinProjection) {
          secondMinProjection = thisProjection;
          secondBestPoint = thisPoint;
        }
      }

      //Contacts are the two most overlapping points along the normal axis
      float<2> contactOne = bestPoint;
      float<2> contactTwo = secondBestPoint;
      float contactTwoOverlap = maxProjection - secondMinProjection;
      float contactOneOverlap = maxProjection - minProjection;

      //Transform the contacts back to world
      contactOne = vec2Add(posA, multiplyVec2ByRotation(rotA.x, rotA.y, contactOne.x, contactOne.y));
      contactTwo = vec2Add(posA, multiplyVec2ByRotation(rotA.x, rotA.y, contactTwo.x, contactTwo.y));

      //Transform normal to world
      bestNormal = multiplyVec2ByRotation(rotA.x, rotA.y, bestNormal.x, bestNormal.y);
      //Flip from being a face on A to going towards A
      resultNormals.x[i] = -bestNormal.x;
      resultNormals.y[i] = -bestNormal.y;

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