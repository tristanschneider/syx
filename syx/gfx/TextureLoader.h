#pragma once

class TextureLoader {
public:
  struct Texture {
    Texture(std::vector<unsigned char>* buffer = nullptr, size_t width = 0, size_t height = 0)
      : mBuffer(buffer)
      , mWidth(width)
      , mHeight(height) {
    }

    size_t mWidth, mHeight;
    std::vector<unsigned char>* mBuffer;
  };

  Texture loadBmp(const std::string& path);

private:
  std::vector<unsigned char> mBuffer;
  //Buffer used temporarily to transform from bgr to rgba
  std::vector<unsigned char> mTempConvert;
};