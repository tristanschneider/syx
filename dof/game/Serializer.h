#pragma once

//Unsafe serializer/deserializer intended for taking/restoring snapshots for debugging purposes
//There is no validation to detect file corruption or to properly handle a different data layout
//while reading than whatever that type was when it was written. That also means snapshots
//should likely only be used on the same device, or at least architecture

#include <iostream>

#include "Simulation.h"

using SerializeStream = std::basic_ostream<uint8_t>;
using DeserializeStream = std::basic_istream<uint8_t>;

template<class T, class E = void>
struct Serializer {};

//Can be used for types that want to opt out
template<class T>
struct NoOpSerializer {
  static void serialize(const T&, SerializeStream&) {}
  static bool deserialize(DeserializeStream&, T&) { return true; }
};

//Unsafe, intended only for debugging
template<class RowT>
struct MemCpySerializer {
  static void serialize(const RowT& row, SerializeStream& stream) {
    const size_t size = row.size();
    stream.write(reinterpret_cast<const uint8_t*>(&size), sizeof(size));
    if(size > 0) {
      stream.write(reinterpret_cast<const uint8_t*>(&row.at(0)), size*sizeof(typename RowT::ElementT));
    }
  }

  static bool deserialize(DeserializeStream& stream, RowT& result) {
    size_t size{};
    stream.read(reinterpret_cast<uint8_t*>(&size), sizeof(size));
    //TODO: validate somehow
    if(size > 0) {
      result.resize(size);
      stream.read(reinterpret_cast<uint8_t*>(&result.at(0)), size*sizeof(typename RowT::ElementT));
    }
    return true;
  }
};

//Same as above but for shared rows
template<class RowT>
struct MemCpySingleSerializer {
  static void serialize(const RowT& row, SerializeStream& stream) {
    stream.write(reinterpret_cast<const uint8_t*>(&row.at(0)), sizeof(typename RowT::ElementT));
  }

  static bool deserialize(DeserializeStream& stream, RowT& result) {
    stream.read(reinterpret_cast<uint8_t*>(&result.at(0)), sizeof(typename RowT::ElementT));
    return true;
  }
};

//Memcpy numeric types
template<class R>
struct Serializer<R, std::enable_if_t<std::is_same_v<std::true_type, typename R::IsBasicRow> && std::is_integral_v<typename R::ElementT>>>
  : MemCpySerializer<R> {};
template<class R>
struct Serializer<R, std::enable_if_t<std::is_same_v<std::true_type, typename R::IsBasicRow> && std::is_floating_point_v<typename R::ElementT>>>
  : MemCpySerializer<R> {};
template<class R>
struct Serializer<R, std::enable_if_t<std::is_same_v<std::true_type, typename R::IsSharedRow> && std::is_integral_v<typename R::ElementT>>>
  : MemCpySingleSerializer<R> {};
template<class R>
struct Serializer<R, std::enable_if_t<std::is_same_v<std::true_type, typename R::IsSharedRow> && std::is_floating_point_v<typename R::ElementT>>>
: MemCpySingleSerializer<R> {};
//Memcpy StableElementIds
template<class R>
struct Serializer<R, std::enable_if_t<std::is_same_v<std::true_type, typename R::IsBasicRow> && std::is_same_v<StableElementID, typename R::ElementT>>>
  : MemCpySerializer<R> {};

template<class... Rows>
struct Serializer<Table<Rows...>> {
  using T = Table<Rows...>;
  static void serialize(const T& table, SerializeStream& stream) {
    (Serializer<Rows>::serialize(std::get<Rows>(table.mRows), stream), ...);
  }

  static bool deserialize(DeserializeStream& stream, T& table) {
    return (Serializer<Rows>::deserialize(stream, std::get<Rows>(table.mRows)) && ...);
  }
};

template<class... Tables>
struct Serializer<Database<Tables...>> {
  using DB = Database<Tables...>;
  static void serialize(const DB& db, SerializeStream& stream) {
    (Serializer<Tables>::serialize(std::get<Tables>(db.mTables), stream), ...);
  }

  static bool deserialize(DeserializeStream& stream, DB& db) {
    return (Serializer<Tables>::deserialize(stream, std::get<Tables>(db.mTables)) && ...);
  }
};

namespace details {
  template<class T>
  void memCpySerialize(const std::vector<T>& toWrite, SerializeStream& stream) {
    const size_t size = toWrite.size();
    stream.write(reinterpret_cast<const uint8_t*>(&size), sizeof(size));
    if(size > 0) {
      stream.write(reinterpret_cast<const uint8_t*>(toWrite.data()), size*sizeof(T));
    }
  }

