#pragma once

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
    static void runSystems(EntityRegistry<EntityT>& registry, JobInfo<EntityT>& root, WorkContainer& container, const EnqueueToThread& enqueueToThread) {
      if(root.mSystem) {
        root.mSystem->tick(registry);
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
    struct DependencyInfo {
      void addJob(typeId_t<SystemInfo> info, std::shared_ptr<JobInfo<EntityT>> job) {
        mJobs[info].push_back(std::move(job));
      }

      //All of the jobs of this type must finish before dependent can run
      void addDependent(typeId_t<SystemInfo> info, std::shared_ptr<JobInfo<EntityT>> dependent) {
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

      void clear(typeId_t<SystemInfo> info) {
        //Would construct an empty container if there wasn't one, which is fine
        mJobs[info].clear();
      }

      void clearAllTypes() {
        mJobs.clear();
      }

      std::unordered_map<typeId_t<SystemInfo>, std::vector<std::shared_ptr<JobInfo<EntityT>>>> mJobs;
    };

    template<class EntityT>
    struct JobGraphBuilder {
      std::shared_ptr<JobInfo<EntityT>> mRoot;
      DependencyInfo<EntityT> mReaders;
      DependencyInfo<EntityT> mWriters;
      DependencyInfo<EntityT> mExistenceReaders;
      DependencyInfo<EntityT> mComponentFactories;
      DependencyInfo<EntityT> mEntityFactories;
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
      static const auto factoryKey = typeId<void, SystemInfo>();

      for(auto&& system : systems) {
        auto job = std::make_shared<JobInfo<EntityT>>();
        const SystemInfo info = system->getInfo();
        job->mSystem = system;
        job->mThreadRequirement = info.mThreadRequirement;
        job->mName = info.mName;

        //Add dependents for each type
        for(auto&& type : info.mExistenceTypes) {
          builder.mComponentFactories.addDependent(type, job);
          builder.mEntityFactories.addDependent(factoryKey, job);
        }
        for(auto&& type : info.mReadTypes) {
          builder.mComponentFactories.addDependent(type, job);
          builder.mEntityFactories.addDependent(factoryKey, job);
          builder.mWriters.addDependent(type, job);
        }
        for(auto&& type : info.mWriteTypes) {
          builder.mComponentFactories.addDependent(type, job);
          builder.mEntityFactories.addDependent(factoryKey, job);
          builder.mReaders.addDependent(type, job);
          builder.mWriters.addDependent(type, job);

          //Since readers and writers depend on writes the previous ones can be cleared and new ones only need
          //to take a dependency on this
          builder.mReaders.clear(type);
          builder.mWriters.clear(type);
        }
        for(auto&& type : info.mFactoryTypes) {
          builder.mComponentFactories.addDependent(type, job);
          builder.mEntityFactories.addDependent(factoryKey, job);
          builder.mReaders.addDependent(type, job);
          builder.mWriters.addDependent(type, job);
          builder.mExistenceReaders.addDependent(type, job);

          //Clear any jobs that point back at this
          builder.mComponentFactories.clear(type);
          builder.mReaders.clear(type);
          builder.mWriters.clear(type);
          builder.mExistenceReaders.clear(type);
        }
        if(info.mUsesEntityFactory) {
          builder.mComponentFactories.addDependentToAllTypes(job);
          builder.mEntityFactories.addDependent(factoryKey, job);
          builder.mReaders.addDependentToAllTypes(job);
          builder.mWriters.addDependentToAllTypes(job);
          builder.mExistenceReaders.addDependentToAllTypes(job);

          //Clear any jobs that point back at this
          builder.mComponentFactories.clearAllTypes();
          builder.mEntityFactories.clear(factoryKey);
          builder.mReaders.clearAllTypes();
          builder.mWriters.clearAllTypes();
          builder.mExistenceReaders.clearAllTypes();
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
        if(info.mUsesEntityFactory) {
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