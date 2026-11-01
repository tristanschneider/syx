#include <sandbox/GrainStack.h>

#include <sandbox/SandGrid.h>

#include <clm/byte4.h>
#include <std/Diagnostics.h>
#include <std/Swap.h>

struct sbx_ConstraintStorage {
  struct sbx_ConstraintSlot slots[8];
};

typedef struct sbx_SandGrain sbx_GrainT;
typedef struct sbx_ConstraintStorage sbx_ConstraintStorageT;
typedef uint32_t sbx_IdToIndexT;
typedef uint32_t sbx_IndexToIdT;
typedef clm_byte4 sbx_BitmapT;
typedef uint32_t sbx_IntegrationPhaseT;
typedef uint32_t sbx_IslandAddPhaseT;
typedef uint32_t sbx_IslandSplitPhaseT;
typedef uint32_t sbx_SolvePhaseT;
typedef uint32_t sbx_MovePhaseT;
typedef uint32_t sbx_TempPhaseT;

typedef struct sbx_PhaseIndexList sbx_PhaseDataT;

const int32_t SENTRY_PADDING = 2;
//Offset to convert user space x,y coordinates to sentry space coordinates
const int32_t USER_TO_SENTRY = 1;

struct sbx_InitLayerData;

struct sbx_LayerTraits {
  size_t stride;
  //Initialize layer and layerData if non-null, and always return size required to do so
  size_t(*initLayer)(struct sbx_InitLayerData*);
};

struct sbx_InitLayerData {
  //Traints of this layer from getLayerTraits
  struct sbx_LayerTraits traits;
  //Buffer to assign layer data to
  void* buffer;
  //The layer and data corresponding to the traits
  void** layer;
  union sbx_LayerData* layerData;
  //Sizes from the ctor
  size_t userCount;
  size_t internalCount;
};

// Initialize a layer with internal storage size and no layer data
size_t sbx_initInternalLayer(struct sbx_InitLayerData* layer) {
  if(layer->layer) {
    *layer->layer = layer->buffer;
  }
  return layer->traits.stride * layer->internalCount;
}

size_t sbx_initUserLayer(struct sbx_InitLayerData* layer) {
  if(layer->layer) {
    *layer->layer = layer->buffer;
  }
  return layer->traits.stride * layer->userCount;
}

size_t sbx_initPhase(struct sbx_InitLayerData* layer) {
  if(layer->layer && layer->layerData) {
    *layer->layer = layer->buffer;
    layer->layerData->phase = (struct sbx_PhaseIndexList){
      .array = *layer->layer,
      .size = 0
    };
  }
  return layer->traits.stride * layer->internalCount;
}

struct sbx_LayerTraits sbx_GrainStack_getLayerTraits(enum sbx_GrainLayer layer) {
  switch(layer) {
    case SBX_GLI_GRAIN:
      return (struct sbx_LayerTraits){ sizeof(sbx_GrainT), &sbx_initInternalLayer };
    case SBX_GLI_ID_TO_INDEX:
      return (struct sbx_LayerTraits){ sizeof(sbx_IdToIndexT), &sbx_initInternalLayer };
    case SBX_GLI_INDEX_TO_ID:
      return (struct sbx_LayerTraits){ sizeof(sbx_IndexToIdT), &sbx_initInternalLayer };
    case SBX_GLI_CONSTRAINT_STORAGE:
      return (struct sbx_LayerTraits){ sizeof(sbx_ConstraintStorageT), &sbx_initInternalLayer };
    case SBX_GLU_BITMAP:
      return (struct sbx_LayerTraits){ sizeof(sbx_BitmapT), &sbx_initUserLayer};
    case SBX_GLI_INTEGRATION_PHASE:
      return (struct sbx_LayerTraits){ sizeof(sbx_IntegrationPhaseT), &sbx_initPhase};
    case SBX_GLI_ISLAND_ADD_PHASE:
      return (struct sbx_LayerTraits){ sizeof(sbx_IslandAddPhaseT), &sbx_initPhase};
    case SBX_GLI_ISLAND_SPLIT_PHASE:
      return (struct sbx_LayerTraits){ sizeof(sbx_IslandSplitPhaseT), &sbx_initPhase};
    case SBX_GLI_SOLVE_PHASE:
      return (struct sbx_LayerTraits){ sizeof(sbx_SolvePhaseT), &sbx_initPhase};
    case SBX_GLI_MOVE_PHASE:
      return (struct sbx_LayerTraits){ sizeof(sbx_MovePhaseT), &sbx_initPhase};
    case SBX_GLI_TEMP_PHASE:
      return (struct sbx_LayerTraits){ sizeof(sbx_TempPhaseT), &sbx_initPhase};
    case SBX_GL_COUNT:
      break;
  }
  STD_ASSERT(false);
  struct sbx_LayerTraits none = { 0 };
  return none;
}

