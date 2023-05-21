#include "File.h"

#include "Simulation.h"

namespace File {
  std::string getFilename(const FileSystem& fs, const std::string& name) {
    return fs.mRoot + name;
  }

  std::optional<std::string> readEntireFile(const FileSystem& fs, const std::string& name) {
    std::ifstream stream(getFilename(fs, name), std::ios::binary | std::ios::ate);
    if(stream.good()) {
      auto size = stream.tellg();
      assert(size < 10000);
      if(size >= 10000) {
        return {};
      }
      std::string buffer(static_cast<size_t>(size), 0);
      stream.seekg(0);
      stream.read(buffer.data(), size);
      return buffer;
    }
    return {};
  }

  bool writeEntireFile(const FileSystem& fs, const std::string& name, const std::string& buffer) {
    if(std::ofstream stream(getFilename(fs, name), std::ios::binary); stream.good()) {
      stream << buffer;
      return stream.good();
    }
    return false;
  }
}