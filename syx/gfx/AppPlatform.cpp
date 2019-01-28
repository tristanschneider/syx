#include "Precompile.h"
#include "AppPlatform.h"

void AppPlatform::addFocusObserver(FocusEvents& o) {
  o.observe(&mFocusSubject);
}

void AppPlatform::addDirectoryObserver(DirectoryWatcher& o) {
  o.observe(&mDirectorySubject);
}

void AppPlatform::onFocusGained() {
  mFocusSubject.forEach([](FocusEvents& e) { e.onFocusGained(); });
}

void AppPlatform::onFocusLost() {
  mFocusSubject.forEach([](FocusEvents& e) { e.onFocusLost(); });
}