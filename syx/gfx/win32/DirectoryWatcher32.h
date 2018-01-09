#pragma once
#include "AppPlatform.h"

class DirectoryWatcher32 {
public:
  DirectoryWatcher32(DirectoryWatcher::SubjectType& subject, const std::string& rootDir);
  ~DirectoryWatcher32();

private:
  void _watchLoop();

  DirectoryWatcher::SubjectType& mSubject;
  bool mTerminate;
  std::string mRootDir;
  void* mDirHandle;
  std::thread mWatchThread;
};