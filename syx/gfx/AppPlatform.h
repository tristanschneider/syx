#pragma once
//Input and graphics are still platform specific, but anything easily compartmentalized goes here

class FilePath;

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

class DragDropObserver : public Observer<DragDropObserver> {
public:
  virtual void onDrop(const std::vector<FilePath>& files) = 0;
};

class AppPlatform {
public:
  void addFocusObserver(FocusEvents& o);
  void addDirectoryObserver(DirectoryWatcher& o);
  void addDragDropObserver(DragDropObserver& o);
  //Notify observers of events
  void onFocusGained();
  void onFocusLost();
  void onDrop(const std::vector<FilePath>& files);

  virtual std::string getExePath() = 0;
  virtual void setWorkingDirectory(const char* working) = 0;

protected:
  FocusEvents::SubjectType mFocusSubject;
  DirectoryWatcher::SubjectType mDirectorySubject;
  DragDropObserver::SubjectType mDragDropSubject;
};