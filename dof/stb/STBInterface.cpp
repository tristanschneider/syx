#include "STBInterface.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

ImageData STBInterface::loadImageFromFile(const char* filename, int desiredChannels) {
  int width, height, fileChannels;
  stbi_uc* bytes = stbi_load(filename, &width, &height, &fileChannels, desiredChannels);
  return {
    bytes,
    size_t(width),
    size_t(height)
  };
}

ImageData STBInterface::loadImageFromBuffer(const unsigned char* bytes, size_t size, int desiredChannels) {
  int width, height, fileChannels;
  stbi_uc* result = stbi_load_from_memory(bytes, static_cast<int>(size), &width, &height, &fileChannels, desiredChannels);
  return {
    result,
    size_t(width),
    size_t(height)
  };
}


void STBInterface::deallocate(ImageData&& data) {
  if(data.mBytes) {
    stbi_image_free(data.mBytes);
  }
}
