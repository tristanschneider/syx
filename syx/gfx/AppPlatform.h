#pragma once
//Input and graphics are still platform specific, but anything easily compartmentalized goes here

class DirectoryWatcher {
public:
  using ObserverType = Observer<std::unique_ptr<DirectoryWatcher>>;
  using SubjectType = ObserverType::SubjectType;
  virtual void onFileChanged(const std::string& filename) = 0;
  virtual void onFileAdded(const std::string& filename) = 0;
  virtual void onFileRemoved(const std::string& filename) = 0;
  virtual void onFileRenamed(const std::string& oldName, const std::string& newName) = 0;
};

class FocusEvents {
public:
  using ObserverType = Observer<std::unique_ptr<FocusEvents>>;
  using SubjectType = ObserverType::SubjectType;
  virtual void onFocusGained() {}
  virtual void onFocusLost() {}
};

class AppPlatform {
public:
  void addFocusObserver(FocusEvents::ObserverType& o);
  void addDirectoryObserver(DirectoryWatcher::ObserverType& o);
  //Notify observers of events
  void onFocusGained();
  void onFocusLost();

  virtual std::string getExePath() = 0;

protected:
  FocusEvents::SubjectType mFocusSubject;
  DirectoryWatcher::SubjectType mDirectorySubject;
};