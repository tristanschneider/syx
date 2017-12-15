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

  mTempConvert.clear();
  //Resize in preparation for the fourth component we'll add to each pixel
  mTempConvert.resize(imageSize);
  stream.seekg(dataStart);
  stream.read(reinterpret_cast<char*>(&mTempConvert[0]), imageSize);
  bytesRead = static_cast<size_t>(stream.gcount());
  if(bytesRead != imageSize) {
    printf("Error reading bmp data at %s\n", path.c_str());
    return Texture();
  }

  mBuffer.clear();
  mBuffer.resize(width*height*4);
  for(uint16_t i = 0; i < width*height; ++i) {
    size_t bp = 4*i;
    size_t tp = 3*i;
    //Flip bgr to rgb and add empty alpha
    mBuffer[bp] = mTempConvert[tp + 2];
    mBuffer[bp + 1] = mTempConvert[tp + 1];
    mBuffer[bp + 2] = mTempConvert[tp];
    mBuffer[bp + 3] = 0;
  }

  return Texture(&mBuffer, width, height);
}
