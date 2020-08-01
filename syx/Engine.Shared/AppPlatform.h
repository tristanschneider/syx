#pragma once
#include "util/Observer.h"
//Input and graphics are still platform specific, but anything easily compartmentalized goes here

class DirectoryWatcher;
class FilePath;
class KeyboardInputImpl;

class FocusEvents : public Observer<FocusEvents> {
public:
  virtual ~FocusEvents() = default;
  virtual void onFocusGained() {}
  virtual void onFocusLost() {}
};

class DragDropObserver : public Observer<DragDropObserver> {
public:
  virtual ~DragDropObserver() = default;
  virtual void onDrop(const std::vector<FilePath>& files) = 0;
};

class AppPlatform {
public:
  virtual ~AppPlatform() = default;
  void addFocusObserver(FocusEvents& o);
  void addDragDropObserver(DragDropObserver& o);
  //Notify observers of events
  void onFocusGained();
  void onFocusLost();
  void onDrop(const std::vector<FilePath>& files);

  virtual std::string getExePath() = 0;
  virtual void setWorkingDirectory(const char* working) = 0;
  virtual std::unique_ptr<DirectoryWatcher> createDirectoryWatcher(FilePath root) = 0;

  virtual KeyboardInputImpl& getKeyboardInput() = 0;

protected:
  FocusEvents::SubjectType mFocusSubject;
  DragDropObserver::SubjectType mDragDropSubject;
};