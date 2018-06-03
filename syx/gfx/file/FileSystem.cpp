#include "Precompile.h"
#include "file/FileSystem.h"

namespace FileSystem {
  bool fileExists(const char* filename) {
    if(std::FILE* file = std::fopen(filename, "rb")) {
      std::fclose(file);
      return true;
    }
    return false;
  }
}