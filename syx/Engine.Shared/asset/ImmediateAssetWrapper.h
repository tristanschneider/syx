#pragma once

class Asset;
struct AssetInfo;
class EventBuffer;
class System;

//A convenience class that wraps the creation event round trip behind the Asset interface so an Asset can
//be immediately created without having to wait for the response
namespace ImmediateAsset {
  std::shared_ptr<Asset> create(AssetInfo info, EventBuffer& msg, typeId_t<System> systemResponseHandler);
};
