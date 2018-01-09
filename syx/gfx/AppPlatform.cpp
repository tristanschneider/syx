#include "Precompile.h"
#include "AppPlatform.h"

void AppPlatform::addFocusObserver(FocusEvents::ObserverType& o) {
  o.observe(&mFocusSubject);
}

void AppPlatform::addDirectoryObserver(DirectoryWatcher::ObserverType& o) {
  o.observe(&mDirectorySubject);
}

void AppPlatform::onFocusGained() {
  CallOnObserversPtr(mFocusSubject, onFocusGained);
}

void AppPlatform::onFocusLost() {
  CallOnObserversPtr(mFocusSubject, onFocusLost);
}