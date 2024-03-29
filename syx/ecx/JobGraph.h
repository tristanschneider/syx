#pragma once

#include "CommandBufferSystem.h"
#include "EntityRegistry.h"
#include "JobInfo.h"
#include "System.h"

namespace ecx {
  struct JobGraph {
    //Reset dependencies of the graph starting at the root. Expected to be while no other threads are operating on the graph
    template<class EntityT>
    static void resetDependencies(JobInfo<EntityT>& root) {
      std::queue<JobInfo<EntityT>*> todo;
      todo.push(&root);
      while(!todo.empty()) {
        JobInfo<EntityT>* current = todo.front();
        current->mDependencies = current->mTotalDependencies;

        for(const auto& dependent : current->mDependents) {
          if(dependent->mDependencies != dependent->mTotalDependencies) {
            todo.push(dependent.get());
          }
        }

        todo.pop();
      }
    }

    //Run this system from the root and enqueue any unblocked systems after the work into the container
    //WorkContainer needs push_back
    //EnqueueToThread is somethind like a function that takes thread index and the job to queue (size_t, std::shared_ptr<JobInfo<EntityT>>)
    template<class EntityT, class WorkContainer, class EnqueueToThread>
    static void runSystems(EntityRegistry<EntityT>& registry, ThreadLocalContext& localContext, JobInfo<EntityT>& root, WorkContainer& container, const EnqueueToThread& enqueueToThread) {
      if(root.mSystem) {
        root.mSystem->tick(registry, localContext);
      }
      for(auto dependent : root.mDependents) {
        if (const uint32_t dependenciesLeft = dependent->mDependencies.fetch_sub(uint32_t(1), std::memory_order_relaxed); dependenciesLeft <= 1) {
          assert(dependenciesLeft == 1 && "This should only hit zero, if it went negative that means there was a bookkeeping error");
          //Work is complete, push them to the container for processing
          if(dependent->mThreadRequirement) {
            enqueueToThread(*dependent->mThreadRequirement, dependent);
          }
          else {
            container.push_back(dependent);
          }
        }
      }
    }

    template<class EntityT>
    static std::string print(const JobInfo<EntityT>& info) {
      std::string result;
      std::unordered_set<const JobInfo<EntityT>*> done;
      std::queue<const JobInfo<EntityT>*> todo;
      todo.push(&info);
      auto ptrname = [](auto ptr) {
        return std::to_string(std::hash<const void*>()(static_cast<const void*>(ptr)));
      };
      while(!todo.empty()) {
        const JobInfo<EntityT>* current = todo.front();
        todo.pop();
        if(!done.insert(current).second) {
          continue;
        }
        result += std::to_string(current->mTotalDependencies) + " " + current->mName + ptrname(current) + " -> ";
        for(const std::shared_ptr<JobInfo<EntityT>>& child : current->mDependents) {
          result += child->mName + ptrname(child.get()) + ", ";
          todo.push(&*child);
        }
        result += "\n";
      }
      return result;
    }

    template<class EntityT>
    struct DependencyInfo {
      void addJob(typeId_t<EntityT> info, std::shared_ptr<JobInfo<EntityT>> job) {
        mJobs[info].push_back(std::move(job));
      }

      //All of the jobs of this type must finish before dependent can run
      void addDependent(typeId_t<EntityT> info, std::shared_ptr<JobInfo<EntityT>> dependent) {
        for(auto& job : mJobs[info]) {
          job->addDependentNoDuplicate(dependent);
        }
      }

      void addDependentToAllTypes(std::shared_ptr<JobInfo<EntityT>> dependent) {
        for(auto& pair : mJobs) {
          for(auto& job : pair.second) {
            job->addDependentNoDuplicate(dependent);
          }
        }
      }

      void clear(typeId_t<EntityT> info) {
        //Would construct an empty container if there wasn't one, which is fine
        mJobs[info].clear();
      }

      void clearAllTypes() {
        mJobs.clear();
      }

      std::unordered_map<typeId_t<EntityT>, std::vector<std::shared_ptr<JobInfo<EntityT>>>> mJobs;
    };

