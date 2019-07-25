#pragma once
#include "file/DirectoryWatcher.h"
#include "file/FilePath.h"

class DirectoryWatcher32 : public DirectoryWatcher {
public:
  DirectoryWatcher32(FilePath rootDir);
  ~DirectoryWatcher32();

private:
  void _watchLoop();

  bool mTerminate;
  FilePath mRootDir;
  void* mDirHandle;
  std::thread mWatchThread;
};