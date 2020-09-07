#include "Precompile.h"
#include "AppPlatform.h"

#include "file/FileSystem.h"

void AppPlatform::addFocusObserver(FocusEvents& o) {
  o.observe(&mFocusSubject);
}

void AppPlatform::addDragDropObserver(DragDropObserver& o) {
  o.observe(&mDragDropSubject);
}

void AppPlatform::onFocusGained() {
  mFocusSubject.forEach([](FocusEvents& e) { e.onFocusGained(); });
}

void AppPlatform::onFocusLost() {
  mFocusSubject.forEach([](FocusEvents& e) { e.onFocusLost(); });
}

void AppPlatform::onDrop(const std::vector<FilePath>& files) {
  mDragDropSubject.forEach([&files](DragDropObserver& e) { e.onDrop(files); });
}

std::unique_ptr<FileSystem::IFileSystem> AppPlatform::createFileSystem() {
  return FileSystem::createStd();
}
