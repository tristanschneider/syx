#include "Precompile.h"
#include "SyxProfiler.h"

namespace Syx
{
    ProfileBlock& Profiler::GetBlock(size_t index)
    {
      while(m_blockStack.size() <= index)
        m_blockStack.emplace_back();
      return m_blockStack[index];
    }

    void Profiler::PushBlock(const std::string& name)
    {
      //If we're back at zero we must be starting over, so clear the history from last frame
      if(!m_curDepth)
        m_history.clear();
      ProfileBlock& block = GetBlock(m_curDepth++);
      block.m_name = name;
      block.m_startTime = GetTime();
      block.m_historyBlock = m_history.size();
      m_history.push_back(ProfileResult());
    }

    void Profiler::PopBlock(const std::string& name)
    {
      SyxAssertError(m_curDepth > 0);
      ProfileBlock& block = GetBlock(--m_curDepth);
      SyxAssertError(block.m_name == name);

      auto duration = std::chrono::duration_cast<Duration>(GetTime() - block.m_startTime);
      m_history[block.m_historyBlock] = ProfileResult(name, duration, m_curDepth);
    }

    const ProfileResult* Profiler::GetBlock(const std::string& name)
    {
      for(ProfileResult& block : m_history)
        if(block.m_name == name)
          return &block;
      return nullptr;
    }

    const std::string& Profiler::GetReport(const std::string& indent)
    {
      m_report.clear();
      for(ProfileResult& block : m_history)
        m_report += block.GetReportString(indent) + "\n";
      return m_report;
    }
}//Syx