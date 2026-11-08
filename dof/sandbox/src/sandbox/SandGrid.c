#include <sandbox/SandGrid.h>

#include <std/Diagnostics.h>
#include <clm/vec2.h>
#include <sandbox/SandGridImpulseIterator.h>
#include <sandbox/GrainStack.h>
#include <float.h>
#include <clm/quadrant.h>

const float GRAIN_DAMPING = 0.99f;
const float GRAIN_MOVE_EPSILON = 1.f;
const float ISLAND_SOLVE_THRESHOLD = 0.001f;

struct sbx_CollisionNeighbors {
  //Up to 3 neighbors as indicated by sbx_CollisionRegion with 0 indicating the end of the sequence.
  int32_t indices[3];
  //Axis into the vec2, so 0 or 1, of the normal pointing from the neighbor to the original
  uint8_t normalAxis[3];
  //1 or -1 for direction of normal, normal is `n[normalAxis] = normalDirection`
  int8_t normalDirection[3];
};
typedef struct sbx_CollisionNeighbors sbx_CollisionNeighbors;

void sbx_buildCollisionNeighbors(sbx_CollisionNeighbors result[CLM_QUADRANT_COUNT], int32_t stride) {
  const int32_t n = -stride;
  const int32_t s = stride;
  const int32_t e = 1;
  const int32_t w = -1;
  const uint8_t x = 0;
  const uint8_t y = 1;
  const int8_t pos = 1;
  const int8_t neg = -1;
  result[CLM_QUADRANT_INSIDE] = (sbx_CollisionNeighbors){ 0 };
  // Diagonals favor Y
  result[CLM_QUADRANT_NW] = (sbx_CollisionNeighbors){
    .indices = { n + w, n, w },
    .normalAxis = { y, y, x },
    .normalDirection = { neg, neg, pos }
  };
  result[CLM_QUADRANT_N] = (sbx_CollisionNeighbors){
    .indices = { n },
    .normalAxis = { y },
    .normalDirection = { neg }
  };
  result[CLM_QUADRANT_NE] = (sbx_CollisionNeighbors){
    .indices = { n, n + e, e },
    .normalAxis = { y, y, x },
    .normalDirection = { neg, neg, neg }
  };
  result[CLM_QUADRANT_E] = (sbx_CollisionNeighbors){
    .indices = { e },
    .normalAxis = { x },
    .normalDirection = { neg }
  };
  result[CLM_QUADRANT_SW] = (sbx_CollisionNeighbors){
    .indices = { e, s, s + e },
    .normalAxis = { x, y, y },
    .normalDirection = { neg, pos, pos }
  };
  result[CLM_QUADRANT_S] = (sbx_CollisionNeighbors){
    .indices = { s },
    .normalAxis = { y },
    .normalDirection = { pos }
  };
  result[CLM_QUADRANT_SW] = (sbx_CollisionNeighbors){
    .indices = { w, w + s, s },
    .normalAxis = { x, y, y },
    .normalDirection = { pos, pos, pos }
  };
  result[CLM_QUADRANT_W] = (sbx_CollisionNeighbors){
    .indices = { w },
    .normalAxis = { x },
    .normalDirection = { pos }
  };
}

struct sbx_SandGrid {
  struct sbx_GrainStack stack;
  sbx_CollisionNeighbors collisionNeighbors[CLM_QUADRANT_COUNT];
  clm_irect rect;
  sbx_SandGridConfig config;
  uint8_t age;
};

struct sbx_GrainIt {
  sbx_SandGrid* grid;
  void* data;
  //Index into grid->grains. grid-bitmap requires sbx_grainToBitmap
  int32_t e;
  //XY coordinates corresponding to `e`
  int32_t x;
  int32_t y;
};
typedef struct sbx_GrainIt sbx_GrainIt;

typedef void(*sbx_GrainIterator)(const sbx_GrainIt*);

void sbx_iterate_grains(sbx_SandGrid* grid, const clm_irect* rect, sbx_GrainIterator callback, void* userdata) {
  //Intersect to ensure rect is within grid
  const clm_irect it = clm_irect_intersect(&grid->rect, rect);
  sbx_GrainIt data;
  data.grid = grid;
  data.data = userdata;
  for(data.y = it.minY; data.y < it.maxY; ++data.y) {
    //Shift the x and y over one to account for the sentry row and column
    //The x and y coordinates are in user space ignoring the sentry elements.
    //e is the direct index which needs to account for sentry elements.
    data.e = sbx_GrainStack_uxytoi(&grid->stack, it.minX, data.y);
    for(data.x = it.minX; data.x < it.maxX; ++data.x, ++data.e) {
      callback(&data);
    }
  }
}

