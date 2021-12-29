#pragma once

#include "JobGraph.h"

namespace ecx {
  //Returns ticks at the specified `targetFPS` using `GetTime`, never returning more than `UpdateCap` ticks
  //Extra ticks are built up and redeemed later
  //TargetFPS of 0 will result in always ticking once
  template<size_t UpdateCap, auto GetTime>
  class Timer {
  public:
    using TimePoint = decltype(GetTime());

    Timer(size_t targetFPS)
      : mTargetFPS(targetFPS)
      , mLastTime(GetTime()) {
    }

    //Advance the timer and redeem all available ticks based on the target FPS
    size_t redeemTicks() {
      if(!mTargetFPS) {
        return size_t(1);
      }
      auto now = GetTime();
      auto elapsed = now - mLastTime;
      mLastTime = now;
      mRemainder += std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
      const std::chrono::milliseconds tickDuration(std::chrono::milliseconds(1000) / std::chrono::milliseconds(mTargetFPS));

      size_t ticks = 0;
      while(mRemainder >= tickDuration && ticks < UpdateCap) {
        ++ticks;
        mRemainder -= tickDuration;
      }

      return ticks;
    }

  private:
    size_t mTargetFPS = 0;
    TimePoint mLastTime;
    std::chrono::milliseconds mRemainder = std::chrono::milliseconds(0);
  };

  //Manages the ticking of the registered update stages, where each stage can have a different tick rate,
  //for instance, input at a faster rate than gameplay, which also differs from physics and graphics.
  //Each phase has a separate job graph but the graphs could share systems if desired
  //SchedulerT requires execute(EntityRegistry<EntityT>& registry) example Scheduler.h
  //TimerT implementation example above
  //StageIdT is any id with operator<
  template<class SchedulerT, class TimerT, class StageIdT, class EntityT>
  class AppContext {
  public:
    AppContext(std::shared_ptr<SchedulerT> scheduler)
      : mScheduler(scheduler) {
    }

    //Register the phase in order as determined by stage. If a phase already exists with this id it is replaced
    void registerUpdatePhase(StageIdT stage, std::vector<std::shared_ptr<ISystem<EntityT>>> systems, size_t targetFPS) {
      auto it = std::lower_bound(mUpdatePhases.begin(), mUpdatePhases.end(), stage, [](const UpdatePhase& phase, const StageIdT& stage) {
        return phase.mPhaseId < stage;
      });
      UpdatePhase newPhase{ std::move(stage), std::move(systems), TimerT(targetFPS), size_t(0) };
      //If this has already been registered, replace it
      if(it != mUpdatePhases.end() && it->mPhaseId == stage) {
        *it = std::move(newPhase);
      }
      //Otherwise, it hasn't already been added, insert it in sorted order here
      else {
        mUpdatePhases.insert(it, std::move(newPhase));
      }
    }

    //Execute all available updates as dictated by each phase's timer
    //If multiple phases have multiple ticks they all tick in order, as in A B C A B C, not AA BB CC
    //That is assuming they have dependencies, if they don't the phases may run in parallel as determined
    //by their job graphs
    bool update(EntityRegistry<EntityT>& registry) {
      assert(mJobConfigurations.size() == (2 << (mUpdatePhases.size() - 1)) && "buildExecutionGraph must be calleed before the first call to update since changing system registration");
      bool updatedAnything = false;
      bool updateTickCredits = true;
      while(true) {
        uint64_t jobConfiguration = 0;
        //Figure out which job configuration to run and update tick credits
        for(size_t i = 0; i < mUpdatePhases.size(); ++i) {
          UpdatePhase& phase = mUpdatePhases[i];
          if(updateTickCredits) {
            phase.mTickCredits += phase.mTimer.redeemTicks();
          }
          if(phase.mTickCredits) {
            jobConfiguration |= (uint64_t(1) << i);
            --phase.mTickCredits;
            updatedAnything = true;
          }
        }
        updateTickCredits = false;

        //Execute the graph containing all systems that should run
        if(std::shared_ptr<JobInfo<EntityT>>& jobGraph = mJobConfigurations[jobConfiguration]) {
          mScheduler->execute(registry, *jobGraph);
        }
        //Job configuration 0 is empty, and means no ticks are left
        else {
          break;
        }
      }

      return updatedAnything;
    }

    void buildExecutionGraph() {
      mJobConfigurations.clear();
      if(mUpdatePhases.empty()) {
        return;
      }
      const size_t totalConfigurations = size_t(2) << (mUpdatePhases.size() - 1);
      mJobConfigurations.resize(totalConfigurations);
      std::vector<std::shared_ptr<ISystem<EntityT>>> systems;
      for(size_t i = 0; i < totalConfigurations; ++i) {
        systems.clear();
        for(size_t s = 0; s < mUpdatePhases.size(); ++s) {
          if(i & (size_t(1) << s)) {
            systems.insert(systems.end(), mUpdatePhases[s].mSystems.begin(), mUpdatePhases[s].mSystems.end());
          }
        }

        mJobConfigurations[i] = !systems.empty() ? JobGraph::build(systems) : nullptr;
      }
    }

    //TODO: start/stop for app suspend?

  private:
    struct UpdatePhase {
      StageIdT mPhaseId;
      std::vector<std::shared_ptr<ISystem<EntityT>>> mSystems;
      TimerT mTimer;
      size_t mTickCredits = 0;
    };

    std::shared_ptr<SchedulerT> mScheduler;
    std::vector<UpdatePhase> mUpdatePhases;
    //All possible execution graphs for when a subset of systems needs to update
    //Ordered by using the bitfield as an index. There are 2^n of these, where n is the number of phases
    //This is under the assumption that there aren't more than a few phases
    std::vector<std::shared_ptr<JobInfo<EntityT>>> mJobConfigurations;
  };
}