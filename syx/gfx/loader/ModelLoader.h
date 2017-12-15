#pragma once
#include "loader/AssetLoader.h"

class Model;

class ModelOBJLoader : public TextAssetLoader {
protected:
  using TextAssetLoader::TextAssetLoader;
  AssetLoadResult _load(Asset& asset) override;
  void postProcess(App& app, Asset& asset) override;

private:
  struct V3 {
    float x, y, z;
  };
  struct V2 {
    float x, y;
  };
  struct VertLookup {
    struct Hasher {
      size_t operator()(const VertLookup& v) const {
        size_t result = 0;
        Util::hashCombine(result, v.mVert, v.mNormal, v.mUV);
        return result;
      }
    };

    bool operator==(const VertLookup& rhs) const {
      return mVert == rhs.mVert
        && mNormal == rhs.mNormal
        && mUV == rhs.mUV;
    }

    VertLookup() {}
    VertLookup(size_t vert, size_t normal, size_t uv)
      : mVert(vert)
      , mNormal(normal)
      , mUV(uv) {
    }
    size_t mVert, mNormal, mUV;
  };

  void _reset();
  bool _readLine();
  //Get the index or create a new one for the given vert/normal/uv combo
  size_t _getVertIndex(const VertLookup& lookup);
  void _addTri(size_t a, size_t b, size_t c);

  Model* mModel;
  AssetLoadResult mResultState;
  std::vector<V3> mVerts;
  std::vector<V3> mNormals;
  std::vector<V2> mUVs;
  //Map VertLookup to the corresponding Vertex in the model buffer
  std::unordered_map<VertLookup, size_t, VertLookup::Hasher> mVertToIndex;
};

