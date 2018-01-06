#include "Precompile.h"
#include "AppPlatform.h"

void AppPlatform::addFocusObserver(FocusEvents::ObserverType& o) {
  o.observe(&mFocusSubject);
}

void AppPlatform::addDirectoryObserver(DirectoryWatcher::ObserverType& o) {
  o.observe(&mDirectorySubject);
}

#define CallOnObservers(subject, method, ...) for(auto o : subject.get()) o->get()->method(__VA_ARGS__);

void AppPlatform::onFocusGained() {
  CallOnObservers(mFocusSubject, onFocusGained);
}

void AppPlatform::onFocusLost() {
  CallOnObservers(mFocusSubject, onFocusLost);
}

void AppPlatform::onFileChanged(const std::string& filename) {
  CallOnObservers(mDirectorySubject, onFileChanged, filename);
}
