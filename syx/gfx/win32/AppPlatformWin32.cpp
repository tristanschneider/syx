#include "Precompile.h"
#include "win32/AppPlatformWin32.h"
#include "win32/DirectoryWatcher32.h"
#include <Windows.h>

AppPlatformWin32::AppPlatformWin32() {
  std::string dir = getExePath();
  //TODO: watch user's desired project path
  dir.resize(dir.find_last_of('\\') + 1);
  mDirectoryWatcher = std::make_unique<DirectoryWatcher32>(mDirectorySubject, dir);
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