void sbx_GrainStack_ctor(struct sbx_GrainStack* dst, uint8_t* buffer, uint32_t userWidth, uint32_t userHeight, size_t* bufferSizeInOut) {
  STD_ASSERT(bufferSizeInOut);

  size_t requiredBytes = 0;
  struct sbx_InitLayerData initData = {
    .userCount = userWidth * userHeight,
    .internalCount = (userWidth + SENTRY_PADDING) * (userHeight + SENTRY_PADDING)
  };
  for(int i = 0; i < SBX_GL_COUNT; ++i) {
    initData.traits = sbx_GrainStack_getLayerTraits(i);
    requiredBytes += initData.traits.initLayer(&initData);
  }

  if(*bufferSizeInOut < requiredBytes) {
    *bufferSizeInOut = requiredBytes;
    return;
  }
  *bufferSizeInOut = requiredBytes;

  dst->internalWidth = userWidth + SENTRY_PADDING;
  dst->internalCount = initData.internalCount;
  dst->userCount = initData.userCount;

  for(int i = 0; i < SBX_GL_COUNT; ++i) {
    initData.traits = sbx_GrainStack_getLayerTraits(i);
    initData.buffer = buffer;
    initData.layer = &dst->layers[i];
    initData.layerData = &dst->layerData[i];

    buffer += initData.traits.initLayer(&initData);
  }
}

uint32_t sbx_GrainStack_itou(const struct sbx_GrainStack* stack, uint32_t ii) {
  const int32_t rows = ii / stack->internalWidth;
  //Sentry row count without counting the portions of the sentry columns that cut into it
  const int32_t sentryRow = stack->internalWidth - SENTRY_PADDING;
  //Every row passed means two sentry columns, and the current row has one more on the left
  const int32_t sentryColumns = rows * 2 + 1;
  return ii - sentryRow - sentryColumns;
}

uint32_t sbx_GrainStack_uxytoi(const struct sbx_GrainStack* stack, uint32_t ux, uint32_t uy) {
  return ux + USER_TO_SENTRY + stack->internalWidth * (uy + USER_TO_SENTRY);
}

struct sbx_SandGrain* sbx_GrainStack_getGrainI(struct sbx_GrainStack* stack, uint32_t ii) {
  sbx_GrainT* grains = stack->layers[SBX_GLI_GRAIN];
  STD_ASSERT(ii < stack->internalCount);
  return &grains[ii];
}

clm_byte4* sbx_GrainStack_getColorU(struct sbx_GrainStack* stack, uint32_t ui) {
  sbx_BitmapT* bmp = stack->layers[SBX_GLU_BITMAP];
  STD_ASSERT(ui < stack->userCount);
  return &bmp[ui];
}

const clm_byte4* sbx_GrainStack_getColorUC(const struct sbx_GrainStack* stack, uint32_t ui) {
  return sbx_GrainStack_getColorU((struct sbx_GrainStack*)stack, ui);
}

const clm_byte4* sbx_GrainStack_getColorIC(const struct sbx_GrainStack* stack, uint32_t ii) {
  return sbx_GrainStack_getColorUC(stack, sbx_GrainStack_itou(stack, ii));
}

uint32_t sbx_GrainStack_getSentryPadding() {
  return SENTRY_PADDING;
}