sbx_SandGrid* sbx_SandGrid_ctor(std_Allocator* alloc, sbx_SandGridConfig config) {
  const int32_t gridBytes = sizeof(sbx_SandGrid);
  size_t stackBytes = 0;
  sbx_GrainStack_ctor(NULL, NULL, config.width, config.height, &stackBytes);

  sbx_SandGrid* result = std_Allocator_zalloc(alloc, gridBytes + stackBytes);
  //Start of extra data packed onto the end of the grid struct
  uint8_t* extra = (uint8_t*)(void*)(result + 1);
  sbx_GrainStack_ctor(&result->stack, extra, config.width, config.height, &stackBytes);
  extra += stackBytes;

  result->config = config;
  result->rect = clm_irect_fromMinMax(0, 0, config.width, config.height);
  sbx_buildCollisionNeighbors(result->collisionNeighbors, result->stack.internalWidth);

  //Create sentry rows/columns
  const struct sbx_SandGrain sentry = {
    .type = SBX_GT_STATIC
  };
  //Sentry on each column going row by row and setting first and last of the row
  for(uint32_t r = 0; r < (uint32_t)config.height + sbx_GrainStack_getSentryPadding(); ++r) {
    const uint32_t rowBegin = r * result->stack.internalWidth;
    *sbx_GrainStack_getGrainI(&result->stack, rowBegin) = sentry;
    *sbx_GrainStack_getGrainI(&result->stack, rowBegin + result->stack.internalWidth - 1) = sentry;
  }
  //Sentry on first and last row
  const int32_t lastRow = result->stack.internalWidth * (config.height + 1);
  for(uint32_t c = 0; c < result->stack.internalWidth; ++c) {
    *sbx_GrainStack_getGrainI(&result->stack, c) = sentry;
    *sbx_GrainStack_getGrainI(&result->stack, lastRow + c) = sentry;
  }

  //Set to clear color
  for(uint32_t i = 0; i < result->stack.userCount; ++i) {
    *sbx_GrainStack_getColorU(&result->stack, i) = config.clearColor;
  }

  return result;
}

struct sbx_InsertData {
  bool result;
  //Never null
  const sbx_SandGridGrain* toInsert;
  //Number of elements in `toInsert`
  const int32_t insertCount;
  //Current index into `toInsert` within iteration callback
  int32_t insertIndex;
};
typedef struct sbx_InsertData sbx_InsertData;

const sbx_SandGridGrain* sbx_InsertData_getAndAdvanceInput(sbx_InsertData* data) {
  const sbx_SandGridGrain* result = &data->toInsert[data->insertIndex];
  if(data->insertIndex + 1 < data->insertCount) {
    ++data->insertIndex;
  }
  return result;
}

void sbx_SandGrid_setGridGrain(sbx_SandGrid* grid, int32_t i, const sbx_SandGridGrain* value) {
  const bool wasActive = sbx_GrainStack_isActive(&grid->stack, i);

  struct sbx_SandGrain grain = {
    .type = value->shape.type,
    .inverseMass = value->mass ? 1.f/((float)value->mass) : 0.f
  };
  sbx_GrainStack_setGrainI(&grid->stack, i, &grain, &grid->config.clearColor, &value->color);

  if(sbx_GrainStack_shouldIntegrateI(&grid->stack, i) && !wasActive && sbx_GrainStack_isActive(&grid->stack, i)) {
    sbx_PhaseIndexList_pushBack(sbx_GrainStack_getPhase(&grid->stack, SBX_GLI_INTEGRATION_PHASE), i);
  }
}

void sbx_SandGrid_tryInsert(const sbx_GrainIt* it) {
  sbx_InsertData* data = it->data;
  const sbx_SandGridGrain* input = sbx_InsertData_getAndAdvanceInput(data);

  //Insert if empty
  if(sbx_GrainStack_isVacant(&it->grid->stack, it->e)) {
    //Any insert counts as success
    data->result = true;
    sbx_SandGrid_setGridGrain(it->grid, it->e, input);
  }
}

void sbx_SandGrid_replace(const sbx_GrainIt* it) {
  sbx_SandGrid_setGridGrain(it->grid, it->e, sbx_InsertData_getAndAdvanceInput(it->data));
}

bool sbx_SandGrid_isValidRect(const sbx_SandGrid* grid, const clm_irect* rect) {
  return grid->config.width >= rect->maxX && grid->config.height >= rect->maxY;
}

