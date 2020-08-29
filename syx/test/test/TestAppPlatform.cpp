#include "Precompile.h"
#include "test/TestAppPlatform.h"

#include "file/DirectoryWatcher.h"
#include "file/FilePath.h"
#include "TestKeyboardInput.h"

TestAppPlatform::TestAppPlatform()
  : mKeyboardInput(std::make_unique<TestKeyboardInputImpl>()) {
}

std::string TestAppPlatform::getExePath() {
  return {};
}

void TestAppPlatform::setWorkingDirectory(const char*) {
}

std::unique_ptr<DirectoryWatcher> TestAppPlatform::createDirectoryWatcher(FilePath) {
  return std::make_unique<DirectoryWatcher>();
}

KeyboardInputImpl& TestAppPlatform::getKeyboardInput() {
  return *mKeyboardInput;
}
