#pragma once
#include "AppPlatform.h"

class TestAppPlatform : public AppPlatform {
public:
  TestAppPlatform();
  virtual std::string getExePath() override;
  virtual void setWorkingDirectory(const char* working) override;
  virtual std::unique_ptr<DirectoryWatcher> createDirectoryWatcher(FilePath root) override;
  virtual KeyboardInputImpl& getKeyboardInput() override;
  virtual std::unique_ptr<FileSystem::IFileSystem> createFileSystem();

private:
  std::unique_ptr<KeyboardInputImpl> mKeyboardInput;
};
