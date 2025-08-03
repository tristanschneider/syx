#pragma once

class IAppModule;
class StorageTableBuilder;

namespace Transform {
  std::unique_ptr<IAppModule> createModule();
  StorageTableBuilder& addTransform25D(StorageTableBuilder& table);
  StorageTableBuilder& addPosXY(StorageTableBuilder& table);
  StorageTableBuilder& addTransform2D(StorageTableBuilder& table);
  StorageTableBuilder& addTransform2DNoScale(StorageTableBuilder& table);
}