#include "Precompile.h"

namespace Syx {
  ProfileBlock& Profiler::_getBlock(size_t index) {
    while(mBlockStack.size() <= index)
      mBlockStack.emplace_back();
    return mBlockStack[index];
  }

  void Profiler::pushBlock(const std::string& name) {
    //If we're back at zero we must be starting over, so clear the history from last frame
    if(!mCurDepth)
      mHistory.clear();
    ProfileBlock& block = _getBlock(mCurDepth++);
    block.mName = name;
    block.mStartTime = _getTime();
    block.mHistoryBlock = mHistory.size();
    mHistory.push_back(ProfileResult());
  }

  void Profiler::popBlock(const std::string& name) {
    SyxAssertError(mCurDepth > 0);
    ProfileBlock& block = _getBlock(--mCurDepth);
    SyxAssertError(block.mName == name);

    auto duration = std::chrono::duration_cast<Duration>(_getTime() - block.mStartTime);
    mHistory[block.mHistoryBlock] = ProfileResult(name, duration, mCurDepth);
  }

  const ProfileResult* Profiler::getBlock(const std::string& name) {
    for(ProfileResult& block : mHistory)
      if(block.mName == name)
        return &block;
    return nullptr;
  }

  const std::string& Profiler::getReport(const std::string& indent) {
    mReport.clear();
    for(ProfileResult& block : mHistory)
      mReport += block.getReportString(indent) + "\n";
    return mReport;
  }
}//Syx