void sbx_SandGrid_assertRect(const sbx_SandGrid* grid, const clm_irect* rect) {
  STD_ASSERT(sbx_SandGrid_isValidRect(grid, rect));
}

bool sbx_SandGrid_insert(const sbx_SandGridInsertOps* ops) {
  sbx_SandGrid_assertRect(ops->grid, ops->rect);
  sbx_SandGridGrain empty = { 0 };
  sbx_InsertData data = {
    .toInsert = ops->grainCount && ops->grains ? ops->grains : &empty,
    .insertCount = ops->grainCount && ops->grains ? ops->grainCount : 1,
  };

  switch(ops->mode) {
    case SBX_SGI_TRY:
      sbx_iterate_grains(ops->grid, ops->rect, &sbx_SandGrid_tryInsert, &data);
      break;
    case SBX_SGI_REPLACE:
      sbx_iterate_grains(ops->grid, ops->rect, &sbx_SandGrid_replace, &data);
      data.result = true;
      break;
  }
  return data.result;
}

const clm_byte4* sbx_SandGrid_getTexture(const sbx_SandGrid* grid) {
  return sbx_GrainStack_getColorUC(&grid->stack, 0);
}

/*
void sbx_integrateGrain(const sbx_GrainIt* it) {
  sbx_SandGrain* grain = &it->grid->grains[it->e];
  if(!sbx_shouldIntegrate(grain, it->grid->age)) {
    return;
  }
  //Prevent this from being iterated again
  grain->age = it->grid->age;

  const sbx_IntegrateData* data = it->data;
  //Integrate velocity. Gravity already contains dt
  grain->velocity = clm_vec2_add(&grain->velocity, &data->gravity);

  //Apply damping
  grain->velocity = clm_vec2_scale(&grain->velocity, GRAIN_DAMPING);

  //Itegrate position
  const clm_vec2 vt = clm_vec2_scale(&grain->velocity, data->dt);
  grain->position = clm_vec2_add(&grain->position, &vt);

  //Check collision with neighboring cells in the direction of velocity
  //If moving into the cell is possible this will do so
  const sbx_CollisionRegion region = sbx_classifyRegion(&grain->velocity);
  const sbx_CollisionNeighbors* neighbors = &it->grid->collisionNeighbors[region];
  for(int i = 0; i < 3 && neighbors->indices[i]; ++i) {
    const int32_t neighborIndex = it->e + neighbors->indices[i];
    sbx_SandGrain* neighbor = &it->grid->grains[neighborIndex];
    if(sbx_isVacant(neighbor->type)) {
      //If vacant, greedily move into the cell and don't try any other neighbors.
      //If moving in the direction of the integration the cell can be integrated twice.
      //For simplicity, this is acceptable.

      //Move this into neighbor
      *neighbor = *grain;
      //Reset position within cell. Subtracting move direction would be more accurate, ok for simplicity.
      neighbor->position = clm_vec2_zero();

      //Reset neighbor
      *grain = (sbx_SandGrain){ 0 };

      //Update bitmap of both locations
      const int32_t bmpSrc = sbx_grainToBitmap(it->grid, it->e);
      const int32_t bmpDst = sbx_grainToBitmap(it->grid, neighborIndex);
      //Write pixel to new location
      it->grid->bitmap[bmpDst] = it->grid->bitmap[bmpSrc];
      //Clear old location
      it->grid->bitmap[bmpSrc] = it->grid->config.clearColor;

      //Skip other collisions as now neighbor information is invalid
      break;
    }

    //Collision. Compute the velocity along the colliding axis
    const uint8_t axis = neighbors->normalAxis[i];
    const float normalDirection = (float)neighbors->normalDirection[i];
    const float ma = (float)grain->mass;
    float va = grain->velocity.xy[axis];

    const float mb = (float)neighbor->mass;
    float vb = neighbor->velocity.xy[axis];

    //Relative velocity of the two project along the normal
    const float vRel = (vb - va) * normalDirection;
    //If objects are already separating, nothing to do
    if(vRel <= 0) {
      continue;
    }

    const float restitution = 0.2f;
    //TODO: could limit masses to a few values and use a lookup table
    //One of the objects must have nonzero mass as they are moving
    const float impulseMass = 1.0f / (ma + mb);
    //Full impulse is:
    //i = ((ma*mb)/(ma+mb)) * (1 + r)(vb - va) * n
    //The change in velocity is va = i/ma, meaning the ma cancels with ma*mb
    //What remains of the mass after cancelling is
    //mb/(ma+mb) for object A
    //ma/(ma+mb) for object B
    const float impulse = impulseMass * vRel * (1.0f + restitution);

    //Apply impulse to both objects. Since they divide by their mass, impulse should be zero if they have no mass
    if (ma) {
      va += impulse / ma;
    }
    if (mb) {
      vb -= impulse / mb;
    }

    //Write final velocities.
    grain->velocity.xy[axis] = va;
    neighbor->velocity.xy[axis] = vb;
  }
}
*/

