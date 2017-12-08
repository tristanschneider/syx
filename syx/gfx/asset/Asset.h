#pragma once

struct AssetInfo {
  AssetInfo(const std::string& location)
    : mUri(location)
    , mId(0) {
  }

  static std::string getCategory(const std::string& uri) {
    size_t ext = uri.find_last_of('.');
    return ext != std::string::npos ? uri.substr(ext + 1) : "";
  }

  std::string mUri;
  size_t mId;
  std::string mCategory;
};

enum class AssetState : uint8_t {
  Empty,
  Loaded,
  Failed
};

class Asset {
public:
  friend class AssetRepo;

  Asset(const AssetInfo& info)
    : mInfo(info)
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

private:
  AssetInfo mInfo;
  AssetState mState;
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