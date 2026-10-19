#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <clm/vec2.h>
#include <clm/byte4.h>

//This file exposes the array-of-arrays storage of the grid data
//Each layer is an array indexed as the coordinates of the entire grid
//Layers are split into "internal" and "user" categories where
//user is the user specified region when creating the grid,
//and internal is that plus a layer of sentinel padding in all directions to simplify iteration rules.
//Data manipulation is exposed here to make it easier to change the layout without greatly changing the code that uses it.
//Types are split between I and U to avoid accidentally using an index in the wrong space.

//Internal grain layers, meaning padding included
enum sbx_GrainLayer {
  SBX_GLI_GRAIN,
  SBX_GLI_ID_TO_INDEX,
  SBX_GLI_INDEX_TO_ID,
  SBX_GLU_BITMAP,
  SBX_GLI_PHASE_BEGIN = SBX_GLU_BITMAP,
  //Integrate internal position and velocity of nodes
  //Nodes sholud be added here after moving
  SBX_GLI_INTEGRATION_PHASE,
  //Narrowphase, check neighboring entries to see if islands need to be added or combined
  //Nodes should be added here after moving
  SBX_GLI_ISLAND_ADD_PHASE,
  //Visits neighbors to see if island can be split
  //Nodes should be added here if the lose neighboring edges
  SBX_GLI_ISLAND_SPLIT_PHASE,
  //Solve nodes in an island. Once a solution is found, all nodes in an island are put in the move phase
  SBX_GLI_SOLVE_PHASE,
  //Moves entire islands at once by trying to reserve (flag on node) a spot for them all to move then
  //moving them all if the reservation succeeded
  //If so, they are all moved and added to the integration and island add phases
  //Otherwise, they are moved back to the solve phase
  SBX_GLI_MOVE_PHASE,
  //A temporary list for use within any of the other phases
  SBX_GLI_TEMP_PHASE,
  SBX_GL_COUNT,
  SBX_GLI_PHASE_END = SBX_GL_COUNT,
};

struct sbx_SandGrain {
  clm_vec2 velocity;
  clm_vec2 position;
  uint8_t mass;
  uint8_t type;
  //ID to avoid integrating the same grain twice at once
  uint8_t age;
};

struct sbx_PhaseIndexList {
  uint32_t* array;
  uint32_t size;
};

union sbx_LayerData {
  struct sbx_PhaseIndexList phase;
};

//Storage of the base array pointers for the internal and user layers.
//The memory points to a block of memory allocated off the end of the sbx_SandGrid, as sized by sxb_GrainStack_ctor size output
struct sbx_GrainStack {
  //Layers, where each is an array the size of the grid
  void* layers[SBX_GL_COUNT];
  //A single optional object for each layer
  union sbx_LayerData layerData[SBX_GL_COUNT];
  uint32_t internalWidth;
  uint32_t internalCount;
  uint32_t userCount;
};

//Assign the array pointers of the grain stack into buffer. Contents are expected to be zero-initialized.
//Buffer size on input should be the available size and on output will be the required size for the stack.
//If the size was too small, the pointers will not be assigned and the buffer parameter ignored.
void sbx_GrainStack_ctor(struct sbx_GrainStack* dst, uint8_t* buffer, uint32_t userWidth, uint32_t userHeight, size_t* bufferSizeInOut);

//Convert internal space index to user space index
uint32_t sbx_GrainStack_itou(const struct sbx_GrainStack* stack, uint32_t ii);
//Convert user space index to internal space index
//uint32_t sbx_GrainStack_utoi(const struct sbx_GrainStack* stack, uint32_t ui);
//Convert user space xy coordinates to internal index
uint32_t sbx_GrainStack_uxytoi(const struct sbx_GrainStack* stack, uint32_t ux, uint32_t uy);

uint32_t sbx_GrainStack_getSentryPadding();
const struct sbx_SandGrain* sbx_GrainStack_getGrainIC(const struct sbx_GrainStack* stack, uint32_t ii);
struct sbx_SandGrain* sbx_GrainStack_getGrainI(struct sbx_GrainStack* stack, uint32_t ii);
const clm_byte4* sbx_GrainStack_getColorUC(const struct sbx_GrainStack* stack, uint32_t ui);
const clm_byte4* sbx_GrainStack_getColorIC(const struct sbx_GrainStack* stack, uint32_t ii);
clm_byte4* sbx_GrainStack_getColorU(struct sbx_GrainStack* stack, uint32_t ui);

bool sbx_GrainStack_isVacant(const struct sbx_GrainStack* stack, uint32_t ii);
bool sbx_GrainStack_isActive(const struct sbx_GrainStack* stack, uint32_t ii);
bool sbx_GrainStack_shouldIntegrateI(const struct sbx_GrainStack* stack, uint32_t ii);

struct sbx_PhaseIndexList* sbx_GrainStack_getPhase(struct sbx_GrainStack* stack, enum sbx_GrainLayer phase);

void sbx_GrainStack_setGrainI(
  struct sbx_GrainStack* stack,
  uint32_t ii,
  const struct sbx_SandGrain* value,
  const clm_byte4* clearColor,
  const clm_byte4* color
);

void sbx_PhaseIndexList_pushBack(struct sbx_PhaseIndexList* list, uint32_t value);
void sbx_PhaseIndexList_swapRemove(struct sbx_PhaseIndexList* list, uint32_t index);