    template<class EntityT>
    struct JobGraphBuilder {
      std::shared_ptr<JobInfo<EntityT>> mRoot;
      DependencyInfo<EntityT> mReaders;
      DependencyInfo<EntityT> mWriters;
      DependencyInfo<EntityT> mExistenceReaders;
      DependencyInfo<EntityT> mComponentFactories;
      DependencyInfo<EntityT> mEntityFactories;
      DependencyInfo<EntityT> mCommandPublishers;
    };

    //Build a graph of JobInfos based on the system dependencies. The root node contains all
    //jobs that have no dependencies
    template<class EntityT>
    static std::shared_ptr<JobInfo<EntityT>> build(std::vector<std::shared_ptr<ISystem<EntityT>>>& systems) {
      JobGraphBuilder<EntityT> builder;
      builder.mRoot = std::make_shared<JobInfo<EntityT>>();
      _build(builder, systems);
      return builder.mRoot;
    }

    template<class EntityT>
    static void _build(JobGraphBuilder<EntityT>& builder, std::vector<std::shared_ptr<ISystem<EntityT>>>& systems) {
      //Key for use in entityFactories so logic for it can look similar to the other dependencyinfos
      static const auto factoryKey = typeId<void, EntityT>();

      std::unordered_set<typeId_t<EntityT>> commandProcessTypes;
      auto tryProcessCommandsForType = [&commandProcessTypes, &builder](typeId_t<EntityT> type) {
        if(const auto& it = builder.mCommandPublishers.mJobs.find(type); it != builder.mCommandPublishers.mJobs.end() && !it->second.empty()) {
          commandProcessTypes.insert(type);
        }
      };

      //Always process at the end of the frame
      systems.push_back(std::make_shared<ProcessEntireCommandBufferSystem<EntityT>>());

      //Hack to skip over injecting a system for which a command processor has already been injected
      size_t skipInject = std::numeric_limits<size_t>::max();
      for(size_t i = 0; i < systems.size();) {
        auto system = systems[i];
        auto job = std::make_shared<JobInfo<EntityT>>();
        const SystemInfo<EntityT> info = system->getInfo();
        job->mSystem = system;
        job->mThreadRequirement = info.mThreadRequirement;
        job->mName = info.mName;

        commandProcessTypes.clear();

        bool processAllCommands = false;
        if(skipInject != i) {
          for(auto&& type : info.mExistenceTypes) {
            tryProcessCommandsForType(type);
          }
          for(auto&& type : info.mReadTypes) {
            tryProcessCommandsForType(type);
          }
          for(auto&& type : info.mWriteTypes) {
            tryProcessCommandsForType(type);
          }
          for(auto&& type : info.mFactoryTypes) {
            tryProcessCommandsForType(type);
          }
          for(auto&& type : info.mCommandBufferTypes) {
            tryProcessCommandsForType(type);
          }
          if(info.mIsBlocking) {
            if(!info.mIsCommandProcessor && !commandProcessTypes.empty()) {
              processAllCommands = true;
            }
          }
          //Defer destroy injects after this system
          if(info.mDeferDestroysEntities) {
            systems.insert(systems.begin() + i + 1, std::make_shared<ProcessEntireCommandBufferSystem<EntityT>>());
          }
        }
        //TODO: ideally commands to process could be on a per-component type basis. That would currently require a compile time component type which isn't available here
        if(processAllCommands || !commandProcessTypes.empty()) {
          systems.insert(systems.begin() + i, std::make_shared<ProcessEntireCommandBufferSystem<EntityT>>());
          //Inject the system and start over the dependency gathering before adding this one to the job graph
          //Then skip injection checking for this system the next time around
          skipInject = i + 1;
          continue;
        }
        else {
          ++i;
        }

        //Add dependents for each type
        for(auto&& type : info.mExistenceTypes) {
          builder.mComponentFactories.addDependent(type, job);
          builder.mEntityFactories.addDependent(factoryKey, job);
          builder.mCommandPublishers.addDependent(type, job);
        }
        for(auto&& type : info.mReadTypes) {
          builder.mComponentFactories.addDependent(type, job);
          builder.mEntityFactories.addDependent(factoryKey, job);
          builder.mWriters.addDependent(type, job);
          builder.mCommandPublishers.addDependent(type, job);
        }
        for(auto&& type : info.mWriteTypes) {
          builder.mComponentFactories.addDependent(type, job);
          builder.mEntityFactories.addDependent(factoryKey, job);
          builder.mReaders.addDependent(type, job);
          builder.mWriters.addDependent(type, job);
          builder.mCommandPublishers.addDependent(type, job);

          //Since readers and writers depend on writes the previous ones can be cleared and new ones only need
          //to take a dependency on this
          builder.mReaders.clear(type);
          builder.mWriters.clear(type);
        }
        //Command buffer publishers change existence and can create entities that readers and writers would see so shouldn't be ordered past them
        for(auto&& type : info.mCommandBufferTypes) {
          builder.mComponentFactories.addDependent(type, job);
          builder.mEntityFactories.addDependent(factoryKey, job);
          builder.mReaders.addDependent(type, job);
          builder.mWriters.addDependent(type, job);
          builder.mCommandPublishers.addDependent(type, job);
          builder.mExistenceReaders.addDependent(type, job);

          builder.mReaders.clear(type);
          builder.mWriters.clear(type);
          builder.mCommandPublishers.clear(type);
        }
        //TODO: I think this is wrong and should be blocking since it moves entities between chunks.
        //should anyway be replaced by command buffer usage
        for(auto&& type : info.mFactoryTypes) {
          builder.mComponentFactories.addDependent(type, job);
          builder.mEntityFactories.addDependent(factoryKey, job);
          builder.mReaders.addDependent(type, job);
          builder.mWriters.addDependent(type, job);
          builder.mExistenceReaders.addDependent(type, job);
          builder.mCommandPublishers.addDependent(type, job);

          //Clear any jobs that point back at this
          builder.mComponentFactories.clear(type);
          builder.mReaders.clear(type);
          builder.mWriters.clear(type);
          builder.mExistenceReaders.clear(type);
          builder.mCommandPublishers.clear(type);
        }
        //Removing an entity could affect any component depending on what happens to be on the entity, so immediately process all commands after the system runs
        if(info.mDeferDestroysEntities) {
          builder.mComponentFactories.addDependentToAllTypes(job);
          builder.mEntityFactories.addDependent(factoryKey, job);
          builder.mReaders.addDependentToAllTypes(job);
          builder.mWriters.addDependentToAllTypes(job);
          builder.mExistenceReaders.addDependentToAllTypes(job);
          builder.mCommandPublishers.addDependentToAllTypes(job);
        }
        if(info.mIsBlocking) {
          builder.mComponentFactories.addDependentToAllTypes(job);
          builder.mEntityFactories.addDependent(factoryKey, job);
          builder.mReaders.addDependentToAllTypes(job);
          builder.mWriters.addDependentToAllTypes(job);
          builder.mExistenceReaders.addDependentToAllTypes(job);
          builder.mCommandPublishers.addDependentToAllTypes(job);

          //Clear any jobs that point back at this
          builder.mComponentFactories.clearAllTypes();
          builder.mEntityFactories.clear(factoryKey);
          builder.mReaders.clearAllTypes();
          builder.mWriters.clearAllTypes();
          builder.mExistenceReaders.clearAllTypes();
          builder.mCommandPublishers.clearAllTypes();
        }

        //Add jobs for each type in a second phase so this doesn't depend on itself
        for(auto&& type : info.mExistenceTypes) {
          builder.mExistenceReaders.addJob(type, job);
        }
        for(auto&& type : info.mReadTypes) {
          builder.mReaders.addJob(type, job);
        }
        for(auto&& type : info.mWriteTypes) {
          builder.mWriters.addJob(type, job);
        }
        for(auto&& type : info.mFactoryTypes) {
          builder.mComponentFactories.addJob(type, job);
        }
        for(auto&& type : info.mCommandBufferTypes) {
          builder.mCommandPublishers.addJob(type, job);
        }
        if(info.mIsBlocking) {
          builder.mEntityFactories.addJob(factoryKey, job);
        }

        //If there are no other dependencies, add this to the root so it doesn't get lost
        if(!job->mTotalDependencies) {
          builder.mRoot->addDependent(job);
        }
      }
    }
  };
}