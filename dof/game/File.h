#pragma once

struct FileSystem;

namespace File {
  std::optional<std::string> readEntireFile(const FileSystem& fs, const std::string& name);
  bool writeEntireFile(const FileSystem& fs, const std::string& name, const std::string& buffer);
};