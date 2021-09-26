#pragma once
#include "threading/RWLock.h"
#include "util/TypeId.h"

class Asset;

struct AssetInfo {
  AssetInfo(std::string location)
    : mUri(std::move(location))
    , mType(0)
    , mId(0) {
  }

  AssetInfo(size_t id)
    : mId(id)
    , mType(0) {
  }

  void fill() {
    if(!mUri.empty()) {
      mId = std::hash<std::string>()(mUri);
      mCategory = AssetInfo::getCategory(mUri);
    }
  }

  bool isEmpty() const {
    return !mId && mUri.empty();
  }

  static std::string getCategory(const std::string& uri) {
    size_t ext = uri.find_last_of('.');
    return ext != std::string::npos ? uri.substr(ext + 1) : "";
  }

  //TODO: change this to FilePath
  std::string mUri;
  size_t mId;
  typeId_t<Asset> mType;
  std::string mCategory;
};

enum class AssetState : uint8_t {
  Empty,
  Loaded,
  //Up to the implementation to use this as an extra step after asset load
  PostProcessed,
  Failed
};

class Asset {
public:
  DECLARE_TYPE_CATEGORY
  friend class AssetRepo;

  Asset(AssetInfo&& info)
    : mInfo(std::move(info))
    , mState(AssetState::Empty) {
  }
  virtual ~Asset() {
  }

  virtual AssetState getState() const {
    return mState;
  }
  virtual const AssetInfo& getInfo() const {
    return mInfo;
  }
  RWLock& getLock() const {
    return mLock;
  }
  bool isReady() const {
    return mState == AssetState::Loaded || mState == AssetState::PostProcessed;
  }
  template<class T>
  bool isOfType() const {
    //TODO: this assert only makes sense if mType is an optional, otherwise 0 may be a valid type
    //assert(mInfo.mType && "Type should have been set on construction");
    return getInfo().mType == Asset::typeId<T>();
  }
  template<class T>
  static typeId_t<Asset> typeId() {
    return ::typeId<T, Asset>();
  }
  template<class T>
  const T* cast() const {
    const Asset* self = _tryUnwrap();
    return self && isOfType<T>() ? static_cast<const T*>(self) : nullptr;
  }
  template<class T>
  T* cast() {
    Asset* self = const_cast<Asset*>(_tryUnwrap());
    return self && isOfType<T>() ? static_cast<T*>(self) : nullptr;
  }

protected:
  virtual const Asset* _tryUnwrap() const {
    return this;
  }

  AssetState mState;
  mutable RWLock mLock;

private:
  AssetInfo mInfo;
};

class TextAsset : public Asset {
public:
  using Asset::Asset;
  virtual ~TextAsset() {
  }

  void set(std::string&& data) {
    mData = std::move(data);
  }
  const std::string& get() const {
    return mData;
  }

private:
  std::string mData;
};

class BufferAsset : public Asset {
public:
  using Asset::Asset;
  virtual ~BufferAsset() {
  };

  void set(std::vector<uint8_t>&& data) {
    mData = std::move(data);
  }
  const std::vector<uint8_t>& get() const {
    return mData;
  }

private:
  std::vector<uint8_t> mData;
};