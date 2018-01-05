#pragma once
//Input and graphics are still platform specific, but anything easily compartmentalized goes here

class DirectoryWatcher {
public:
  virtual void onFileChanged(const std::string& filename) = 0;
};

class AppPlatform {
public:
};