#include "Precompile.h"
#include "TextureLoader.h"

static const int sDataPosOffset = 0x0A;
static const int sImageSizeOffset = 0x22;
static const int sWidthOffset = 0x12;
static const int sHeightOffset = 0x16;

TextureLoader::Texture TextureLoader::loadBmp(const std::string& path) {
  std::ifstream stream(path);
  if(!stream.good()) {
    printf("Unable to open bmp at %s\n", path.c_str());
    return Texture();
  }

  const size_t headerSize = 54;
  unsigned char header[headerSize];
  stream.read(reinterpret_cast<char*>(header), headerSize);
  size_t bytesRead = static_cast<size_t>(stream.gcount());
  if(bytesRead != headerSize) {
    printf("Error reading header of bmp at %s\n", path.c_str());
    return Texture();
  }

  if(header[0] != 'B' || header[1] != 'M') {
    printf("Not a bmp file at %s\n", path.c_str());
    return Texture();
  }

  uint32_t dataStart = reinterpret_cast<uint32_t&>(header[sDataPosOffset]);
  uint32_t imageSize = reinterpret_cast<uint32_t&>(header[sImageSizeOffset]);
  uint16_t width = reinterpret_cast<uint16_t&>(header[sWidthOffset]);
  uint16_t height = reinterpret_cast<uint16_t&>(header[sHeightOffset]);

  //If the fields are missing, fill them in
  if(!imageSize)
    imageSize=width*height*3;
  if(!dataStart)
    dataStart = 54;

  mBuffer.clear();
  mBuffer.resize(imageSize);
  stream.seekg(dataStart);
  stream.read(reinterpret_cast<char*>(&mBuffer[0]), imageSize);
  bytesRead = static_cast<size_t>(stream.gcount());
  if(bytesRead != imageSize) {
    printf("Error reading bmp data at %s\n", path.c_str());
    return Texture();
  }

  return Texture(&mBuffer, width, height);
}
