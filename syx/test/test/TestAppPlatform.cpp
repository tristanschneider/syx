#include "Precompile.h"
#include "test/TestAppPlatform.h"

#include "file/DirectoryWatcher.h"
#include "file/FilePath.h"
#include "TestFileSystem.h"

TestAppPlatform::TestAppPlatform() = default;

std::string TestAppPlatform::getExePath() {
  return {};
}

void TestAppPlatform::setWorkingDirectory(const char*) {
}

std::unique_ptr<DirectoryWatcher> TestAppPlatform::createDirectoryWatcher(FilePath) {
  return std::make_unique<DirectoryWatcher>();
}

std::unique_ptr<FileSystem::IFileSystem> TestAppPlatform::createFileSystem() {
  return std::make_unique<FileSystem::TestFileSystem>();
}