void sbx_SandGrid_integration_phase(sbx_SandGrid* grid, struct sbx_PhaseIndexList* list, float dt) {
  const clm_vec2 gravity = clm_vec2_scale(&grid->config.gravity, dt);

  for(uint32_t i = 0; i < list->size; ++i) {
    struct sbx_SandGrain* grain =  sbx_GrainStack_getGrainI(&grid->stack, list->array[i]);
    STD_ASSERT(sbx_GrainStack_shouldIntegrateI(&grid->stack, list->array[i]));

    //Integrate velocity. Gravity already contains dt
    grain->velocity = clm_vec2_add(&grain->velocity, &gravity);

    //Apply damping
    grain->velocity = clm_vec2_scale(&grain->velocity, GRAIN_DAMPING);
  }
  list->size = 0;
}

void sbx_SandGrid_island_add_phase(sbx_SandGrid* grid, struct sbx_PhaseIndexList* list) {
  for(uint32_t i = 0; i < list->size; ++i) {
    struct sbx_SandGrain* grain =  sbx_GrainStack_getGrainI(&grid->stack, list->array[i]);
    STD_UNUSED(grain);
    //Visit all direct neighbors and compute neighbor bitfield
    //If there are none, add to move_phase
    //If this isn't in an island but they are, set this island to theirs and add to solve_phase.
    //The information added here should be enough that solving doesn't need to recompute anything.
    //If this is in a different island than the other, set both to the island with the lower value, then add the node that changed back into the add_phase, which will traverse the graph.
    //Adding back into this phase is done by writing over previously iterated values and keeping track of the newly inserted size.
  }
}

void sbx_SandGrid_island_split_phase(sbx_SandGrid* grid, struct sbx_PhaseIndexList* list) {
  for(uint32_t i = 0; i < list->size; ++i) {
    struct sbx_SandGrain* grain =  sbx_GrainStack_getGrainI(&grid->stack, list->array[i]);
    STD_UNUSED(grain);
    //Nodes in this list have lost edges.
    //Visit all neighbors using a pending island index, visiting any neighbors that don't have it set yet, then setting it.
    //Once none more can be traversed this way, the island index is set to the pending index if the entire island wasn't traversed.
    //The age parameter also needs to be used to avoid traversing the same islands multiple times in the same frame.
  }
}

struct IslandData {
  float islandError;
  struct sbx_GrainStack* stack;
  uint8_t age;
};

void sbx_SandGrid_solveIslandPair(struct IslandData* data, uint32_t a, uint32_t b, int8_t normalAxis, int8_t normalDir) {
  //Skip if this doesn't need solving
  //TODO: avoid visiting this in the first place
  if(sbx_GrainStack_isVacant(data->stack, b)) {
    return;
  }

  //Visit constraint once
  struct sbx_ConstraintSlot* constraint = sbx_GrainStack_getConstraintSlot(data->stack, a, b);
  if(constraint->age == data->age) {
    return;
  }
  constraint->age = data->age;

  struct sbx_SandGrain* ga = sbx_GrainStack_getGrainI(data->stack, a);
  struct sbx_SandGrain* gb = sbx_GrainStack_getGrainI(data->stack, b);

  float* va = &ga->velocity.xy[normalAxis];
  float* vb = &gb->velocity.xy[normalAxis];
  const float ja = normalDir > 0 ? 1.f : -1.f;
  const float jb = -ja;
  const float ma = ga->inverseMass;
  const float mb = gb->inverseMass;
  STD_ASSERT(ma && mb);
  //Normally 1 / (ja*jma + jb+jmb)
  //But the jacobian is the normal axis 1 or -1, meaning only the mass is relevant.
  //This can't be division by zero because a static pair should have been skipped by the caller.
  //TODO: this could be a lookup table if allowed mass range were limited
  const float constraintMass = 1.f / (ma + mb);
  const float bias = 0;
  const float lambdaMin = 0;
  const float lambdaMax = FLT_MAX;

  float lambda = (bias - (ja * *va) + (jb * *vb)) * constraintMass;
  const float prevLambda = constraint->lambdaSum;
  constraint->lambdaSum = std_max(lambdaMin, std_min(prevLambda + lambda, lambdaMax));
  lambda = constraint->lambdaSum - prevLambda;

  //Apply impulse
  *va += ja*ma*lambda;
  *vb += jb*mb*lambda;

  data->islandError += fabsf(lambda);
}

