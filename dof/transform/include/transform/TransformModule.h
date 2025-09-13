#pragma once

class IAppBuilder;
class IAppModule;
class StorageTableBuilder;

struct TableID;

namespace Transform {
  std::unique_ptr<IAppModule> createModule();
  //Register a task that ensures WorldTransformRow and WorldInverseTransformRow match.
  //This will always be the case at the beginning of the frame but doing this allows greater precision on demand.
  //Updates are based on TransformNeedsUpdateRow
  void updateInverse(IAppBuilder& builder, TableID table);
  StorageTableBuilder& addTransform25D(StorageTableBuilder& table);
  StorageTableBuilder& addPosXY(StorageTableBuilder& table);
  StorageTableBuilder& addTransform2D(StorageTableBuilder& table);
  StorageTableBuilder& addTransform2DNoScale(StorageTableBuilder& table);
}