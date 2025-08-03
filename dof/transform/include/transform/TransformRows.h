#pragma once

#include <SparseRow.h>
#include <Table.h>
#include <transform/Transform.h>

namespace Transform {
  struct WorldTransformRow : Row<PackedTransform> {};
  struct WorldInverseTransformRow : Row<PackedTransform> {};
  struct TransformNeedsUpdateRow : SparseFlagRow {};
}