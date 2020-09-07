#include "Precompile.h"
#include "AppPlatformWin32.h"

#include "DirectoryWatcher32.h"
#include "file/FilePath.h"
#include "KeyboardInputWin32.h"
#include <Windows.h>

AppPlatformWin32::AppPlatformWin32()
  : mKeyboardInput(std::make_unique<KeyboardInputWin32>()) {
}

AppPlatformWin32::~AppPlatformWin32() {
}

std::string AppPlatformWin32::getExePath() {
  const size_t buffSize = _MAX_FNAME;
  TCHAR buff[buffSize];
  DWORD resultSize = GetModuleFileName(NULL, buff, buffSize);
  std::string result;
  result.resize(resultSize);
  for(size_t i = 0; i < result.size(); ++i)
    result[i] = static_cast<char>(buff[i]);
  return result;
}

void AppPlatformWin32::setWorkingDirectory(const char* working) {
  ::SetCurrentDirectoryA(working);
}

std::unique_ptr<DirectoryWatcher> AppPlatformWin32::createDirectoryWatcher(FilePath root) {
  return std::make_unique<DirectoryWatcher32>(root);
}

KeyboardInputImpl& AppPlatformWin32::getKeyboardInput() {
  return *mKeyboardInput;
}
