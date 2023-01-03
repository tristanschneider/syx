#include "Precompile.h"
#include "Physics.h"

#include "glm/common.hpp"
#include "out_ispc/unity.h"
#include "TableOperations.h"

#include "glm/detail/func_geometric.inl"

namespace {
  template<class RowT, class TableT>
  decltype(std::declval<RowT&>().mElements.data()) _unwrapRow(TableT& t) {
    if constexpr(TableOperations::hasRow<RowT, TableT>()) {
      return std::get<RowT>(t.mRows).mElements.data();
    }
    else {
      return nullptr;
    }
  }

  template<class TableT>
  ispc::UniformContactConstraintPairData _unwrapUniformConstraintData(TableT& constraints) {
    return {
      _unwrapRow<ConstraintData::LinearAxisX>(constraints),
      _unwrapRow<ConstraintData::LinearAxisY>(constraints),
      _unwrapRow<ConstraintData::AngularAxisOneA>(constraints),
      _unwrapRow<ConstraintData::AngularAxisOneB>(constraints),
      _unwrapRow<ConstraintData::AngularAxisTwoA>(constraints),
      _unwrapRow<ConstraintData::AngularAxisTwoB>(constraints),
      _unwrapRow<ConstraintData::AngularFrictionAxisOneA>(constraints),
      _unwrapRow<ConstraintData::AngularFrictionAxisOneB>(constraints),
      _unwrapRow<ConstraintData::AngularFrictionAxisTwoA>(constraints),
      _unwrapRow<ConstraintData::AngularFrictionAxisTwoB>(constraints),
      _unwrapRow<ConstraintData::ConstraintMassOne>(constraints),
      _unwrapRow<ConstraintData::ConstraintMassTwo>(constraints),
      _unwrapRow<ConstraintData::FrictionConstraintMassOne>(constraints),
      _unwrapRow<ConstraintData::FrictionConstraintMassTwo>(constraints),
      _unwrapRow<ConstraintData::LinearImpulseX>(constraints),
      _unwrapRow<ConstraintData::LinearImpulseY>(constraints),
      _unwrapRow<ConstraintData::AngularImpulseOneA>(constraints),
      _unwrapRow<ConstraintData::AngularImpulseOneB>(constraints),
      _unwrapRow<ConstraintData::AngularImpulseTwoA>(constraints),
      _unwrapRow<ConstraintData::AngularImpulseTwoB>(constraints),
      _unwrapRow<ConstraintData::FrictionAngularImpulseOneA>(constraints),
      _unwrapRow<ConstraintData::FrictionAngularImpulseOneB>(constraints),
      _unwrapRow<ConstraintData::FrictionAngularImpulseTwoA>(constraints),
      _unwrapRow<ConstraintData::FrictionAngularImpulseTwoB>(constraints),
      _unwrapRow<ConstraintData::BiasOne>(constraints),
      _unwrapRow<ConstraintData::BiasTwo>(constraints)
    };
  }

  template<class CObj>
  ispc::UniformConstraintObject _unwrapUniformConstraintObject(ConstraintCommonTable& constraints) {
    using ConstraintT = ConstraintObject<CObj>;
    return {
      _unwrapRow<ConstraintT::LinVelX>(constraints),
      _unwrapRow<ConstraintT::LinVelY>(constraints),
      _unwrapRow<ConstraintT::AngVel>(constraints),
      _unwrapRow<ConstraintT::SyncIndex>(constraints),
      _unwrapRow<ConstraintT::SyncType>(constraints)
    };
  }

  struct IRect {
    glm::ivec2 mMin, mMax;
  };

  IRect _buildRect(const glm::vec2& min, const glm::vec2& max) {
    return {
      glm::ivec2{ int(std::floor(min.x)), int(std::floor(min.y)) },
      glm::ivec2{ int(std::ceil(max.x)), int(std::ceil(max.y)) }
    };
  }

  size_t _toIndex(int x, int y, const GridBroadphase::AllocatedDimensions& dimensions) {
    const int cx = glm::clamp(x, dimensions.mMin.x, dimensions.mMax.x);
    const int cy = glm::clamp(y, dimensions.mMin.y, dimensions.mMax.y);
    return size_t(cx - dimensions.mMin.x) + size_t(cy - dimensions.mMin.y)*dimensions.mStride;
  }

  bool _tryOrderCollisionPair(size_t& pairA, size_t& pairB, const PhysicsTableIds& tables) {
    //Always make zero mass object 'B' in a pair for simplicity
    if((pairA & tables.mTableIDMask) == tables.mZeroMassTable) {
      if((pairB & tables.mTableIDMask) == tables.mZeroMassTable) {
        //If they're both static, skip this pair
        return false;
      }
      std::swap(pairA, pairB);
    }
    return true;
  }

  void _addCollisionPair(size_t self, size_t other, CollisionPairsTable& table, const PhysicsTableIds& tableIds) {
    if(!_tryOrderCollisionPair(self, other, tableIds)) {
      return;
    }

    CollisionPairIndexA& rowA = std::get<CollisionPairIndexA>(table.mRows);
    CollisionPairIndexB& rowB = std::get<CollisionPairIndexB>(table.mRows);
    //Table is sorted by index A, so find A first
    auto it = std::lower_bound(rowA.begin(), rowA.end(), self);
    if(it != rowA.end() && *it == self) {
      //If A was found, start here in B to see if that index exists
      const size_t indexA = std::distance(rowA.begin(), it);
      for(size_t i = indexA; i < rowA.size(); ++i) {
        //If this passed the range of A indices then stop now, this is not a duplicate
        if(rowA.at(i) != self) {
          break;
        }
        //If B was found, that means the pair of A and B already exists, so no need to add it
        if(rowB.at(i) == other) {
          return;
        }
      }
    }

    //Insert only in this row here, at the end of collision pair gathering there will be a single resize to realign all the rows
    size_t insertIndex = size_t(std::distance(rowA.begin(), it));
    rowA.mElements.insert(it, self);
    rowB.mElements.insert(rowB.begin() + insertIndex, other);
  }
}

void Physics::details::_integratePositionAxis(float* velocity, float* position, size_t count) {
  ispc::integratePosition(position, velocity, uint32_t(count));
}

void Physics::details::_integrateRotation(float* rotX, float* rotY, float* velocity, size_t count) {
  ispc::integrateRotation(rotX, rotY, velocity, uint32_t(count));
}

void Physics::details::_applyDampingMultiplier(float* velocity, float amount, size_t count) {
  ispc::applyDampingMultiplier(velocity, amount, uint32_t(count));
}

void Physics::allocateBroadphase(GridBroadphase::BroadphaseTable& table) {
  const auto& dimensions = std::get<SharedRow<GridBroadphase::RequestedDimensions>>(table.mRows).at();
  auto& allocatedDimensions = std::get<SharedRow<GridBroadphase::AllocatedDimensions>>(table.mRows).at();
  constexpr float cellSize = 1.0f;
  assert(dimensions.mMin.x <= dimensions.mMax.x);
  assert(dimensions.mMin.y <= dimensions.mMax.y);

  const IRect rect = _buildRect(dimensions.mMin, dimensions.mMax);

  allocatedDimensions.mMin = rect.mMin;
  allocatedDimensions.mMax = rect.mMax;
  allocatedDimensions.mStride = std::max(size_t(1), size_t(rect.mMax.x - rect.mMin.x));
  size_t sizeY = size_t(allocatedDimensions.mMax.y - allocatedDimensions.mMin.y) + size_t(1);

  //Allocate desired space
  TableOperations::resizeTable(table, sizeY * allocatedDimensions.mStride);
  clearBroadphase(table);
}