void sbx_tryVisitIslandNode(sbx_SandGrid* grid, struct sbx_PhaseIndexList* temp, uint32_t node, uint8_t age) {
  //Visit if it hasn't been visited, and if it is non-static.
  //Adding to the list sets the visited flag (age), meaning it can't be added twice, and that the loop processing indices doesn't need to double-check.
  if(sbx_GrainStack_trySetAge(&grid->stack, node, age) && sbx_GrainStack_isActive(&grid->stack, node)) {
    sbx_PhaseIndexList_pushBack(temp, node);
  }
}

void sbx_SandGrid_solveIsland(sbx_SandGrid* grid, uint32_t islandRoot, uint8_t age) {
  struct sbx_PhaseIndexList* temp = sbx_GrainStack_getPhase(&grid->stack, SBX_GLI_TEMP_PHASE);
  sbx_PhaseIndexList_clear(temp);
  sbx_PhaseIndexList_pushBack(temp, islandRoot);
  sbx_tryVisitIslandNode(grid, temp, islandRoot, age);
  struct IslandData data = {
    .age = age,
    .stack = &grid->stack,
  };

  for(uint32_t i = 0; i < temp->size; ++i) {
    //We know this is not visited yet and active otherwise it wouldn't have been in the list.
    const uint32_t e = temp->array[i];

    //Push all neighbors into temp list for visiting
    //TODO: cache to limit traversal and avoid visiting redundant pairs
    sbx_tryVisitIslandNode(grid, temp, e - grid->stack.internalWidth - 1, age);
    sbx_tryVisitIslandNode(grid, temp, e - grid->stack.internalWidth, age);
    sbx_tryVisitIslandNode(grid, temp, e - grid->stack.internalWidth + 1, age);
    sbx_tryVisitIslandNode(grid, temp, e - 1, age);
    sbx_tryVisitIslandNode(grid, temp, e + 1, age);
    sbx_tryVisitIslandNode(grid, temp, e + grid->stack.internalWidth - 1, age);
    sbx_tryVisitIslandNode(grid, temp, e + grid->stack.internalWidth, age);
    sbx_tryVisitIslandNode(grid, temp, e + grid->stack.internalWidth + 1, age);

    //Solve contact constraints in the direction of velocity
    const struct sbx_SandGrain* grain = sbx_GrainStack_getGrainIC(&grid->stack, e);
    const enum clm_quadrant_region region = clm_quadrant_getRegion(&grain->velocity, 0.f);
    const sbx_CollisionNeighbors* neighborInfo = &grid->collisionNeighbors[region];
    for(int r = 0; neighborInfo->indices[r]; ++r) {
      const uint32_t n = neighborInfo->indices[r] + e;
      sbx_SandGrid_solveIslandPair(&data, e, n, neighborInfo->normalAxis[r], neighborInfo->normalDirection[r]);
    }
  }

  if(data.islandError < ISLAND_SOLVE_THRESHOLD) {
    struct sbx_PhaseIndexList* toMove = sbx_GrainStack_getPhase(data.stack, SBX_GLI_MOVE_PHASE);
    //Go back over the island and put them all in the move list
    for(uint32_t i = 0; i < temp->size; ++i) {
      sbx_PhaseIndexList_pushBack(toMove, temp->array[i]);
    }
  }
}

void sbx_SandGrid_solve_phase(sbx_SandGrid* grid, struct sbx_PhaseIndexList* list) {
  const uint8_t age = ++grid->age;
  for(uint32_t i = 0; i < list->size; ++i) {
    const uint32_t islandRoot = list->array[i];
    //If it's already solved, skip it
    //TODO: need to remove islands when they are solved. I think that would mean removing these from the list
    if(!sbx_GrainStack_isOldAge(&grid->stack, islandRoot, age)) {
      continue;
    }
    //Traverse and solve the island
    sbx_SandGrid_solveIsland(grid, islandRoot, age);
  }
}