  template<class T>
  static bool memCpyDeserialize(DeserializeStream& stream, std::vector<T>& result) {
    size_t size{};
    stream.read(reinterpret_cast<uint8_t*>(&size), sizeof(size));
    //TODO: validate somehow
    if(size > 0) {
      result.resize(size);
      stream.read(reinterpret_cast<uint8_t*>(result.data()), size*sizeof(T));
    }
    return true;
  }

  template<class T>
  void memCpySerialize(const T& t, SerializeStream& stream) {
    stream.write(reinterpret_cast<const uint8_t*>(&t), sizeof(T));
  }

  template<class T>
  void memCpyDeserialize(DeserializeStream& stream, T& t) {
    stream.read(reinterpret_cast<uint8_t*>(&t), sizeof(T));
  }

  template<class K, class V>
  void memCpySerialize(const std::unordered_map<K, V>& toWrite, SerializeStream& stream) {
    const size_t size = toWrite.size();
    stream.write(reinterpret_cast<const uint8_t*>(&size), sizeof(size));
    for(auto&& pair : toWrite) {
      //TODO: decltype would be more accurate if there were specializations that didn't actually result in storing K,V
      stream.write(reinterpret_cast<const uint8_t*>(&pair.first), sizeof(K));
      stream.write(reinterpret_cast<const uint8_t*>(&pair.second), sizeof(V));
    }
  }

  template<class K, class V>
  static bool memCpyDeserialize(DeserializeStream& stream, std::unordered_map<K, V>& result) {
    size_t size{};
    stream.read(reinterpret_cast<uint8_t*>(&size), sizeof(size));
    result.reserve(size);
    for(size_t i = 0; i < size; ++i) {
      std::pair<K, V> pair;
      stream.read(reinterpret_cast<uint8_t*>(&pair.first), sizeof(K));
      stream.read(reinterpret_cast<uint8_t*>(&pair.second), sizeof(V));
      result.insert(std::move(pair));
    }
    return true;
  }
}

//Specializations
template<> struct Serializer<SharedRow<SceneState>> : MemCpySingleSerializer<SharedRow<SceneState>>{};
template<> struct Serializer<SharedRow<PhysicsTableIds>> : MemCpySingleSerializer<SharedRow<PhysicsTableIds>>{};

template<> struct Serializer<SharedRow<Scheduler>> : NoOpSerializer<SharedRow<Scheduler>>{};
template<> struct Serializer<Row<CubeSprite>> : MemCpySerializer<Row<CubeSprite>> {};
template<> struct Serializer<SharedRow<TextureReference>> : NoOpSerializer<SharedRow<TextureReference>>{};
template<> struct Serializer<Row<PlayerInput>> : MemCpySerializer<Row<PlayerInput>> {};
template<> struct Serializer<Row<PlayerKeyboardInput>> : MemCpySerializer<Row<PlayerKeyboardInput>> {};
template<> struct Serializer<Row<DebugPoint>> : MemCpySerializer<Row<DebugPoint>> {};
template<> struct Serializer<Row<Camera>> : MemCpySerializer<Row<Camera>> {};
template<> struct Serializer<Row<DebugCameraControl>> : MemCpySerializer<Row<DebugCameraControl>> {};
template<> struct Serializer<ThreadLocalsRow> : MemCpySerializer<ThreadLocalsRow> {};
template<> struct Serializer<ExternalDatabasesRow> : MemCpySerializer<ExternalDatabasesRow> {};

template<> struct Serializer<SharedRow<FileSystem>> : NoOpSerializer<SharedRow<FileSystem>>{};
template<> struct Serializer<Row<TextureLoadRequest>> : NoOpSerializer<Row<TextureLoadRequest>>{};
template<> struct Serializer<SharedRow<GameConfig>> : NoOpSerializer<SharedRow<GameConfig>>{};


//TODO: restore this
template<>
struct Serializer<SharedRow<StableElementMappings>> {
  using SelfT = SharedRow<StableElementMappings>;
  static void serialize(const SelfT& mappings,[[maybe_unused]] SerializeStream& stream) {
    [[maybe_unused]] const StableElementMappings& m = mappings.at();
    //details::memCpySerialize(m.mKeygen, stream);
    //details::memCpySerialize(m.mStableToUnstable, stream);
  }

  static bool deserialize([[maybe_unused]] DeserializeStream& stream, SelfT& mappings) {
    [[maybe_unused]] StableElementMappings& m = mappings.at();
    //details::memCpyDeserialize(stream, m.mKeygen);
    //details::memCpyDeserialize(stream, m.mStableToUnstable);
    return true;
  }
};

template<>
struct Serializer<SharedRow<ConstraintsTableMappings>> {
  using SelfT = SharedRow<ConstraintsTableMappings>;
  static void serialize(const SelfT& mappings, SerializeStream& stream) {
    const ConstraintsTableMappings& m = mappings.at();
    details::memCpySerialize(m.mConstraintFreeList, stream);
    details::memCpySerialize(m.mZeroMassStartIndex, stream);
  }

