#include <sandbox/SandGrid.h>

#include <std/Diagnostics.h>
#include <clm/vec2.h>
#include <sandbox/SandGridImpulseIterator.h>
#include <sandbox/GrainStack.h>

const float GRAIN_DAMPING = 0.99f;
const float GRAIN_MOVE_EPSILON = 1.f;

//Given a reference point X the enum indicates the respective combinations of neighbors
//123
//4X5
//678
enum sbx_CollisionRegion {
  SBX_CR_NONE,
  //124
  SBX_CR_NW,
  //2
  SBX_CR_N,
  //235
  SBX_CR_NE,
  //5
  SBX_CR_E,
  //578
  SBX_CR_SE,
  //7
  SBX_CR_S,
  //467
  SBX_CR_SW,
  //4
  SBX_CR_W,
  SBX_CR_COUNT,
};
typedef enum sbx_CollisionRegion sbx_CollisionRegion;
struct sbx_CollisionNeighbors {
  //Up to 3 neighbors as indicated by sbx_CollisionRegion with 0 indicating the end of the sequence.
  int32_t indices[3];
  //Axis into the vec2, so 0 or 1, of the normal pointing from the neighbor to the original
  uint8_t normalAxis[3];
  //1 or -1 for direction of normal, normal is `n[normalAxis] = normalDirection`
  int8_t normalDirection[3];
};
typedef struct sbx_CollisionNeighbors sbx_CollisionNeighbors;

void sbx_buildCollisionNeighbors(sbx_CollisionNeighbors result[SBX_CR_COUNT], int32_t stride) {
  const int32_t n = -stride;
  const int32_t s = stride;
  const int32_t e = 1;
  const int32_t w = -1;
  const uint8_t x = 0;
  const uint8_t y = 1;
  const int8_t pos = 1;
  const int8_t neg = -1;
  result[SBX_CR_NONE] = (sbx_CollisionNeighbors){ 0 };
  // Diagonals favor Y
  result[SBX_CR_NW] = (sbx_CollisionNeighbors){
    .indices = { n + w, n, w },
    .normalAxis = { y, y, x },
    .normalDirection = { neg, neg, pos }
  };
  result[SBX_CR_N] = (sbx_CollisionNeighbors){
    .indices = { n },
    .normalAxis = { y },
    .normalDirection = { neg }
  };
  result[SBX_CR_NE] = (sbx_CollisionNeighbors){
    .indices = { n, n + e, e },
    .normalAxis = { y, y, x },
    .normalDirection = { neg, neg, neg }
  };
  result[SBX_CR_E] = (sbx_CollisionNeighbors){
    .indices = { e },
    .normalAxis = { x },
    .normalDirection = { neg }
  };
  result[SBX_CR_SE] = (sbx_CollisionNeighbors){
    .indices = { e, s, s + e },
    .normalAxis = { x, y, y },
    .normalDirection = { neg, pos, pos }
  };
  result[SBX_CR_S] = (sbx_CollisionNeighbors){
    .indices = { s },
    .normalAxis = { y },
    .normalDirection = { pos }
  };
  result[SBX_CR_SW] = (sbx_CollisionNeighbors){
    .indices = { w, w + s, s },
    .normalAxis = { x, y, y },
    .normalDirection = { pos, pos, pos }
  };
  result[SBX_CR_W] = (sbx_CollisionNeighbors){
    .indices = { w },
    .normalAxis = { x },
    .normalDirection = { pos }
  };
}

