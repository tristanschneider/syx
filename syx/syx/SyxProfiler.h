#pragma once
#include <chrono>
#include <string>
#include <vector>
namespace Syx
{
  typedef std::chrono::nanoseconds Duration;
  typedef std::chrono::high_resolution_clock::time_point Time;

  struct ProfileBlock
  {
    std::string m_name;
    Time m_startTime;
    size_t m_historyBlock;
  };

  struct ProfileResult
  {
    ProfileResult(void) {}
    ProfileResult(const std::string& name, Duration duration, size_t depth):
      m_name(name), m_duration(duration), m_depth(depth) {}

    std::string GetReportString(const std::string& indent, float time)
    {
      std::string result;
      result.reserve(indent.size()*m_depth + m_name.size() + 10);

      for(size_t i = 0; i < m_depth; ++i)
        result += indent;

      result += m_name;
      result += ": ";
      result += std::to_string(time);
      return result;
    }

    std::string GetReportString(const std::string& indent)
    {
      return GetReportString(indent, Milliseconds());
    }

    float Seconds(void) const
    {
      double billion = 1000000000.0;
      return static_cast<float>(static_cast<double>(m_duration.count())/billion);
    }

    float Milliseconds(void) const
    {
      double million = 1000000.0;
      return static_cast<float>(static_cast<double>(m_duration.count())/million);
    }

    std::string m_name;
    Duration m_duration;
    size_t m_depth;
  };

  class Profiler
  {
  public:
    Profiler(void): m_curDepth(0) {}

    void PushBlock(const std::string& name);
    //This will pop the next block on the stack, but throw an error if it's not the block you think it is
    void PopBlock(const std::string& name);

    const std::vector<ProfileResult> GetBlocks(void) { return m_history; }
    //Returns first block with given name
    const ProfileResult* GetBlock(const std::string& name);

    const std::string& GetReport(const std::string& indent);
    const std::vector<ProfileResult>& GetHistory(void) { return m_history; }

  private:
    ProfileBlock& GetBlock(size_t index);
    Time GetTime(void) { return std::chrono::high_resolution_clock::now(); }

    size_t m_curDepth;
    std::vector<ProfileBlock> m_blockStack;
    std::vector<ProfileResult> m_history;
    std::string m_report;
  };

  struct AutoProfileBlock
  {
    AutoProfileBlock(Profiler& profiler, const std::string& name): m_profiler(profiler), m_name(name)
    {
      m_profiler.PushBlock(m_name);
    }

    ~AutoProfileBlock(void)
    {
      m_profiler.PopBlock(m_name);
    }

    Profiler& m_profiler;
    std::string m_name;
  };
}