#include "Precompile.h"
#include "editor/AssetWatcher.h"

#include "asset/Asset.h"
#include "editor/Editor.h"
#include "event/AssetEvents.h"
#include "event/EditorEvents.h"
#include "event/EventHandler.h"
#include "file/FilePath.h"
#include "ProjectLocator.h"
#include "provider/MessageQueueProvider.h"

AssetWatcher::AssetWatcher(MessageQueueProvider& msg, EventHandler& handler, class AppPlatform& platform, const ProjectLocator& locator)
  : mLocator(locator)
  , mMsg(msg)
  , mPlayState(PlayState::Stopped)
  , mDirectoryWatcher(platform.createDirectoryWatcher(locator.transform("", PathSpace::Project, PathSpace::Full))) {
  mDirectoryWatcher->addObserver(*this);
  platform.addFocusObserver(*this);

  mEventListeners.push_back(handler.registerEventListener([this](const SetPlayStateEvent& e) {
    mPlayState = e.mState;
  }));
}

AssetWatcher::~AssetWatcher() = default;

void AssetWatcher::onFileChanged(const std::string& filename) {
  _enqueueAction(Action::Modify, FilePath(filename.c_str()));
}

void AssetWatcher::onFileAdded(const std::string& filename) {
  _enqueueAction(Action::Add, FilePath(filename.c_str()));
}

void AssetWatcher::onFileRemoved(const std::string& filename) {
  _enqueueAction(Action::Remove, FilePath(filename.c_str()));
}

void AssetWatcher::onFileRenamed(const std::string& oldName, const std::string& newName) {
  _enqueueAction(Action::Remove, oldName.c_str());
  _enqueueAction(Action::Add, FilePath(newName.c_str()));
}

void AssetWatcher::onFocusGained() {
  mIsAppFocused = true;
  if(mPlayState == PlayState::Stopped) {
    _processActions();
  }
}

void AssetWatcher::onFocusLost() {
  mIsAppFocused = false;
}

void AssetWatcher::_enqueueAction(Action action, FilePath path) {
  auto toStr = [](Action a) {
    switch(a) {
      case Action::Add: return "add";
      case Action::Modify: return "modify";
      case Action::Remove: return "remove";
      default: return "";
    }
  };
  printf("Enqueue %s to %s\n", toStr(action), path.cstr());

  auto it = mActions.find(path);
  Action newAction = action;
  if(it != mActions.end()) {
    const Action prevAction = it->second;
    newAction = _transformAction(prevAction, action);
  }
  mActions[path] = newAction;
}

void AssetWatcher::_processActions() {
  for(const auto& pair : mActions) {
    //TODO: figure out the proper way to exclude temporary files
    if(std::string_view(pair.first.cstr(), pair.first.size()) == "scene.json") {
      continue;
    }
    switch(pair.second) {
      case Action::Add:
      case Action::Modify: {
        AssetInfo info(pair.first.cstr());
        info.fill();
        mMsg.getMessageQueue()->push(ReloadAssetRequest(info).then(typeId<Editor, System>(), [](const ReloadAssetResponse& e) {
          if(e.mAsset) {
            if(!e.mWasNewlyCreated) {
              printf("Reloading asset %s\n",  e.mAsset->getInfo().mUri.c_str());
              //TODO: tell users of this asset to reload it somehow
            }
            else {
              printf("Adding asset %s\n", e.mAsset->getInfo().mUri.c_str());
            }
          }
        }));
      }

      case Action::Remove:
        //TODO: remove assets here
        break;
    }
  }
  mActions.clear();
}

AssetWatcher::Action AssetWatcher::_transformAction(Action oldAction, Action newAction) {
  switch(oldAction) {
    //Remove cancels out add, modify is redundant before add
    case Action::Add:
      return newAction == Action::Remove ? Action::Remove : Action::Add;
    //Remove cancels out modify, add doesn't make sense
    case Action::Modify:
      return newAction == Action::Remove ? Action::Remove : Action::Modify;
    //Add cancels out remove, modify doesn't matter if removing
    case Action::Remove:
      return newAction == Action::Add ? Action::Add : Action::Remove;
  }
  assert(false && "Unhandled case");
  return newAction;
}
