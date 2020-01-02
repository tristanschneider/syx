#pragma once

#include "AppPlatform.h"

class DirectoryWatcher32;
class KeyboardInputWin32;

class AppPlatformWin32 : public AppPlatform {
public:
  AppPlatformWin32();
  ~AppPlatformWin32();

  std::string getExePath() override;
  void setWorkingDirectory(const char* working) override;
  std::unique_ptr<DirectoryWatcher> createDirectoryWatcher(FilePath root) override;
  KeyboardInputImpl& getKeyboardInput() override;

private:
  std::unique_ptr<KeyboardInputWin32> mKeyboardInput;
};
