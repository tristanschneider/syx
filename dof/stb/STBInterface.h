#pragma once

struct ImageData {
  unsigned char* mBytes = nullptr;
  size_t mWidth = 0;
  size_t mHeight = 0;
};

struct STBInterface {
  static ImageData loadImageFromFile(const char* filename, int desiredChannels);
  static void deallocate(ImageData&& data);
};