void Physics::rebuildBroadphase(
  size_t baseIndex,
  const float* xPositions,
  const float* yPositions,
  GridBroadphase::BroadphaseTable& broadphase,
  size_t insertCount) {
  const auto& dimensions = std::get<SharedRow<GridBroadphase::AllocatedDimensions>>(broadphase.mRows).at();
  auto& overflow = std::get<SharedRow<GridBroadphase::Overflow>>(broadphase.mRows).at();
  std::vector<GridBroadphase::Cell>& cells = std::get<Row<GridBroadphase::Cell>>(broadphase.mRows).mElements;

  //Vector to max extents of the shape regardless of rotation
  const float centerToEdge = 0.5f;
  const glm::vec2 extents(std::sqrt(centerToEdge*centerToEdge*2));
  for(size_t i = 0; i < insertCount; ++i) {
    const glm::vec2 center{ xPositions[i], yPositions[i] };
    const IRect rect = _buildRect(center - extents, center + extents);
    const size_t indexToStore = baseIndex + i;

    //Store index to this in all cells it overlaps with
    for(int x = rect.mMin.x; x < rect.mMax.x; ++x) {
      for(int y = rect.mMin.y; y < rect.mMax.y; ++y) {
        bool slotFound = false;
        const size_t cellIndex = _toIndex(x, y, dimensions);
        for(size_t& storedIndex : cells[cellIndex].mElements) {
          if(storedIndex == GridBroadphase::EMPTY_ID) {
            storedIndex = indexToStore;
            slotFound = true;
            break;
          }
          //Don't put self in list multiple times. Shouldn't generally happen unless index is getting clamped due to being outside the boundaries
          else if(storedIndex == indexToStore) {
            slotFound = true;
            break;
          }
        }

        if(!slotFound) {
          overflow.mElements.push_back(indexToStore);
        }
      }
    }
  }

  //Remove duplicates
  std::sort(overflow.mElements.begin(), overflow.mElements.end());
  overflow.mElements.erase(std::unique(overflow.mElements.begin(), overflow.mElements.end()), overflow.mElements.end());
}

void Physics::clearBroadphase(GridBroadphase::BroadphaseTable& broadphase) {
  for(GridBroadphase::Cell& cell : std::get<Row<GridBroadphase::Cell>>(broadphase.mRows).mElements) {
    cell.mElements.fill(GridBroadphase::EMPTY_ID);
  }
  std::get<SharedRow<GridBroadphase::Overflow>>(broadphase.mRows).at().mElements.clear();
}

//TODO: this is a bit clunky to do separately from generation and update because most of the time all collision pairs from last frame would have been fine
void Physics::generateCollisionPairs(const GridBroadphase::BroadphaseTable& broadphase, CollisionPairsTable& pairs, const PhysicsTableIds& tableIds) {
  const GridBroadphase::Overflow& overflow = std::get<SharedRow<GridBroadphase::Overflow>>(broadphase.mRows).at();

  //TODO: retain pairs for a while then remove them occasionally if far away
  TableOperations::resizeTable(pairs, 0);

  //There could be a way to optimize for empty cells but the assumption is that most cells are not empty
  for(const GridBroadphase::Cell& cell : std::get<Row<GridBroadphase::Cell>>(broadphase.mRows).mElements) {
    for(size_t self = 0; self < cell.mElements.size(); ++self) {
      const size_t selfID = cell.mElements[self];
      if(selfID == GridBroadphase::EMPTY_ID) {
        continue;
      }
      //Add pairs with all others in the cell
      for(size_t other = self + 1; other < cell.mElements.size(); ++other) {
        const size_t otherID = cell.mElements[other];
        if(otherID == GridBroadphase::EMPTY_ID) {
          break;
        }
        //TODO: optimization here for avoiding pairs of static objects and for doing all inserts at once
        _addCollisionPair(selfID, otherID, pairs, tableIds);
      }

      //Add pairs with cell-less overflow objects
      //The hope is that overflow is rare/never happens
      for(size_t other : overflow.mElements) {
        if(other != selfID) {
          _addCollisionPair(selfID, other, pairs,tableIds);
        }
      }
    }
  }
  //_addCollisionPair modifies the index rows by themselves, after which this single resize fills in the rest of the rows
  TableOperations::resizeTable(pairs, std::get<CollisionPairIndexA>(pairs.mRows).size());
}

std::vector<float> DEBUG_HACK;