void sbx_SandGrid_island_move_phase(sbx_SandGrid* grid, struct sbx_PhaseIndexList* list, float dt) {
  const float cellHalfSize = 0.5f;

  for(uint32_t i = 0; i < list->size; ++i) {
    const uint32_t e = list->array[i];
    struct sbx_SandGrain* grain =  sbx_GrainStack_getGrainI(&grid->stack, list->array[i]);
    STD_UNUSED(grain);
    //This approach will attempt to move into a space and fail if the destination is taken.
    //It means motion will be skipped unnecessarily when grains are swapping places, like a stack falling down.
    //In that case, each node is falling down into the one below it, which is fine, because they too will move out of the way,
    //but attempting to move top to bottom would seem them all as skipped except the top one.

    //Itegrate position
    const clm_vec2 vt = clm_vec2_scale(&grain->velocity, dt);
    grain->position = clm_vec2_add(&grain->position, &vt);

    //Compute the region of their position within the cell.
    //Any value other than INSIDE means the grain needs to move in that direction.
    enum clm_quadrant_region moveDir = clm_quadrant_getRegion(&grain->position, cellHalfSize);
    uint32_t destination = e + clm_quadrant_index2D(moveDir, (int32_t)grid->stack.internalWidth);

    //TODO: add to the proper phases, even if they don't move

    //Spot is taken, move is not allowed
    if(!sbx_GrainStack_isVacant(&grid->stack, destination)) {
      //TODO: should this undo position? It could build up strangely.
      continue;
    }

    //Subtract the direction moved, making the position cell-relative
    const clm_vec2 translation = clm_quadrant_vec2FromRegion(moveDir, cellHalfSize);
    grain->position = clm_vec2_sub(&grain->position, &translation);

    sbx_GrainStack_moveI(&grid->stack, e, destination, &grid->config.clearColor);
  }

  //Everything moved or tried to, clear the list
  sbx_PhaseIndexList_clear(list);
}

void sbx_SandGrid_integrate(sbx_SandGrid* grid, float dt) {
  struct sbx_GrainStack* stack = &grid->stack;
  sbx_SandGrid_integration_phase(grid, sbx_GrainStack_getPhase(stack, SBX_GLI_INTEGRATION_PHASE), dt);
  sbx_SandGrid_solve_phase(grid, sbx_GrainStack_getPhase(stack, SBX_GLI_SOLVE_PHASE));
  sbx_SandGrid_island_move_phase(grid, sbx_GrainStack_getPhase(stack, SBX_GLI_MOVE_PHASE), dt);
}

void sbx_applyImpulseIterator(const sbx_GrainIt* it) {
  sbx_SandGridImpulseIterator* impulse = it->data;
  const clm_vec2 pos = clm_vec2_ctor((float)it->x, (float)it->y);
  struct sbx_SandGrain* grain = sbx_GrainStack_getGrainI(&it->grid->stack, it->e);
  if(grain->inverseMass) {
    clm_vec2 i = impulse->computeImpulse(&pos, impulse->data);
    if(!clm_vec2_isZero(&i)) {
      i = clm_vec2_scale(&i, grain->inverseMass);

      grain->velocity = clm_vec2_add(&grain->velocity, &i);
    }
  }
}

void sbx_SandGrid_applyImpulse(sbx_SandGrid* grid, const clm_irect* rect, sbx_SandGridImpulseIterator it) {
  clm_irect query = sbx_SandGrid_clipToGrid(grid, rect);
  sbx_iterate_grains(grid, &query, &sbx_applyImpulseIterator, &it);
}

void sbx_doQuery(const sbx_GrainIt* it) {
  sbx_SandQueryResult** result = it->data;
  struct sbx_SandGrain* grain = sbx_GrainStack_getGrainI(&it->grid->stack, it->e);
  **result = (sbx_SandQueryResult){
    .velocity = grain->velocity,
    .position = grain->position,
    .shape = grain->type,
    .color = *sbx_GrainStack_getColorIC(&it->grid->stack, it->e),
    .mass = grain->inverseMass ? (uint8_t)(1.0f/grain->inverseMass) : 0
  };
  (*result)++;
}

void sbx_SandGrid_query(sbx_SandGrid* grid, const clm_irect* rect, sbx_SandQueryResult* result) {
  sbx_SandGrid_assertRect(grid, rect);
  sbx_iterate_grains(grid, rect, &sbx_doQuery, &result);
}

clm_irect sbx_SandGrid_clipToGrid(const sbx_SandGrid* grid, const clm_irect* toClip) {
  return clm_irect_intersect(&grid->rect, toClip);
}