struct sbx_SandGrid {
  struct sbx_GrainStack stack;
  sbx_CollisionNeighbors collisionNeighbors[SBX_CR_COUNT];
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
    .mass = value->mass
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

//TODO: get dominant axis for desired move direction and collision normal
//Return the region type for when the position exceeds the epsilon in any of the directions.
//This is the direction a cell wants to move and must check collisions against
sbx_CollisionRegion sbx_classifyRegion(clm_vec2* v) {
  //North
  if(v->y > GRAIN_MOVE_EPSILON) {
   if(v->x > GRAIN_MOVE_EPSILON) {
     return SBX_CR_NE;
   }
   if(v->x < -GRAIN_MOVE_EPSILON) {
     return SBX_CR_NW;
   }
   return SBX_CR_N;
  }
  //South
  if(v->y < -GRAIN_MOVE_EPSILON) {
   if(v->x > GRAIN_MOVE_EPSILON) {
     return SBX_CR_SE;
   }
   if(v->x < -GRAIN_MOVE_EPSILON) {
     return SBX_CR_SW;
   }
   return SBX_CR_S;
  }
  //East and West
  if(v->x > GRAIN_MOVE_EPSILON) {
    return SBX_CR_E;
  }
  if(v->x < -GRAIN_MOVE_EPSILON) {
    return SBX_CR_W;
  }
  return SBX_CR_NONE;
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

    //Itegrate position
    const clm_vec2 vt = clm_vec2_scale(&grain->velocity, dt);
    grain->position = clm_vec2_add(&grain->position, &vt);
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

void sbx_SandGrid_solve_phase(sbx_SandGrid* grid, struct sbx_PhaseIndexList* list) {
  for(uint32_t i = 0; i < list->size; ++i) {
    struct sbx_SandGrain* grain =  sbx_GrainStack_getGrainI(&grid->stack, list->array[i]);
    STD_UNUSED(grain);
    //Solve all constraints stored on this as computed by the island_add_phase.
    //The error is accumulated on the island root node.
    //For the first element in the island, see if the error from the previous frame is below the acceptable margin. If so, add to move_phase and remove from here.
    //Use the age parameter on the root node to do this check
  }
}

void sbx_SandGrid_island_move_phase(sbx_SandGrid* grid, struct sbx_PhaseIndexList* list) {
  for(uint32_t i = 0; i < list->size; ++i) {
    struct sbx_SandGrain* grain =  sbx_GrainStack_getGrainI(&grid->stack, list->array[i]);
    STD_UNUSED(grain);
    //Compute the move destination index from position, then attempt to write a reserved flag there.
    //Accumulate a count of successful reservations on the island node.
    //On a second pass, move all nodes whose island root success reservations matches the island size.
    //Moves are added to the integration and add phases
    //Failed moves are put back in the solve_phase
  }
}

void sbx_SandGrid_integrate(sbx_SandGrid* grid, float dt) {
  struct sbx_GrainStack* stack = &grid->stack;
  sbx_SandGrid_integration_phase(grid, sbx_GrainStack_getPhase(stack, SBX_GLI_INTEGRATION_PHASE), dt);
  sbx_SandGrid_island_add_phase(grid, sbx_GrainStack_getPhase(stack, SBX_GLI_ISLAND_ADD_PHASE));
  sbx_SandGrid_island_split_phase(grid, sbx_GrainStack_getPhase(stack, SBX_GLI_ISLAND_SPLIT_PHASE));
  sbx_SandGrid_solve_phase(grid, sbx_GrainStack_getPhase(stack, SBX_GLI_SOLVE_PHASE));
  sbx_SandGrid_island_move_phase(grid, sbx_GrainStack_getPhase(stack, SBX_GLI_MOVE_PHASE));
}

void sbx_applyImpulseIterator(const sbx_GrainIt* it) {
  sbx_SandGridImpulseIterator* impulse = it->data;
  const clm_vec2 pos = clm_vec2_ctor((float)it->x, (float)it->y);
  struct sbx_SandGrain* grain = sbx_GrainStack_getGrainI(&it->grid->stack, it->e);
  if(grain->mass) {
    clm_vec2 i = impulse->computeImpulse(&pos, impulse->data);
    if(!clm_vec2_isZero(&i)) {
      i = clm_vec2_scale(&i, 1.f/(float)grain->mass);

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
    .mass = grain->mass
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