namespace notispc {
glm::vec2 transposeRotation(float cosAngle, float sinAngle) {
  //Rotation matrix is
  //[cos(x), -sin(x)]
  //[sin(x), cos(x)]
  //So the transpose given cos and sin is negating sin
  glm::vec2 result = { cosAngle, -sinAngle };
  return result;
}

//Multiply rotation matrices A*B represented by cos and sin since they're symmetric
glm::vec2 multiplyRotationMatrices(float cosAngleA, float sinAngleA, float cosAngleB, float sinAngleB) {
  //[cosAngleA, -sinAngleA]*[cosAngleB, -sinAngleB] = [cosAngleA*cosAngleB - sinAngleA*sinAngleB, ...]
  //[sinAngleA, cosAngleA]  [sinAngleB, cosAngleB]    [sinAngleA*cosAngleB + cosAngleA*sinAngleB, ...]
  glm::vec2 result = { cosAngleA*cosAngleB - sinAngleA*sinAngleB, sinAngleA*cosAngleB + cosAngleA*sinAngleB };
  return result;
}

//Multiply M*V where M is the rotation matrix represented by cos/sinangle and V is a vector
glm::vec2 multiplyVec2ByRotation(float cosAngle, float sinAngle, float vx, float vy) {
  //[cosAngle, -sinAngle]*[vx] = [cosAngle*vx - sinAngle*vy]
  //[sinAngle,  cosAngle] [vy]   [sinAngle*vx + cosAngle*vy]
  glm::vec2 result = { cosAngle*vx - sinAngle*vy, sinAngle*vx + cosAngle*vy };
  return result;
}

//Get the relative right represented by this rotation matrix, in other words the first basis vector (first column of matrix)
glm::vec2 getRightFromRotation(float cosAngle, float sinAngle) {
  //It already is the first column
  glm::vec2 result = { cosAngle, sinAngle };
  return result;
}

//Get the second basis vector (column)
glm::vec2 getUpFromRotation(float cosAngle, float sinAngle) {
  //[cosAngle, -sinAngle]
  //[sinAngle,  cosAngle]
  glm::vec2 result = { -sinAngle, cosAngle };
  return result;
}

void generateUnitCubeCubeContacts(
  ispc::UniformConstVec2& positionsA,
  ispc::UniformRotation& rotationsA,
  ispc::UniformConstVec2& positionsB,
  ispc::UniformRotation& rotationsB,
  ispc::UniformVec2& resultNormals,
  ispc::UniformContact& resultContactOne,
  ispc::UniformContact& resultContactTwo,
  float debug[],
  uint32_t count
) {
        int d = 0;

  const glm::vec2 aNormals[4] = { { 0.0f, 1.0f }, { 0.0f, -1.0f }, { 1.0f, 0.0f }, { -1.0f, 0.0f } };

  for(uint32_t i = 0; i < count; ++i) {
    //Transpose to undo the rotation of A
    const glm::vec2 rotA = { rotationsA.cosAngle[i], rotationsA.sinAngle[i] };
    const glm::vec2 rotAInverse = transposeRotation(rotA.x, rotA.y);
    const glm::vec2 posA = { positionsA.x[i], positionsA.y[i] };

    const glm::vec2 rotB = { rotationsB.cosAngle[i], rotationsB.sinAngle[i] };
    const glm::vec2 posB = { positionsB.x[i], positionsB.y[i] };
    //B's rotation in A's local space. Transforming to local space A allows this to be solved as computing
    //contacts between an AABB and an OBB instead of OBB to OBB
    const glm::vec2 rotBInA = multiplyRotationMatrices(rotB.x, rotB.y, rotAInverse.x, rotAInverse.y);
    //Get basis vectors with the lengths of B so that they go from the center to the extents
    const glm::vec2 upB = getUpFromRotation(rotBInA.x, rotBInA.y) * 0.5f;
    const glm::vec2 rightB = getRightFromRotation(rotBInA.x, rotBInA.y) * 0.5f;
    glm::vec2 posBInA = posB - posA;
    posBInA = multiplyVec2ByRotation(rotAInverse.x, rotAInverse.y, posBInA.x, posBInA.y);

    const float extentAX = 0.5f;
    const float extentAY = 0.5f;
    //Sutherland hodgman clipping of B in the space of A, meaning all the clipping planes are cardinal axes
    int outputCount = 0;
    //8 should be the maximum amount of points that can result from clipping a square against another, which is when they are inside each-other and all corners of one intersect the edges of the other
    glm::vec2 outputPoints[8];

    //Upper right, lower right, lower left, upper left
    outputPoints[0] = posBInA + upB + rightB;
    outputPoints[1] = posBInA - upB + rightB;
    outputPoints[2] = posBInA - upB - rightB;
    outputPoints[3] = posBInA + upB - rightB;
    outputCount = 4;

    //ispc prefers single floats even here to avoid "gather required to load value" performance warnings
    float inputPointsX[8];
    float inputPointsY[8];
    int inputCount = 0;
    float bestOverlap = 9999.0f;
    glm::vec2 bestNormal = aNormals[0];

    bool allPointsInside = true;
    for(int edgeA = 0; edgeA < 4; ++edgeA) {
      //Copy previous output to current input
      inputCount = outputCount;
      glm::vec2 lastPoint;
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
      glm::vec2 aNormal = aNormals[edgeA];

      //Last inside is invalidated when the edges change since it's inside relative to a given edge
      float lastOverlap = 0.5f - glm::dot(aNormal, lastPoint);
      bool lastInside = lastOverlap >= 0.0f;

      float currentEdgeOverlap = 0.0f;
      for(int j = 0; j < inputCount; ++j) {
        const glm::vec2 currentPoint = { inputPointsX[j], inputPointsY[j] };
        //(e-p).n
        const float currentOverlap = 0.5f - glm::dot(aNormal, currentPoint);
        const bool currentInside = currentOverlap >= 0;
        const glm::vec2 lastToCurrent = currentPoint - lastPoint;
        //Might be division by zero but if so the intersect won't be used because currentInside would match lastInside
        //(e-p).n/(e-s).n
        const float t = 1.0f - (abs(currentOverlap)/std::abs(glm::dot(aNormal, lastToCurrent)));
        const glm::vec2 intersect = lastPoint + lastToCurrent*t;

        currentEdgeOverlap = std::max(currentOverlap, currentEdgeOverlap);

        //TODO: re-use subtraction above in intersect calculation below
        if(currentInside) {
          allPointsInside = false;
          if(!lastInside) {
            //Went from outside to inside, add intersect
            outputPoints[outputCount] = intersect;
            ++outputCount;
          }
          //Is inside, add current
          outputPoints[outputCount] = currentPoint;
          ++outputCount;
        }
        else if(lastInside) {
          //Went from inside to outside, add intersect.
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
      glm::vec2 candidateNormals[3] = { bestNormal, getUpFromRotation(rotBInA.x, rotBInA.y), getRightFromRotation(rotBInA.x, rotBInA.y) };

      debug[d++] = posB.x;
      debug[d++] = posB.y;
      debug[d++] = posB.x + candidateNormals[1].x;
      debug[d++] = posB.y + candidateNormals[1].y;
      debug[d++] = posB.x;
      debug[d++] = posB.y;
      debug[d++] = posB.x + candidateNormals[2].x;
      debug[d++] = posB.y + candidateNormals[2].y;


      const glm::vec2 originalBestNormal = bestNormal;
      float bestNormalDiff = 99.0f;
      //Figuring out the best normal has two parts:
      // - Determining which results  in the least overlap along the normal, which is the distance between the two extremes of projections of all clipped points onto normal
      // - Determine the sign of the normal
      // This loop will do the former by determining all the projections on the normal and picking the normal that has the greatest difference
      // Then the result can be flipped so it's pointing in the same direction as the original best
      // This is a hacky assumption based on that either the original best will be chosen or one not far off from it
      for(int j = 0; j < 3; ++j) {
        float thisMin = 999.0f;
        float thisMax = -thisMin;
        glm::vec2 normal = candidateNormals[j];
        normal = glm::normalize(normal);
        float l = glm::length(normal);
        l;

        for(int k = 0; k < outputCount; ++k) {
          float thisOverlap = glm::dot(normal, outputPoints[k]);
          thisMin = std::min(thisMin, thisOverlap);
          thisMax = std::max(thisMax, thisOverlap);
        }
        //This is the absolute value of the total amount of overlap along this axis, we're looking for the normal with the smallest overlap
        const float thisNormalDiff = thisMax - thisMin;
        if(thisNormalDiff < bestNormalDiff) {
          bestNormalDiff = thisNormalDiff;
          bestNormal = normal;
        }
      }

      //Now the best normal is known, make sure it's pointing in a similar direction to the original
      if(glm::dot(originalBestNormal, bestNormal) < 0.0f) {
        bestNormal = -bestNormal;
      }

      //Now find the two best contact points. The normal is going away from A, so the smallest projection is the one with the most overlap, since it's going most against the normal
      //The overlap for any point is the distance of its projection from the greatest projection: the point furthest away from A
      glm::vec2 bestPoint{};
      glm::vec2 secondBestPoint;
      float minProjection = 999.0f;
      float secondMinProjection = minProjection;
      float maxProjection = -1;
      for(int j = 0; j < outputCount; ++j) {
        const glm::vec2 thisPoint = outputPoints[j];
        const float thisProjection = glm::dot(bestNormal, thisPoint);
        maxProjection = std::max(maxProjection, thisProjection);
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
      glm::vec2 contactOne = bestPoint;
      glm::vec2 contactTwo = secondBestPoint;
      float contactTwoOverlap = maxProjection - secondMinProjection;
      float contactOneOverlap = maxProjection - minProjection;

      //Transform the contacts back to world
      contactOne = posA + multiplyVec2ByRotation(rotA.x, rotA.y, contactOne.x, contactOne.y);
      contactTwo = posA + multiplyVec2ByRotation(rotA.x, rotA.y, contactTwo.x, contactTwo.y);

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

      for(int j = 0; j < outputCount; ++j) {
        glm::vec2& thisPoint = outputPoints[j];
        thisPoint = posA + multiplyVec2ByRotation(rotA.x, rotA.y, thisPoint.x, thisPoint.y);
      }
      debug[d++] = outputPoints[outputCount - 1].x;
      debug[d++] = outputPoints[outputCount - 1].y;
      for(int k = 0; k < outputCount; ++k) {
        debug[d++] = outputPoints[k].x;
        debug[d++] = outputPoints[k].y;
        debug[d++] = outputPoints[k].x;
        debug[d++] = outputPoints[k].y;
      }
      debug[d++] = outputPoints[0].x;
      debug[d++] = outputPoints[0].y;
    }
    debug[d] = 1000.0f;
  }
}
}

void Physics::generateContacts(CollisionPairsTable& pairs) {
  ispc::UniformConstVec2 positionsA{ _unwrapRow<NarrowphaseData<PairA>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairA>::PosY>(pairs) };
  ispc::UniformRotation rotationsA{ _unwrapRow<NarrowphaseData<PairA>::CosAngle>(pairs), _unwrapRow<NarrowphaseData<PairA>::SinAngle>(pairs) };
  ispc::UniformConstVec2 positionsB{ _unwrapRow<NarrowphaseData<PairB>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairB>::PosY>(pairs) };
  ispc::UniformRotation rotationsB{ _unwrapRow<NarrowphaseData<PairB>::CosAngle>(pairs), _unwrapRow<NarrowphaseData<PairB>::SinAngle>(pairs) };
  ispc::UniformVec2 normals{ std::get<SharedNormal::X>(pairs.mRows).mElements.data(), std::get<SharedNormal::Y>(pairs.mRows).mElements.data() };

  ispc::UniformContact contactsOne{
    _unwrapRow<ContactPoint<ContactOne>::PosX>(pairs),
    _unwrapRow<ContactPoint<ContactOne>::PosY>(pairs),
    _unwrapRow<ContactPoint<ContactOne>::Overlap>(pairs)
  };
  ispc::UniformContact contactsTwo{
    _unwrapRow<ContactPoint<ContactTwo>::PosX>(pairs),
    _unwrapRow<ContactPoint<ContactTwo>::PosY>(pairs),
    _unwrapRow<ContactPoint<ContactTwo>::Overlap>(pairs)
  };
  //DEBUG_HACK.resize(1000);
  ispc::generateUnitCubeCubeContacts(positionsA, rotationsA, positionsB, rotationsB, normals, contactsOne, contactsTwo, uint32_t(TableOperations::size(pairs)));
  //ispc::generateUnitSphereSphereContacts(positionsA, positionsB, normals, contacts, uint32_t(TableOperations::size(pairs)));

  //ispc::UniformConstVec2 positionsA{ _unwrapRow<NarrowphaseData<PairA>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairA>::PosY>(pairs) };
  //ispc::UniformConstVec2 positionsB{ _unwrapRow<NarrowphaseData<PairB>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairB>::PosY>(pairs) };
  //ispc::UniformVec2 normals{ std::get<SharedNormal::X>(pairs.mRows).mElements.data(), std::get<SharedNormal::Y>(pairs.mRows).mElements.data() };
  //ispc::UniformContact contacts{
  //  _unwrapRow<ContactPoint<ContactOne>::PosX>(pairs),
  //  _unwrapRow<ContactPoint<ContactOne>::PosY>(pairs),
  //  _unwrapRow<ContactPoint<ContactOne>::Overlap>(pairs)
  //};
  //ispc::generateUnitSphereSphereContacts(positionsA, positionsB, normals, contacts, uint32_t(TableOperations::size(pairs)));
}

template<class PosRowA, class PosRowB, class ContactRow, class DstRowA, class DstRowB>
void _toRVector(CollisionPairsTable& pairs, ConstraintsTable& constraints) {
  PosRowA& posA = std::get<PosRowA>(pairs.mRows);
  PosRowB& posB = std::get<PosRowB>(pairs.mRows);
  ContactRow& contacts = std::get<ContactRow>(pairs.mRows);
  DstRowA& dstA = std::get<DstRowA>(constraints.mRows);
  DstRowB& dstB = std::get<DstRowB>(constraints.mRows);

  ispc::turnContactsToRVectors(
    posA.mElements.data(),
    posB.mElements.data(),
    contacts.mElements.data(),
    dstA.mElements.data(),
    dstB.mElements.data(),
    uint32_t(TableOperations::size(pairs)));
}

struct ConstraintSyncData {
  template<class T, class TableT>
  T* _tryGetRow(TableT& table) {
    if constexpr(TableOperations::hasRow<T, TableT>()) {
      return &std::get<T>(table.mRows);
    }
    else {
      return nullptr;
    }
  }

  template<class TableT>
  ConstraintSyncData(CollisionPairsTable& pairs, ConstraintCommonTable& common, TableT& constraints)
    : mSyncIndexA(std::get<ConstraintObject<ConstraintObjA>::SyncIndex>(common.mRows))
    , mSyncTypeA(std::get<ConstraintObject<ConstraintObjA>::SyncType>(common.mRows))
    , mPairIndexA(std::get<CollisionPairIndexA>(common.mRows))
    , mCenterToContactOneXA(_tryGetRow<ConstraintObject<ConstraintObjA>::CenterToContactOneX>(constraints))
    , mCenterToContactOneYA(_tryGetRow<ConstraintObject<ConstraintObjA>::CenterToContactOneY>(constraints))
    , mCenterToContactTwoXA(_tryGetRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoX>(constraints))
    , mCenterToContactTwoYA(_tryGetRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoY>(constraints))
    , mSyncIndexB(std::get<ConstraintObject<ConstraintObjB>::SyncIndex>(common.mRows))
    , mSyncTypeB(std::get<ConstraintObject<ConstraintObjB>::SyncType>(common.mRows))
    , mPairIndexB(std::get<CollisionPairIndexB>(common.mRows))
    , mCenterToContactOneXB(_tryGetRow<ConstraintObject<ConstraintObjB>::CenterToContactOneX>(constraints))
    , mCenterToContactOneYB(_tryGetRow<ConstraintObject<ConstraintObjB>::CenterToContactOneY>(constraints))
    , mCenterToContactTwoXB(_tryGetRow<ConstraintObject<ConstraintObjB>::CenterToContactTwoX>(constraints))
    , mCenterToContactTwoYB(_tryGetRow<ConstraintObject<ConstraintObjB>::CenterToContactTwoY>(constraints))
    , mDestContactOneOverlap(_tryGetRow<ContactPoint<ContactOne>::Overlap>(constraints))
    , mDestContactTwoOverlap(_tryGetRow<ContactPoint<ContactTwo>::Overlap>(constraints))
    , mDestNormalX(_tryGetRow<SharedNormal::X>(constraints))
    , mDestNormalY(_tryGetRow<SharedNormal::Y>(constraints))
    , mContactOneX(_tryGetRow<ContactPoint<ContactOne>::PosX>(pairs))
    , mContactOneY(_tryGetRow<ContactPoint<ContactOne>::PosY>(pairs))
    , mContactTwoX(_tryGetRow<ContactPoint<ContactTwo>::PosX>(pairs))
    , mContactTwoY(_tryGetRow<ContactPoint<ContactTwo>::PosY>(pairs))
    , mPosAX(_tryGetRow<NarrowphaseData<PairA>::PosX>(pairs))
    , mPosAY(_tryGetRow<NarrowphaseData<PairA>::PosY>(pairs))
    , mPosBX(_tryGetRow<NarrowphaseData<PairB>::PosX>(pairs))
    , mPosBY(_tryGetRow<NarrowphaseData<PairB>::PosY>(pairs))
    , mSourceContactOneOverlap(_tryGetRow<ContactPoint<ContactOne>::Overlap>(pairs))
    , mSourceContactTwoOverlap(_tryGetRow<ContactPoint<ContactTwo>::Overlap>(pairs))
    , mSourceNormalX(_tryGetRow<SharedNormal::X>(pairs))
    , mSourceNormalY(_tryGetRow<SharedNormal::Y>(pairs)) {
  }

  ConstraintObject<ConstraintObjA>::SyncIndex& mSyncIndexA;
  ConstraintObject<ConstraintObjA>::SyncType& mSyncTypeA;
  CollisionPairIndexA& mPairIndexA;
  ConstraintObject<ConstraintObjB>::SyncIndex& mSyncIndexB;
  ConstraintObject<ConstraintObjB>::SyncType& mSyncTypeB;
  CollisionPairIndexB& mPairIndexB;

  ConstraintObject<ConstraintObjA>::CenterToContactOneX* mCenterToContactOneXA{};
  ConstraintObject<ConstraintObjA>::CenterToContactOneY* mCenterToContactOneYA{};
  ConstraintObject<ConstraintObjA>::CenterToContactTwoX* mCenterToContactTwoXA{};
  ConstraintObject<ConstraintObjA>::CenterToContactTwoY* mCenterToContactTwoYA{};

  ConstraintObject<ConstraintObjB>::CenterToContactOneX* mCenterToContactOneXB{};
  ConstraintObject<ConstraintObjB>::CenterToContactOneY* mCenterToContactOneYB{};
  ConstraintObject<ConstraintObjB>::CenterToContactTwoX* mCenterToContactTwoXB{};
  ConstraintObject<ConstraintObjB>::CenterToContactTwoY* mCenterToContactTwoYB{};

  ContactPoint<ContactOne>::Overlap* mDestContactOneOverlap{};
  ContactPoint<ContactTwo>::Overlap* mDestContactTwoOverlap{};
  SharedNormal::X* mDestNormalX{};
  SharedNormal::Y* mDestNormalY{};

  ContactPoint<ContactOne>::PosX* mContactOneX{};
  ContactPoint<ContactOne>::PosY* mContactOneY{};
  ContactPoint<ContactTwo>::PosX* mContactTwoX{};
  ContactPoint<ContactTwo>::PosY* mContactTwoY{};

  NarrowphaseData<PairA>::PosX* mPosAX{};
  NarrowphaseData<PairA>::PosY* mPosAY{};
  NarrowphaseData<PairB>::PosX* mPosBX{};
  NarrowphaseData<PairB>::PosY* mPosBY{};

  ContactPoint<ContactOne>::Overlap* mSourceContactOneOverlap{};
  ContactPoint<ContactTwo>::Overlap* mSourceContactTwoOverlap{};
  SharedNormal::X* mSourceNormalX{};
  SharedNormal::Y* mSourceNormalY{};
};

void _syncConstraintData(ConstraintSyncData& data, size_t globalConstraintIndex, size_t constraintIndex, size_t objectIndex, size_t indexA, size_t indexB) {
  data.mPairIndexA.at(globalConstraintIndex) = indexA;

  //Assume that if one row of A exist they all do
  if(data.mCenterToContactOneXA) {
    data.mCenterToContactOneXA->at(constraintIndex) = data.mContactOneX->at(objectIndex) - data.mPosAX->at(objectIndex);
    data.mCenterToContactOneYA->at(constraintIndex) = data.mContactOneY->at(objectIndex) - data.mPosAY->at(objectIndex);
    data.mCenterToContactTwoXA->at(constraintIndex) = data.mContactTwoX->at(objectIndex) - data.mPosAX->at(objectIndex);
    data.mCenterToContactTwoYA->at(constraintIndex) = data.mContactTwoY->at(objectIndex) - data.mPosAY->at(objectIndex);
  }

  data.mDestContactOneOverlap->at(constraintIndex) = data.mSourceContactOneOverlap->at(objectIndex);
  data.mDestContactTwoOverlap->at(constraintIndex) = data.mSourceContactTwoOverlap->at(objectIndex);
  data.mDestNormalX->at(constraintIndex) = data.mSourceNormalX->at(objectIndex);
  data.mDestNormalY->at(constraintIndex) = data.mSourceNormalY->at(objectIndex);

  data.mPairIndexB.at(globalConstraintIndex) = indexB;
  if(data.mCenterToContactOneXB) {
    data.mCenterToContactOneXB->at(constraintIndex) = data.mContactOneX->at(objectIndex) - data.mPosBX->at(objectIndex);
    data.mCenterToContactOneYB->at(constraintIndex) = data.mContactOneY->at(objectIndex) - data.mPosBY->at(objectIndex);
    data.mCenterToContactTwoXB->at(constraintIndex) = data.mContactTwoX->at(objectIndex) - data.mPosBX->at(objectIndex);
    data.mCenterToContactTwoYB->at(constraintIndex) = data.mContactTwoY->at(objectIndex) - data.mPosBY->at(objectIndex);
  }
}

struct VisitAttempt {
  size_t mDesiredObjectIndex{};
  size_t mDesiredConstraintIndex{};
  std::vector<ConstraintData::VisitData>::iterator mIt;
  size_t mRequiredPadding{};
};

void _setVisitDataAndTrySetSyncPoint(std::vector<ConstraintData::VisitData>& visited, VisitAttempt& attempt,
  ConstraintObject<ConstraintObjA>::SyncIndex& syncIndexA,
  ConstraintObject<ConstraintObjA>::SyncType& syncTypeA,
  ConstraintObject<ConstraintObjB>::SyncIndex& syncIndexB,
  ConstraintObject<ConstraintObjB>::SyncType& syncTypeB,
  ConstraintData::VisitData::Location location,
  VisitAttempt* dependentAttempt) {
  //Set it to nosync for now, later iteration might set this as new constraints are visited
  syncTypeA.at(attempt.mDesiredConstraintIndex) = ispc::NoSync;
  syncTypeB.at(attempt.mDesiredConstraintIndex) = ispc::NoSync;
  if(attempt.mIt == visited.end() || attempt.mIt->mObjectIndex != attempt.mDesiredObjectIndex) {
    //Goofy hack here, if there's another iterator for object B, make sure the iterator is still valid after the new element is inserted
    //Needs to be rebuilt if it is after the insert location, as everything would have shifted over
    const bool needRebuiltDependent = dependentAttempt && dependentAttempt->mDesiredObjectIndex > attempt.mDesiredObjectIndex;

    //If this is the first time visiting this object, no need to sync anything, but note it for later
    visited.insert(attempt.mIt, ConstraintData::VisitData{
      attempt.mDesiredObjectIndex,
      attempt.mDesiredConstraintIndex,
      location,
      attempt.mDesiredConstraintIndex,
      location
    });

    if(needRebuiltDependent) {
      dependentAttempt->mIt = std::lower_bound(visited.begin(), visited.end(), dependentAttempt->mDesiredObjectIndex);
    }
  }
  else {
    //A has been visited before, add a sync index
    const int newLocation = location == ConstraintData::VisitData::Location::InA ? ispc::SyncToIndexA : ispc::SyncToIndexB;
    //Make the previously visited constraint publish the velocity forward to this one
    switch(attempt.mIt->mLocation) {
      case ConstraintData::VisitData::Location::InA:
        syncTypeA.at(attempt.mIt->mConstraintIndex) = newLocation;
        syncIndexA.at(attempt.mIt->mConstraintIndex) = attempt.mDesiredConstraintIndex;
        break;
      case ConstraintData::VisitData::Location::InB:
        syncTypeB.at(attempt.mIt->mConstraintIndex) = newLocation;
        syncIndexB.at(attempt.mIt->mConstraintIndex) = attempt.mDesiredConstraintIndex;
        break;
    }
    //Now that the latest instance of this object is at this visit location, update the visit data
    attempt.mIt->mConstraintIndex = attempt.mDesiredConstraintIndex;
    attempt.mIt->mLocation = location;
  }
}

//If an object shows up in multiple constraints, sync the velocity data from the last constraint back to the first
void _trySetFinalSyncPoint(const ConstraintData::VisitData& visited,
  ConstraintObject<ConstraintObjA>::SyncIndex& syncIndexA,
  ConstraintObject<ConstraintObjA>::SyncType& syncTypeA,
  ConstraintObject<ConstraintObjB>::SyncIndex& syncIndexB,
  ConstraintObject<ConstraintObjB>::SyncType& syncTypeB) {
  //If the first is the latest, there is only one instance of this object meaning its velocity was never copied
  if(visited.mFirstConstraintIndex == visited.mConstraintIndex) {
    return;
  }

  //The container to sync from, meaning the final visited entry where the most recent velocity is
  std::vector<int>* syncFromIndex = &syncIndexA.mElements;
  std::vector<int>* syncFromType = &syncTypeA.mElements;
  if(visited.mLocation == ConstraintData::VisitData::Location::InB) {
    syncFromIndex = &syncIndexB.mElements;
    syncFromType = &syncTypeB.mElements;
  }

  //Sync from this visited entry back to the first element
  syncFromIndex->at(visited.mConstraintIndex) = visited.mFirstConstraintIndex;
  //Write from the location of the final entry to the location of the first
  switch(visited.mFirstLocation) {
    case ConstraintData::VisitData::Location::InA: syncFromType->at(visited.mConstraintIndex) = ispc::SyncToIndexA; break;
    case ConstraintData::VisitData::Location::InB: syncFromType->at(visited.mConstraintIndex) = ispc::SyncToIndexB; break;
  }
}

VisitAttempt _tryVisit(std::vector<ConstraintData::VisitData>& visited, size_t toVisit, size_t currentConstraintIndex, size_t targetWidth) {
  VisitAttempt result;
  result.mIt = std::lower_bound(visited.begin(), visited.end(), toVisit);
  if(result.mIt != visited.end() && result.mIt->mObjectIndex == toVisit && 
    (currentConstraintIndex - result.mIt->mConstraintIndex) < targetWidth) {
    result.mRequiredPadding = targetWidth - (currentConstraintIndex - result.mIt->mConstraintIndex);
  }
  result.mDesiredConstraintIndex = currentConstraintIndex;
  result.mDesiredObjectIndex = toVisit;
  return result;
}

//TODO: what if all the constraints were block solved? If they were solved in blocks as wide as simd lanes then
//maybe this complicated shuffling wouldn't be needed. Or maybe even a giant matrix for all objects

void Physics::buildConstraintsTable(
  CollisionPairsTable& pairs,
  ConstraintsTable& constraints,
  ContactConstraintsToStaticObjectsTable& staticConstraints,
  ConstraintCommonTable& constraintsCommon,
  const PhysicsTableIds& tableIds) {
  const size_t tableIDMask = tableIds.mTableIDMask;
  const size_t sharedMassTableId = tableIds.mSharedMassTable;
  const size_t zeroMassTableId = tableIds.mZeroMassTable;

  //TODO: only rebuild if collision pairs changed. Could be really lazy about it since solving stale constraints should result in no changes to velocity
  //Clear before resize. Shouldn't be necessary but there seems to be a bug somewhere
  TableOperations::resizeTable(constraints, 0);
  TableOperations::resizeTable(staticConstraints, 0);
  TableOperations::resizeTable(constraintsCommon, 0);
  TableOperations::resizeTable(constraints, TableOperations::size(pairs));
  TableOperations::resizeTable(staticConstraints, TableOperations::size(pairs));
  TableOperations::resizeTable(constraintsCommon, TableOperations::size(pairs));

  ConstraintData::SharedVisitData& visitData = std::get<ConstraintData::SharedVisitDataRow>(constraintsCommon.mRows).at();
  std::vector<ConstraintData::VisitData>& visited = visitData.mVisited;
  visited.clear();
  //Need to reserve big enough because visited vector growth would cause iterator invalidation for visitA/visitB cases below
  //Size of pairs is bigger than necessary, it only needs to be the number of objects, but that's not known here and over-allocating
  //a bit isn't a problem
  visited.reserve(TableOperations::size(pairs));
  ConstraintSyncData data(pairs, constraintsCommon, constraints);

  //Specifically from the collision table not the constraints table, to be used before the indices in the collision table have been created yet
  auto& srcPairIndexA = std::get<CollisionPairIndexA>(pairs.mRows);
  auto& srcPairIndexB = std::get<CollisionPairIndexB>(pairs.mRows);

  //The same object can't be within this many indices of itself since the velocity needs to be seen immediately by the next constraint
  //which wouldn't be the case if it was being solved in another simd lane
  const size_t targetWidth = size_t(ispc::getTargetWidth());
  //Figure out sync indices
  std::deque<size_t>& indicesToFill = visitData.mIndicesToFill;
  std::deque<size_t>& nextToFill = visitData.mNextToFill;
  nextToFill.clear();
  indicesToFill.resize(TableOperations::size(pairs));
  for(size_t i = 0; i < TableOperations::size(pairs); ++i) {
    indicesToFill[i] = i;
  }
  size_t currentConstraintIndex = 0;
  size_t failedPlacements = 0;
  //First fill shared mass pairs
  //Fill each element in the constraints table one by one, trying the latest in indicesToFill each type
  //If it fails, swap it to back and hope it works later. If it doesn't work this will break out with remaining indices
  const size_t sharedMassStart = currentConstraintIndex;
  std::get<ConstraintData::CommonTableStartIndex>(constraints.mRows).at() = sharedMassStart;
  while(!indicesToFill.empty()) {
    const size_t indexToFill = indicesToFill.front();
    indicesToFill.pop_front();
    const size_t desiredA = srcPairIndexA.at(indexToFill);
    const size_t desiredB = srcPairIndexB.at(indexToFill);
    //This loop is for shared mass, store the rest for the next pass
    //TODO: this is a bit weird. != shared mass would be nice but that means players get excluded
    if((desiredA & tableIDMask) == zeroMassTableId || (desiredB & tableIDMask) == zeroMassTableId) {
      nextToFill.push_back(indexToFill);
      continue;
    }

    VisitAttempt visitA = _tryVisit(visited, desiredA, currentConstraintIndex, targetWidth);
    VisitAttempt visitB = _tryVisit(visited, desiredB, currentConstraintIndex, targetWidth);

    //If this can't be filled now keep going and hopefully this will work later
    if(visitA.mRequiredPadding || visitB.mRequiredPadding) {
      indicesToFill.push_back(indexToFill);
      //If the last few are impossible to fill this way, break
      if(++failedPlacements >= indicesToFill.size()) {
        break;
      }
      continue;
    }

    _syncConstraintData(data, currentConstraintIndex, currentConstraintIndex - sharedMassStart, indexToFill, desiredA, desiredB);

    _setVisitDataAndTrySetSyncPoint(visited, visitA, data.mSyncIndexA, data.mSyncTypeA, data.mSyncIndexB, data.mSyncTypeB, ConstraintData::VisitData::Location::InA, &visitB);
    _setVisitDataAndTrySetSyncPoint(visited, visitB, data.mSyncIndexA, data.mSyncTypeA, data.mSyncIndexB, data.mSyncTypeB, ConstraintData::VisitData::Location::InB, nullptr);

    ++currentConstraintIndex;
    failedPlacements = 0;
  }

  //If elements remain here it means there wasn't an order of elements possible as-is.
  //Add padding between these remaining elements to solve the problem.
  while(!indicesToFill.empty()) {
    const size_t indexToFill = indicesToFill.front();
    const size_t desiredA = srcPairIndexA.at(indexToFill);
    const size_t desiredB = srcPairIndexB.at(indexToFill);
    VisitAttempt visitA = _tryVisit(visited, desiredA, currentConstraintIndex, targetWidth);
    VisitAttempt visitB = _tryVisit(visited, desiredB, currentConstraintIndex, targetWidth);
    const size_t padding = std::max(visitA.mRequiredPadding, visitB.mRequiredPadding);

    //If no padding is required that means the previous iteration made space, add the constraint here
    if(!padding) {
      _syncConstraintData(data, currentConstraintIndex, currentConstraintIndex - sharedMassStart, indexToFill, desiredA, desiredB);
      _setVisitDataAndTrySetSyncPoint(visited, visitA, data.mSyncIndexA, data.mSyncTypeA, data.mSyncIndexB, data.mSyncTypeB, ConstraintData::VisitData::Location::InA, &visitB);
      _setVisitDataAndTrySetSyncPoint(visited, visitB, data.mSyncIndexA, data.mSyncTypeA, data.mSyncIndexB, data.mSyncTypeB, ConstraintData::VisitData::Location::InB, nullptr);
      indicesToFill.pop_front();
      ++currentConstraintIndex;
    }
    //Space needs to be made, add padding then let the iteration contine which will attempt the index again with the new space
    else {
      TableOperations::resizeTable(constraints, TableOperations::size(constraints) + padding);
      TableOperations::resizeTable(constraintsCommon, TableOperations::size(constraintsCommon) + padding);
      for(size_t i = 0; i < padding; ++i) {
        //These are nonsense entires that won't be used for anything, at least set them to no sync so they don't try to copy stale data
        data.mSyncTypeA.at(currentConstraintIndex) = ispc::NoSync;
        data.mSyncTypeB.at(currentConstraintIndex) = ispc::NoSync;
        ++currentConstraintIndex;
      }
    }
  }
  //Trim off unused entries
  TableOperations::resizeTable(constraints, currentConstraintIndex - sharedMassStart);

  //Fill contacts with static objects
  const size_t zeroMassStart = currentConstraintIndex;
  std::get<ConstraintData::CommonTableStartIndex>(staticConstraints.mRows).at() = zeroMassStart;
  ConstraintSyncData staticData(pairs, constraintsCommon, staticConstraints);
  indicesToFill = nextToFill;
  nextToFill.clear();
  while(!indicesToFill.empty()) {
    const size_t indexToFill = indicesToFill.front();
    indicesToFill.pop_front();
    size_t desiredA = srcPairIndexA.at(indexToFill);
    size_t desiredB = srcPairIndexB.at(indexToFill);
    //This loop is for zero mass, store the rest for the next pass
    const bool aZeroMass = (desiredA & tableIDMask) == zeroMassTableId;
    const bool bZeroMass = (desiredB & tableIDMask) == zeroMassTableId;
    //If neither is zero mass, push them to the next phase
    if(!aZeroMass && !bZeroMass) {
      nextToFill.push_back(indexToFill);
      continue;
    }
    assert(!aZeroMass && "Pair generation should have ensured zero mass object is always B");

    //No visit of static B needed because no velocity syncing is needed
    VisitAttempt visitA = _tryVisit(visited, desiredA, currentConstraintIndex, targetWidth);

    //If this can't be filled now keep going and hopefully this will work later
    //No padding needed for B because its velocity will always be zero
    if(visitA.mRequiredPadding) {
      indicesToFill.push_back(indexToFill);
      //If the last few are impossible to fill this way, break
      if(++failedPlacements >= indicesToFill.size()) {
        break;
      }
      continue;
    }

    _syncConstraintData(staticData, currentConstraintIndex, currentConstraintIndex - zeroMassStart, indexToFill, desiredA, desiredB);

    _setVisitDataAndTrySetSyncPoint(visited, visitA, staticData.mSyncIndexA, staticData.mSyncTypeA, staticData.mSyncIndexB, staticData.mSyncTypeB, ConstraintData::VisitData::Location::InA, nullptr);
    staticData.mSyncTypeB.at(currentConstraintIndex) = ispc::NoSync;

    ++currentConstraintIndex;
    failedPlacements = 0;
  }

  assert(nextToFill.empty() && "No other categories implemented");

  //If elements remain here it means there wasn't an order of elements possible as-is.
  //Add padding between these remaining elements to solve the problem.
  while(!indicesToFill.empty()) {
    const size_t indexToFill = indicesToFill.front();
    const size_t desiredA = srcPairIndexA.at(indexToFill);
    const size_t desiredB = srcPairIndexB.at(indexToFill);
    VisitAttempt visitA = _tryVisit(visited, desiredA, currentConstraintIndex, targetWidth);
    const size_t padding = visitA.mRequiredPadding;

    //If no padding is required that means the previous iteration made space, add the constraint here
    if(!padding) {
      _syncConstraintData(staticData, currentConstraintIndex, currentConstraintIndex - zeroMassStart, indexToFill, desiredA, desiredB);
      _setVisitDataAndTrySetSyncPoint(visited, visitA, staticData.mSyncIndexA, staticData.mSyncTypeA, staticData.mSyncIndexB, staticData.mSyncTypeB, ConstraintData::VisitData::Location::InA, nullptr);
      staticData.mSyncTypeB.at(currentConstraintIndex) = ispc::NoSync;
      indicesToFill.pop_front();
      ++currentConstraintIndex;
    }
    //Space needs to be made, add padding then let the iteration contine which will attempt the index again with the new space
    else {
      TableOperations::resizeTable(staticConstraints, TableOperations::size(staticConstraints) + padding);
      TableOperations::resizeTable(constraintsCommon, TableOperations::size(constraintsCommon) + padding);
      for(size_t i = 0; i < padding; ++i) {
        //These are nonsense entires that won't be used for anything, at least set them to no sync so they don't try to copy stale data
        staticData.mSyncTypeA.at(currentConstraintIndex) = ispc::NoSync;
        staticData.mSyncTypeB.at(currentConstraintIndex) = ispc::NoSync;
        ++currentConstraintIndex;
      }
    }
  }
  TableOperations::resizeTable(staticConstraints, currentConstraintIndex - zeroMassStart);

  //Store the final indices that the velocity will end up in
  //This is in the visited data since that's been tracking every access
  FinalSyncIndices& finalData = std::get<SharedRow<FinalSyncIndices>>(constraintsCommon.mRows).at();
  finalData.mMappingsA.clear();
  finalData.mMappingsB.clear();
  for(const ConstraintData::VisitData& v : visited) {
    //Link the final constraint entry back to the velocity data of the first that uses the objects if velocity was duplicated
    //This matters from one iteration to the next to avoid working on stale data, doesn't matter for the last iteration
    //since the final results will be copied from the end locations stored in mappings below
    _trySetFinalSyncPoint(v, data.mSyncIndexA, data.mSyncTypeA, data.mSyncIndexB, data.mSyncTypeB);

    //Store the final location mapping
    switch(v.mLocation) {
      case ConstraintData::VisitData::Location::InA: {
        finalData.mMappingsA.emplace_back(FinalSyncIndices::Mapping{ v.mObjectIndex, v.mConstraintIndex });
        break;
      }
      case ConstraintData::VisitData::Location::InB: {
        finalData.mMappingsB.emplace_back(FinalSyncIndices::Mapping{ v.mObjectIndex, v.mConstraintIndex });
        break;
      }
    }
  }
}

void Physics::setupConstraints(ConstraintsTable& constraints, ContactConstraintsToStaticObjectsTable& staticContacts) {
  //Currently computing as square
  //const float pi = 3.14159265359f;
  //const float r = 0.5f;
  //const float density = 1.0f;
  //const float mass = pi*r*r;
  //const float inertia = pi*(r*r*r*r)*0.25f;
  const float w = 1.0f;
  const float h = 1.0f;
  const float mass = w*h;
  const float inertia = mass*(h*h + w*w)/12.0f;
  const float invMass = 1.0f/mass;
  const float invInertia = 1.0f/inertia;
  const float bias = 0.1f;
  ispc::UniformVec2 normal{ _unwrapRow<SharedNormal::X>(constraints), _unwrapRow<SharedNormal::Y>(constraints) };
  ispc::UniformVec2 aToContactOne{ _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactOneX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactOneY>(constraints) };
  ispc::UniformVec2 bToContactOne{ _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactOneX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactOneY>(constraints) };
  ispc::UniformVec2 aToContactTwo{ _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoY>(constraints) };
  ispc::UniformVec2 bToContactTwo{ _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactTwoX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactTwoY>(constraints) };
  float* overlapOne = _unwrapRow<ContactPoint<ContactOne>::Overlap>(constraints);
  float* overlapTwo = _unwrapRow<ContactPoint<ContactTwo>::Overlap>(constraints);
  ispc::UniformContactConstraintPairData data = _unwrapUniformConstraintData(constraints);

  //TODO: don't clear this here and use it for warm start
  float* sumsOne = _unwrapRow<ConstraintData::LambdaSumOne>(constraints);
  float* sumsTwo = _unwrapRow<ConstraintData::LambdaSumTwo>(constraints);
  float* frictionSumsOne = _unwrapRow<ConstraintData::FrictionLambdaSumOne>(constraints);
  float* frictionSumsTwo = _unwrapRow<ConstraintData::FrictionLambdaSumTwo>(constraints);
  for(size_t i = 0; i < TableOperations::size(constraints); ++i) {
    sumsOne[i] = sumsTwo[i] = frictionSumsOne[i] = frictionSumsTwo[i] = 0.0f;
  }

  ispc::setupConstraintsSharedMass(invMass, invInertia, bias, normal, aToContactOne, aToContactTwo, bToContactOne, bToContactTwo, overlapOne, overlapTwo, data, uint32_t(TableOperations::size(constraints)));

  normal = { _unwrapRow<SharedNormal::X>(staticContacts), _unwrapRow<SharedNormal::Y>(staticContacts) };
  aToContactOne = { _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactOneX>(staticContacts), _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactOneY>(staticContacts) };
  aToContactTwo = { _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoX>(staticContacts), _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactTwoY>(staticContacts) };
  overlapOne = _unwrapRow<ContactPoint<ContactOne>::Overlap>(staticContacts);
  overlapTwo = _unwrapRow<ContactPoint<ContactTwo>::Overlap>(staticContacts);
  data = _unwrapUniformConstraintData(staticContacts);

  sumsOne = _unwrapRow<ConstraintData::LambdaSumOne>(staticContacts);
  sumsTwo = _unwrapRow<ConstraintData::LambdaSumTwo>(staticContacts);
  frictionSumsOne = _unwrapRow<ConstraintData::FrictionLambdaSumOne>(staticContacts);
  frictionSumsTwo = _unwrapRow<ConstraintData::FrictionLambdaSumTwo>(staticContacts);
  for(size_t i = 0; i < TableOperations::size(staticContacts); ++i) {
    sumsOne[i] = sumsTwo[i] = frictionSumsOne[i] = frictionSumsTwo[i] = 0.0f;
  }

  ispc::setupConstraintsSharedMassBZeroMass(invMass, invInertia, bias, normal, aToContactOne, aToContactTwo, overlapOne, overlapTwo, data, uint32_t(TableOperations::size(staticContacts)));
}

void Physics::solveConstraints(ConstraintsTable& constraints, ContactConstraintsToStaticObjectsTable& staticContacts, ConstraintCommonTable& common) {
  ispc::UniformContactConstraintPairData data = _unwrapUniformConstraintData(constraints);
  ispc::UniformConstraintObject objectA = _unwrapUniformConstraintObject<ConstraintObjA>(common);
  ispc::UniformConstraintObject objectB = _unwrapUniformConstraintObject<ConstraintObjB>(common);
  float* lambdaSumOne = _unwrapRow<ConstraintData::LambdaSumOne>(constraints);
  float* lambdaSumTwo = _unwrapRow<ConstraintData::LambdaSumTwo>(constraints);
  float* frictionLambdaSumOne = _unwrapRow<ConstraintData::FrictionLambdaSumOne>(constraints);
  float* frictionLambdaSumTwo = _unwrapRow<ConstraintData::FrictionLambdaSumTwo>(constraints);

  const float frictionCoeff = 0.8f;
  const size_t startContact = std::get<ConstraintData::CommonTableStartIndex>(constraints.mRows).at();
  const size_t startStatic = std::get<ConstraintData::CommonTableStartIndex>(staticContacts.mRows).at();

  ispc::solveContactConstraints(data, objectA, objectB, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, frictionCoeff, uint32_t(startContact), uint32_t(TableOperations::size(constraints)));

  data = _unwrapUniformConstraintData(staticContacts);
  lambdaSumOne = _unwrapRow<ConstraintData::LambdaSumOne>(constraints);
  lambdaSumTwo = _unwrapRow<ConstraintData::LambdaSumTwo>(constraints);
  frictionLambdaSumOne = _unwrapRow<ConstraintData::FrictionLambdaSumOne>(constraints);
  frictionLambdaSumTwo = _unwrapRow<ConstraintData::FrictionLambdaSumTwo>(constraints);

  ispc::solveContactConstraintsBZeroMass(data, objectA, lambdaSumOne, lambdaSumTwo, frictionLambdaSumOne, frictionLambdaSumTwo, frictionCoeff, uint32_t(startStatic), uint32_t(TableOperations::size(staticContacts)));
}
