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
          todo.push(dependent.get());
        }

        todo.pop();
      }
    }

    //Run this system from the root and enqueue any unblocked systems after the work into the container
    template<class EntityT, class WorkContainer>
    static void runSystems(EntityRegistry<EntityT>& registry, JobInfo<EntityT>& root, WorkContainer& container) {
      if(root.mSystem) {
        root.mSystem->tick(registry);
      }
      for(auto dependent : root.mDependents) {
        if (const uint32_t dependenciesLeft = dependent->mDependencies.fetch_sub(uint32_t(1), std::memory_order_relaxed); dependenciesLeft <= 1) {
          assert(dependenciesLeft == 1 && "This should only hit zero, if it went negative that means there was a bookkeeping error");
          //Work is complete, push them to the container for processing
          container.push_back(dependent);
        }
      }
    }

    //Build a graph of JobInfos based on the system dependencies. The root node contains all
    //jobs that have no dependencies
    template<class EntityT>
    static std::shared_ptr<JobInfo<EntityT>> build(std::vector<std::shared_ptr<ISystem<EntityT>>>& systems) {
      auto root = std::make_shared<JobInfo<EntityT>>();
      DependencyInfo<EntityT> readers;
      DependencyInfo<EntityT> writers;
      DependencyInfo<EntityT> existenceReaders;
      DependencyInfo<EntityT> componentFactories;
      DependencyInfo<EntityT> entityFactories;
      //Key for use in entityFactories so logic for it can look similar to the other dependencyinfos
      static const auto factoryKey = typeId<void, SystemInfo>();

      for(auto&& system : systems) {
        auto job = std::make_shared<JobInfo<EntityT>>();
        job->mSystem = system;
        const SystemInfo info = system->getInfo();

        //Add dependents for each type
        for(auto&& type : info.mExistenceTypes) {
          componentFactories.addDependent(type, job);
          entityFactories.addDependent(factoryKey, job);
        }
        for(auto&& type : info.mReadTypes) {
          componentFactories.addDependent(type, job);
          entityFactories.addDependent(factoryKey, job);
          writers.addDependent(type, job);
        }
        for(auto&& type : info.mWriteTypes) {
          componentFactories.addDependent(type, job);
          entityFactories.addDependent(factoryKey, job);
          readers.addDependent(type, job);
          writers.addDependent(type, job);

          //Since readers and writers depend on writes the previous ones can be cleared and new ones only need
          //to take a dependency on this
          readers.clear(type);
          writers.clear(type);
        }
        for(auto&& type : info.mFactoryTypes) {
          componentFactories.addDependent(type, job);
          entityFactories.addDependent(factoryKey, job);
          readers.addDependent(type, job);
          writers.addDependent(type, job);
          existenceReaders.addDependent(type, job);

          //Clear any jobs that point back at this
          componentFactories.clear(type);
          readers.clear(type);
          writers.clear(type);
          existenceReaders.clear(type);
        }
        if(info.mUsesEntityFactory) {
          componentFactories.addDependentToAllTypes(job);
          entityFactories.addDependent(factoryKey, job);
          readers.addDependentToAllTypes(job);
          writers.addDependentToAllTypes(job);
          existenceReaders.addDependentToAllTypes(job);

          //Clear any jobs that point back at this
          componentFactories.clearAllTypes();
          entityFactories.clear(factoryKey);
          readers.clearAllTypes();
          writers.clearAllTypes();
          existenceReaders.clearAllTypes();
        }

        //Add jobs for each type in a second phase so this doesn't depend on itself
        for(auto&& type : info.mExistenceTypes) {
          existenceReaders.addJob(type, job);
        }
        for(auto&& type : info.mReadTypes) {
          readers.addJob(type, job);
        }
        for(auto&& type : info.mWriteTypes) {
          writers.addJob(type, job);
        }
        for(auto&& type : info.mFactoryTypes) {
          componentFactories.addJob(type, job);
        }
        if(info.mUsesEntityFactory) {
          entityFactories.addJob(factoryKey, job);
        }

        //If there are no other dependencies, add this to the root so it doesn't get lost
        if(!job->mTotalDependencies) {
          root->addDependent(job);
        }
      }

      return root;
    }

  private:
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
  };
}