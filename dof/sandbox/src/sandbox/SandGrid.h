#pragma once

#include <std/Allocator.h>
#include <clm/byte4.h>
#include <clm/irect.h>
#include <clm/vec2.h>

#include <stdint.h>
#include <stdbool.h>

struct sbx_SandGrid;
struct sbx_ModelVertices;

typedef struct sbx_SandGrid sbx_SandGrid;
typedef struct sbx_ModelVertices sbx_ModelVertices;

struct sbx_SandGridConfig {
  int32_t width, height;

  clm_byte4 clearColor;
  clm_vec2 gravity;
};
typedef struct sbx_SandGridConfig sbx_SandGridConfig;

enum sbx_GrainType {
  SBX_GT_EMPTY = 0,
  SBX_GT_GRAIN,
  SBX_GT_STATIC
};
typedef enum sbx_GrainType sbx_GrainType;

struct sbx_SandGridShape {
  sbx_GrainType type;
};
typedef struct sbx_SandGridShape sbx_SandGridShape;

struct sbx_SandGridGrain {
  uint8_t mass;
  sbx_SandGridShape shape;
  clm_byte4 color;
};
typedef struct sbx_SandGridGrain sbx_SandGridGrain;

struct sbx_SandQueryResult {
  clm_vec2 velocity;
  clm_vec2 position;
  sbx_SandGridShape shape;
  clm_byte4 color;
  uint8_t mass;
};
typedef struct sbx_SandQueryResult sbx_SandQueryResult;

enum sbx_SandGridInsertMode {
  //Try to insert at the given coordinates and fail if they are occupied
  SBX_SGI_TRY,
  SBX_SGI_REPLACE
};
typedef enum sbx_SandGridInsertMode sbx_SandGridInsertMode;

struct sbx_SandGridInsertOps {
  sbx_SandGrid* grid;
  //Grains will be iterated in rect order from min to max
  //If count is exhausted the last one will be repeated for all remaining entries.
  //If grains are null or count is zero, empty grains will be inserted
  const sbx_SandGridGrain* grains;
  int32_t grainCount;
  const clm_irect* rect;
  sbx_SandGridInsertMode mode;
};
typedef struct sbx_SandGridInsertOps sbx_SandGridInsertOps;

//Allocates a block of memory that can be freed with the provided allocator
sbx_SandGrid* sbx_SandGrid_ctor(std_Allocator* alloc, sbx_SandGridConfig config);

bool sbx_SandGrid_insert(const sbx_SandGridInsertOps* ops);

//Integrate the area of the grid specified by rect. Use clm_irect_limits to integrate the entire grid.
void sbx_SandGrid_integrate(sbx_SandGrid* grid, const clm_irect* rect, float dt);

//Get an rgba bitmap of the size specified by sbx_SandGridConfig
const clm_byte4* sbx_SandGrid_getTexture(const sbx_SandGrid* grid);

bool sbx_SandGrain_isValidRect(const sbx_SandGrid* grid, const clm_irect* rect);
//Query the region of `rect` and put the results in `result`. Area must be within grid.
void sbx_SandGrid_query(sbx_SandGrid* grid, const clm_irect* rect, sbx_SandQueryResult* result);
