#pragma once

struct AssetInfo {
  AssetInfo(const std::string& location)
    : mUri(location)
    , mType(0)
    , mId(0) {
  }

  AssetInfo(size_t id)
    : mId(id)
    , mType(0) {
  }

  bool isEmpty() const {
    return !mId && mUri.empty();
  }

  static std::string getCategory(const std::string& uri) {
    size_t ext = uri.find_last_of('.');
    return ext != std::string::npos ? uri.substr(ext + 1) : "";
  }

  std::string mUri;
  size_t mId;
  size_t mType;
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

  AssetState getState() const {
    return mState;
  }
  const AssetInfo& getInfo() const {
    return mInfo;
  }
  RWLock& getLock() const {
    return mLock;
  }
  virtual bool isReady() const {
    return mState == AssetState::Loaded || mState == AssetState::PostProcessed;
  }
  template<class T>
  bool isOfType() const {
    assert(mInfo.mType && "Type should have been set on construction");
    return mInfo.mType == Asset::typeId<T>();
  }
  template<class T>
  static size_t typeId() {
    return ::typeId<Asset, T>();
  }
  template<class T>
  const T* cast() const {
    return isOfType<T>() ? static_cast<const T*>(this) : nullptr;
  }

protected:
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