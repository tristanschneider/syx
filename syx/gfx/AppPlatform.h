#pragma once
//Input and graphics are still platform specific, but anything easily compartmentalized goes here

class DirectoryWatcher : public Observer<DirectoryWatcher> {
public:
  virtual void onFileChanged(const std::string& filename) = 0;
  virtual void onFileAdded(const std::string& filename) = 0;
  virtual void onFileRemoved(const std::string& filename) = 0;
  virtual void onFileRenamed(const std::string& oldName, const std::string& newName) = 0;
};

class FocusEvents : public Observer<FocusEvents> {
public:
  virtual void onFocusGained() {}
  virtual void onFocusLost() {}
};

class AppPlatform {
public:
  void addFocusObserver(FocusEvents& o);
  void addDirectoryObserver(DirectoryWatcher& o);
  //Notify observers of events
  void onFocusGained();
  void onFocusLost();

  virtual std::string getExePath() = 0;

protected:
  FocusEvents::SubjectType mFocusSubject;
  DirectoryWatcher::SubjectType mDirectorySubject;
};