#pragma once
#include "AppPlatform.h"

class DirectoryWatcher32;

class AppPlatformWin32 : public AppPlatform {
public:
  AppPlatformWin32();
  ~AppPlatformWin32();

  std::string getExePath() override;

private:
  std::unique_ptr<DirectoryWatcher32> mDirectoryWatcher;
};