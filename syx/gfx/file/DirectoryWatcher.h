#pragma once
#include <util/Observer.h>

class DirectoryObserver : public Observer<DirectoryObserver> {
public:
  virtual void onFileChanged(const std::string& filename) = 0;
  virtual void onFileAdded(const std::string& filename) = 0;
  virtual void onFileRemoved(const std::string& filename) = 0;
  virtual void onFileRenamed(const std::string& oldName, const std::string& newName) = 0;
};

class DirectoryWatcher {
public:
  virtual ~DirectoryWatcher() = default;

  void addObserver(DirectoryObserver& observer) {
    observer.observe(&mSubject);
  }

protected:
  DirectoryObserver::SubjectType mSubject;
};