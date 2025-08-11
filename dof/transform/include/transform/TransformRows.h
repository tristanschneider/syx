#pragma once

#include <SparseRow.h>
#include <Table.h>
#include <transform/Transform.h>

namespace Transform {
  struct WorldTransformRow : Row<PackedTransform> {};
  struct WorldInverseTransformRow : Row<PackedTransform> {};
  //Flagged by a caller that has modified a transform to notify the transform module it needs to update
  struct TransformNeedsUpdateRow : SparseFlagRow {};
  //Flagged by the transform module when it updates a transform so other modules can react to changed transforms.
  //Unlike TransformNeedsUpdateRow, this can be observed regardless of module registration order.
  struct TransformHasUpdatedRow : SparseFlagRow {};
}