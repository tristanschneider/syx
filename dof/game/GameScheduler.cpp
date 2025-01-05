#include "Precompile.h"
#include "GameScheduler.h"

#include "AppBuilder.h"
#include "Scheduler.h"
#include "ThreadLocals.h"
#include "Profile.h"
#include "GameTaskArgs.h"

namespace GameScheduler {
  constexpr size_t MAIN_THREAD = 0;

  struct ConversionTask {
    AppTaskNode* src{};
    TaskNode* dst{};
  };

  struct ProfileData {
    std::string_view name;
    ProfileToken profileToken{};
  };

  std::unique_ptr<AppTaskArgs> createAppTaskArgs(ThreadLocals* tls, size_t threadIndex) {
    if(tls) {
      return std::make_unique<GameTaskArgs>(enki::TaskSetPartition{}, *tls, threadIndex);
    }
    //Will probably need more argument later if single threaded mode is used but this should be good enough for tests
    std::unique_ptr<GameTaskArgs> st = std::make_unique<GameTaskArgs>();
    st->threadIndex = threadIndex;
    return st;
  }

  ProfileData createProfileData(std::string_view name) {
    //Hack to cache tokens forever because microprofile has on way to clear them
    static std::unordered_map<size_t, ProfileToken> tokenCache;
    const size_t hash = std::hash<std::string_view>()(name);
    auto it = tokenCache.emplace(hash, ProfileToken{});
    ProfileData result;
    result.name = name;
    assert(name.data());
    if(!it.second) {
      result.profileToken = it.first->second;
    }
    else {
      result.profileToken = PROFILE_CREATETOKEN("scheduler", name.data(), (uint32_t)std::rand());
      it.first->second = result.profileToken;
    }
    return result;
  }

  void setConfigurableTask(enki::ITaskSet& task, AppTask& info) {
    if(info.config) {
      //Raw capture means it's the responsibility of the app to ensure it won't destroy tasks while they're running,
      //which should be reasonable
      info.config->setSize = [&task](const AppTaskSize& desiredSize) {
        task.m_MinRange = static_cast<uint32_t>(desiredSize.batchSize);
        task.m_SetSize = static_cast<uint32_t>(desiredSize.workItemCount);
      };
    }
  }

  void executeTask(AppTaskArgs& args, AppTask& task, ProfileData& profile) {
    PROFILE_ENTER_TOKEN(profile.profileToken);
    task.callback(args);
    PROFILE_EXIT_TOKEN(profile.profileToken);
  }

  struct TaskAdapter : enki::ITaskSet {
    TaskAdapter(AppTaskNode& t, ThreadLocals& tl)
      : task{ std::move(t.task) }
      , profile{ createProfileData(t.name) }
      , tls{ tl }{
      setConfigurableTask(*this, task);
    }

    void ExecuteRange(enki::TaskSetPartition range, uint32_t thread) override {
      if(!task.callback) {
        return;
      }
      GameTaskArgs args{ range, tls, thread };
      executeTask(args, task, profile);
    }

    AppTask task;
    ProfileData profile;
    ThreadLocals& tls;
  };

  struct PinnedTaskAdapter : enki::IPinnedTask {
    PinnedTaskAdapter(AppTaskNode& t, ThreadLocals& tl)
      : enki::IPinnedTask(MAIN_THREAD)
      , task{ std::move(t.task) }
      , profile{ createProfileData(t.name) }
      , tls{ tl } {
    }

    void Execute() override {
      if(task.callback) {
        GameTaskArgs args{ enki::TaskSetPartition{}, tls, MAIN_THREAD };
        executeTask(args, task, profile);
      }
    }

    AppTask task;
    ProfileData profile;
    ThreadLocals& tls;
  };

  struct PopulateTask {
    void operator()(AppTaskPinning::None) {
      task.dst->name = task.src->name;
      task.dst->mTask.mTask = std::make_unique<TaskAdapter>(*task.src, tls);
    }

    void operator()(AppTaskPinning::MainThread) {
      task.dst->name = task.src->name;
      task.dst->mTask.mTask = std::make_unique<PinnedTaskAdapter>(*task.src, tls);
    }

    void operator()(AppTaskPinning::Synchronous) {
      //Synchronous behavior is addressed by GameBuilder.cpp
      task.dst->name = task.src->name;
      task.dst->mTask.mTask = std::make_unique<TaskAdapter>(*task.src, tls);
    }

    ConversionTask& task;
    ThreadLocals& tls;
  };

  TaskRange buildTasks(std::shared_ptr<AppTaskNode> root, ThreadLocals& tls) {
    std::deque<ConversionTask> todo;
    std::unordered_map<AppTaskNode*, std::shared_ptr<TaskNode>> visited;
    auto result = std::make_shared<TaskNode>();
    todo.push_back({ root.get(), result.get() });
    while(!todo.empty()) {
      ConversionTask current = todo.front();
      todo.pop_front();

      //Fill in the task callback for this one
      std::visit(PopulateTask{ current, tls }, current.src->task.pinning);

      //Create empty children and add them to the todo list
      current.dst->mChildren.resize(current.src->children.size());
      for(size_t i = 0; i < current.src->children.size(); ++i) {
        AppTaskNode* child = current.src->children[i].get();
        if(auto found = visited.find(child); found != visited.end()) {
          current.dst->mChildren[i] = found->second;
        }
        else {
          current.dst->mChildren[i] = std::make_shared<TaskNode>();
          visited[child] = current.dst->mChildren[i];
          todo.push_back({ current.src->children[i].get(), current.dst->mChildren[i].get() });
        }
      }
    }

    return TaskBuilder::buildDependencies(result);
  }

  std::vector<SyncWorkItem> buildSync(std::shared_ptr<AppTaskNode> root) {
    std::deque<std::shared_ptr<AppTaskNode>> todo;
    std::vector<SyncWorkItem> result;
    todo.push_back(root);
    while(!todo.empty()) {
      auto current = todo.front();
      todo.pop_front();
      std::shared_ptr<AppTaskSize> size;
      if(current->task.config) {
        size = std::make_shared<AppTaskSize>();
        current->task.config->setSize = [size](AppTaskSize s) { *size = s; };
      }
      result.push_back({ [current, size] {
        //TODO: this probably will eventually need at least the local database
        GameTaskArgs args;
        if(current->task.callback) {
          if(size) {
            size_t complete = 0;
            while(complete < size->workItemCount) {
              args.begin = complete;
              complete += size->batchSize;
              args.end = std::min(complete, size->workItemCount);
              current->task.callback(args);
            }
          }
          else {
            current->task.callback(args);
          }
        }
      }});
      todo.insert(todo.end(), current->children.begin(), current->children.end());
    }
    return result;
  }
};