  static bool deserialize(DeserializeStream& stream, SelfT& mappings) {
    ConstraintsTableMappings& m = mappings.at();
    details::memCpyDeserialize(stream, m.mConstraintFreeList);
    details::memCpyDeserialize(stream, m.mZeroMassStartIndex);
    return true;
  }
};

template<>
struct Serializer<SharedRow<Sweep2D>> {
  using SelfT = SharedRow<Sweep2D>;
  static void serialize(const SelfT& toWrite, SerializeStream& stream) {
    const auto& v = toWrite.at();
    details::memCpySerialize(v.mX, stream);
    details::memCpySerialize(v.mY, stream);
    details::memCpySerialize(v.mGained, stream);
    details::memCpySerialize(v.mLost, stream);
    details::memCpySerialize(v.mContaining, stream);
    details::memCpySerialize(v.mKeyToBoundaries, stream);
  }

  static bool deserialize(DeserializeStream& stream, SelfT& toRead) {
    auto& v = toRead.at();
    details::memCpyDeserialize(stream, v.mX);
    details::memCpyDeserialize(stream, v.mY);
    details::memCpyDeserialize(stream, v.mGained);
    details::memCpyDeserialize(stream, v.mLost);
    details::memCpyDeserialize(stream, v.mContaining);
    details::memCpyDeserialize(stream, v.mKeyToBoundaries);
    return true;
  }
};

template<>
struct Serializer<SharedRow<SweepNPruneBroadphase::PairChanges>> {
  using SelfT = SharedRow<SweepNPruneBroadphase::PairChanges>;
  static void serialize(const SelfT& toWrite, SerializeStream& stream) {
    const auto& v = toWrite.at();
    details::memCpySerialize(v.mGained, stream);
    details::memCpySerialize(v.mLost, stream);
  }

  static bool deserialize(DeserializeStream& stream, SelfT& toRead) {
    auto& v = toRead.at();
    details::memCpyDeserialize(stream, v.mGained);
    details::memCpyDeserialize(stream, v.mLost);
    return true;
  }
};

template<>
struct Serializer<SharedRow<SweepNPruneBroadphase::ChangedCollisionPairs>> {
  using SelfT = SharedRow<SweepNPruneBroadphase::ChangedCollisionPairs>;
  static void serialize(const SelfT& toWrite, SerializeStream& stream) {
    const auto& v = toWrite.at();
    details::memCpySerialize(v.mGained, stream);
    details::memCpySerialize(v.mLost, stream);
  }

  static bool deserialize(DeserializeStream& stream, SelfT& toRead) {
    auto& v = toRead.at();
    details::memCpyDeserialize(stream, v.mGained);
    details::memCpyDeserialize(stream, v.mLost);
    return true;
  }
};

template<>
struct Serializer<SharedRow<SweepNPruneBroadphase::CollisionPairMappings>> {
  using SelfT = SharedRow<SweepNPruneBroadphase::CollisionPairMappings>;
  static void serialize(const SelfT& toWrite, SerializeStream& stream) {
    const auto& v = toWrite.at();
    details::memCpySerialize(v.mCollisionTableIndexToSweepPair, stream);
    details::memCpySerialize(v.mSweepPairToCollisionTableIndex, stream);
  }

  static bool deserialize(DeserializeStream& stream, SelfT& toRead) {
    auto& v = toRead.at();
    details::memCpyDeserialize(stream, v.mCollisionTableIndexToSweepPair);
    details::memCpyDeserialize(stream, v.mSweepPairToCollisionTableIndex);
    return true;
  }
};

template<>
struct Serializer<SharedRow<FinalSyncIndices>> {
  using SelfT = SharedRow<FinalSyncIndices>;
  static void serialize(const SelfT& toWrite, SerializeStream& stream) {
    const auto& v = toWrite.at();
    details::memCpySerialize(v.mMappingsA, stream);
    details::memCpySerialize(v.mMappingsB, stream);
  }

  static bool deserialize(DeserializeStream& stream, SelfT& toRead) {
    auto& v = toRead.at();
    details::memCpyDeserialize(stream, v.mMappingsA);
    details::memCpyDeserialize(stream, v.mMappingsB);
    return true;
  }
};

template<>
struct Serializer<ConstraintData::SharedVisitDataRow> {
  using SelfT = ConstraintData::SharedVisitDataRow;
  static void serialize(const SelfT& toWrite, SerializeStream& stream) {
    const auto& v = toWrite.at();
    details::memCpySerialize(v.mVisited, stream);
  }

  static bool deserialize(DeserializeStream& stream, SelfT& toRead) {
    auto& v = toRead.at();
    details::memCpyDeserialize(stream, v.mVisited);
    return true;
  }
};