const struct sbx_SandGrain* sbx_GrainStack_getGrainIC(const struct sbx_GrainStack* stack, uint32_t ii) {
  return sbx_GrainStack_getGrainI((struct sbx_GrainStack*)stack, ii);
}

bool sbx_GrainStack_isVacant(const struct sbx_GrainStack* stack, uint32_t ii) {
  return sbx_GrainStack_getGrainIC(stack, ii)->type == SBX_GT_EMPTY;
}

bool sbx_GrainStack_isActive(const struct sbx_GrainStack* stack, uint32_t ii) {
  return sbx_GrainStack_getGrainIC(stack, ii)->type == SBX_GT_GRAIN;
}

bool sbx_GrainStack_shouldIntegrateI(const struct sbx_GrainStack* stack, uint32_t ii) {
  return sbx_GrainStack_getGrainIC(stack, ii)->inverseMass != 0.f;
}

bool sbx_GrainStack_trySetAge(struct sbx_GrainStack* stack, uint32_t ii, uint8_t age) {
  struct sbx_SandGrain* grain = sbx_GrainStack_getGrainI(stack, ii);
  bool result = grain->age != age;
  grain->age = age;
  return result;
}

bool sbx_GrainStack_isOldAge(const struct sbx_GrainStack* stack, uint32_t ii, uint8_t age) {
  return sbx_GrainStack_getGrainIC(stack, ii)->age != age;
}

uint8_t sbx_getConstraintSlotIndex(uint32_t stride, uint32_t ia, uint32_t ib) {
  uint8_t slot = 0;
  const uint32_t cornerA = ia - 1 - stride;
  //Address slots around A like this where A is at 4:
  //0 1 2
  //3 4 5
  //6 7 8
  for(uint32_t y = 0; y < 3; ++y) {
    for(uint32_t x = 0; x < 3; ++x) {
      //Middle is wher A is, skip it, and don't count it as a slot
      if(y == 1 && x == 1) {
        continue;
      }
      if(ib == cornerA + x + stride * y) {
        return slot;
      }
      ++slot;
    }
  }
  STD_UNREACHABLE();
  return 0;
}

struct sbx_ConstraintSlot* sbx_GrainStack_getConstraintSlot(struct sbx_GrainStack* stack, uint32_t ia, uint32_t ib) {
  //So this always produces the same result regardless of parameter order
  if(ia > ib) {
    STD_SWAP_U32(ia, ib);
  }
  //TODO: wasted space, can make each grain only store constraints in its lower half
  const uint8_t slotIndex = sbx_getConstraintSlotIndex(stack->internalWidth, ia, ib);
  sbx_ConstraintStorageT* storage = stack->layers[SBX_GLI_CONSTRAINT_STORAGE];
  return &storage[ia].slots[slotIndex];
}

struct sbx_PhaseIndexList* sbx_GrainStack_getPhase(struct sbx_GrainStack* stack, enum sbx_GrainLayer phase) {
  return &stack->layerData[phase].phase;
}

void sbx_GrainStack_setGrainI(
  struct sbx_GrainStack* stack,
  uint32_t ii,
  const struct sbx_SandGrain* value,
  const clm_byte4* clearColor,
  const clm_byte4* color
) {
  struct sbx_SandGrain* grain = sbx_GrainStack_getGrainI(stack, ii);

  *grain = *value;

  *sbx_GrainStack_getColorU(stack, sbx_GrainStack_itou(stack, ii)) = sbx_GrainStack_isVacant(stack, ii) ? *clearColor : *color;
}


void sbx_PhaseIndexList_pushBack(struct sbx_PhaseIndexList* list, uint32_t value) {
  list->array[list->size++] = value;
}

void sbx_PhaseIndexList_swapRemove(struct sbx_PhaseIndexList* list, uint32_t index) {
  STD_ASSERT(list->size);
  STD_ASSERT(index < list->size);
  list->array[index] = list->array[list->size - 1];
  --(list->size);
}

void sbx_PhaseIndexList_clear(struct sbx_PhaseIndexList* list) {
  list->size = 0;
}