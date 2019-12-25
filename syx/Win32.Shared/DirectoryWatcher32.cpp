#include "Precompile.h"
#include "DirectoryWatcher32.h"

DirectoryWatcher32::DirectoryWatcher32(FilePath rootDir)
  : mTerminate(false)
  , mRootDir(rootDir)
  , mDirHandle(nullptr)
  , mWatchThread(&DirectoryWatcher32::_watchLoop, this) {
}

DirectoryWatcher32::~DirectoryWatcher32() {
  mTerminate = true;
  //Cancel an existing wait task if it exists
  CancelIoEx(mDirHandle, NULL);
  mWatchThread.join();
  CloseHandle(mDirHandle);
}

void DirectoryWatcher32::_watchLoop() {
  TCHAR dirName[_MAX_FNAME];
  size_t i;
  //Copy root dir into dirName as tchar, leave one slot for terminating character
  for(i = 0; i + 1 < _MAX_FNAME && i < mRootDir.size(); ++i)
    dirName[i] = static_cast<TCHAR>(mRootDir[i]);
  dirName[i] = 0;

  //Read access to directory, allow full sharing, and use backup semantic as required for directories
  HANDLE handle = CreateFile(dirName, GENERIC_READ,
    FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS,
    NULL);
  mDirHandle = static_cast<void*>(handle);

  if(handle == INVALID_HANDLE_VALUE) {
    printf("Failed to get diretory handle\n");
    return;
  }

  const size_t infoBufferSize = 100*sizeof(DWORD);
  //Read function takes a dword aligned buffer of whatever size, each element is variable size since it contains the filename string
  uint8_t infoBuffer[infoBufferSize];
  std::string stringBuff;
  std::string oldFilename;
  while(!mTerminate) {
    DWORD bytesRead = 0;
    BOOL success = ReadDirectoryChangesW(handle, static_cast<LPVOID>(infoBuffer), infoBufferSize, TRUE,
      FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
      &bytesRead, NULL, NULL);

    if(success == FALSE || !bytesRead) {
      if(mTerminate)
        printf("Terminating directory watcher\n");
      else
        printf("Failed to read directory with error code %i\n", static_cast<int>(GetLastError()));
      continue;
    }

    FILE_NOTIFY_INFORMATION* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(infoBuffer);
    //Next entry offset is number of bytes to next element, or 0 if this is the end
    while(true) {
      //Coppy wchar buffer into char buffer, then notify observers
      stringBuff.resize(info->FileNameLength/sizeof(wchar_t));
      for(i = 0; i < stringBuff.size(); ++i)
        stringBuff[i] = static_cast<char>(info->FileName[i]);

      switch(info->Action) {
        case FILE_ACTION_ADDED: CallOnObserversPtr(mSubject, onFileAdded, stringBuff); break;
        case FILE_ACTION_REMOVED: CallOnObserversPtr(mSubject, onFileRemoved, stringBuff); break;
        case FILE_ACTION_MODIFIED: CallOnObserversPtr(mSubject, onFileChanged, stringBuff); break;
        case FILE_ACTION_RENAMED_NEW_NAME: CallOnObserversPtr(mSubject, onFileRenamed, oldFilename, stringBuff); break;
        case FILE_ACTION_RENAMED_OLD_NAME: oldFilename = stringBuff; break;
      }

      if(!info->NextEntryOffset)
        break;
      info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<uint8_t*>(info) + info->NextEntryOffset);
    }
  }
}