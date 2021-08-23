#pragma once

#include "AppPlatform.h"
#include "file/DirectoryWatcher.h"
#include "file/FilePath.h"

class AssetRepo;
class DirectoryWatcher;
class EventHandler;
struct EventListener;
class MessageQueueProvider;
enum class PlayState : uint8_t;
class ProjectLocator;

class AssetWatcher : public DirectoryObserver, public FocusEvents {
public:
  AssetWatcher(MessageQueueProvider& msg, EventHandler& handler, class AppPlatform& platform, AssetRepo& repo, const ProjectLocator& locator);
  ~AssetWatcher();

  void onFileChanged(const std::string& filename) override;
  void onFileAdded(const std::string& filename) override;
  void onFileRemoved(const std::string& filename) override;
  void onFileRenamed(const std::string& oldName, const std::string& newName) override;

  void onFocusGained() override;
  void onFocusLost() override;

private:
  enum class Action {
    Add,
    Remove,
    Modify,
  };

  void _enqueueAction(Action action, FilePath path);
  void _processActions();
  //Evaluate resulting action with the highest precedence given the old and new actions
  Action _transformAction(Action oldAction, Action newAction);

  const ProjectLocator& mLocator;
  AssetRepo& mAssetRepo;
  bool mIsAppFocused;
  std::unordered_map<FilePath, Action> mActions;
  std::unique_ptr<DirectoryWatcher> mDirectoryWatcher;
  PlayState mPlayState;
  std::vector<std::shared_ptr<EventListener>> mEventListeners;
};