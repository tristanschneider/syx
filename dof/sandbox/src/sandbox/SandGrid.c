#include <sandbox/SandGrid.h>

#include <std/Diagnostics.h>
#include <clm/vec2.h>

const int32_t SENTRY_PADDING = 2;
//Offset to convert user space x,y coordinates to sentry space coordinates
const int32_t USER_TO_SENTRY = 1;

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

bool sbx_isVacant(sbx_GrainType type) {
  return type == SBX_GT_EMPTY;
}

struct sbx_SandGrain {
  clm_vec2 velocity;
  clm_vec2 position;
  uint8_t mass;
  uint8_t type;
  //ID to avoid integrating the same grain twice at once
  uint8_t age;
};
typedef struct sbx_SandGrain sbx_SandGrain;

struct sbx_SandGrid {
  sbx_SandGrain* grains;
  //Index stride of grains, so the width of the grain array including sentry columns
  int32_t grainStride;
  int32_t grainCount;
  sbx_CollisionNeighbors collisionNeighbors[SBX_CR_COUNT];
  clm_irect rect;
  sbx_SandGridConfig config;
  clm_byte4* bitmap;
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
    data.e = (it.minX + USER_TO_SENTRY + grid->grainStride * (data.y + USER_TO_SENTRY));
    for(data.x = it.minX; data.x < it.maxX; ++data.x, ++data.e) {
      callback(&data);
    }
  }
}

sbx_SandGrid* sbx_SandGrid_ctor(std_Allocator* alloc, sbx_SandGridConfig config) {
  const int32_t grainCount = config.width * config.height;
  //Grain area padded by sentry elements so neighbors can always be indexed without bounds checks
  //This is not reflected to the user, the rect refers to the inner space
  const int32_t paddedGrainCount = (config.width + SENTRY_PADDING) * (config.height + SENTRY_PADDING);
  const int32_t bitmapBytes = grainCount * sizeof(clm_byte4);
  const int32_t grainBytes = paddedGrainCount * sizeof(sbx_SandGrain);
  const int32_t gridBytes = sizeof(sbx_SandGrid);
  sbx_SandGrid* result = std_Allocator_alloc(alloc, gridBytes + grainBytes + bitmapBytes);
  //Start of extra data packed onto the end of the grid struct
  uint8_t* extraBegin = (uint8_t*)(void*)(result + 1);
  result->grains = (void*)(extraBegin);
  result->bitmap = (void*)(extraBegin + grainBytes);

  result->grainStride = config.width + 2;
  result->grainCount = grainCount;
  result->config = config;
  result->rect = clm_irect_fromMinMax(0, 0, config.width, config.height);
  sbx_buildCollisionNeighbors(result->collisionNeighbors, result->grainStride);

  //Zero memory for all grains, which is zero velocity, zero mass, empty type
  memset(result->grains, 0, grainBytes);

  //Create sentry rows/columns
  const sbx_SandGrain sentry = {
    .type = SBX_GT_STATIC
  };
  //Sentry on each column going row by row and setting first and last of the row
  for(int32_t r = 0; r < config.height + SENTRY_PADDING; ++r) {
    const int32_t rowBegin = r * result->grainStride;
    result->grains[rowBegin] = sentry;
    result->grains[rowBegin + result->grainStride - 1] = sentry;
  }
  //Sentry on first and last row
  const int32_t lastRow = result->grainStride * (config.height + 1);
  for(int32_t c = 0; c < result->grainStride; ++c) {
    result->grains[c] = sentry;
    result->grains[lastRow + c] = sentry;
  }

  //Set to clear color
  for(int32_t i = 0; i < grainCount; ++i) {
    result->bitmap[i] = config.clearColor;
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
  if(data->insertIndex < data->insertCount) {
    ++data->insertIndex;
  }
  return result;
}

int32_t sbx_grainToBitmap(const sbx_SandGrid* grid, int32_t i) {
  const int32_t rows = i / grid->grainStride;
  //Sentry row count without counting the portions of the sentry columns that cut into it
  const int32_t sentryRow = grid->grainStride - SENTRY_PADDING;
  //Every row passed means two sentry columns, and the current row has one more on the left
  const int32_t sentryColumns = rows * 2 + 1;
  return i - sentryRow - sentryColumns;
}

void sbx_SandGrid_setGridGrain(sbx_SandGrid* grid, int32_t i, const sbx_SandGridGrain* value) {
  sbx_SandGrain* grain = &grid->grains[i];
  grain->type = value->shape.type;
  grain->mass = value->mass;
  grain->velocity = clm_vec2_zero();

  grid->bitmap[sbx_grainToBitmap(grid, i)] = sbx_isVacant(grain->type) ? grid->config.clearColor : value->color;
}

void sbx_SandGrid_tryInsert(const sbx_GrainIt* it) {
  sbx_InsertData* data = it->data;
  const sbx_SandGridGrain* input = sbx_InsertData_getAndAdvanceInput(data);

  //Insert if empty
  if(sbx_isVacant(it->grid->grains[it->e].type)) {
    //Any insert counts as success
    data->result = true;
    sbx_SandGrid_setGridGrain(it->grid, it->e, input);
  }
}

void sbx_SandGrid_replace(const sbx_GrainIt* it) {
  sbx_SandGrid_setGridGrain(it->grid, it->e, sbx_InsertData_getAndAdvanceInput(it->data));
}

void sbx_SandGrid_assertRect(const sbx_SandGrid* grid, const clm_irect* rect) {
  STD_ASSERT(grid->config.width >= rect->maxX && grid->config.height >= rect->maxY);
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
  return grid->bitmap;
}

struct sbx_IntegrateData {
  clm_vec2 gravity;
  float dt;
};
typedef struct sbx_IntegrateData sbx_IntegrateData;

bool sbx_shouldIntegrate(const sbx_SandGrain* grain, uint8_t age) {
  return grain->mass != 0.f && grain->age != age;
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

    const float restitution = 0;
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
      va += impulse * mb;
    }
    if (mb) {
      vb += impulse * ma;
    }

    //Write final velocities.
    grain->velocity.xy[axis] = va;
    neighbor->velocity.xy[axis] = vb;
  }
}

void sbx_SandGrid_integrate(sbx_SandGrid* grid, const clm_irect* rect, float dt) {
  grid->age++;
  sbx_IntegrateData data = {
    .gravity = clm_vec2_scale(&grid->config.gravity, dt),
    .dt = dt
  };
  sbx_iterate_grains(grid, rect, &sbx_integrateGrain, &data);
}
