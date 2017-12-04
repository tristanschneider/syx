#pragma once
namespace Syx {
  typedef std::chrono::nanoseconds Duration;
  typedef std::chrono::high_resolution_clock::time_point Time;

  struct ProfileBlock {
    std::string mName;
    Time mStartTime;
    size_t mHistoryBlock;
  };

  struct ProfileResult {
    ProfileResult() {}
    ProfileResult(const std::string& name, Duration duration, size_t depth) :
      mName(name), mDuration(duration), mDepth(depth) {
    }

    std::string getReportString(const std::string& indent, float time) {
      std::string result;
      result.reserve(indent.size()*mDepth + mName.size() + 10);

      for(size_t i = 0; i < mDepth; ++i)
        result += indent;

      result += mName;
      result += ": ";
      result += std::to_string(time);
      return result;
    }

    std::string getReportString(const std::string& indent) {
      return getReportString(indent, milliseconds());
    }

    float seconds() const {
      double billion = 1000000000.0;
      return static_cast<float>(static_cast<double>(mDuration.count()) / billion);
    }

    float milliseconds() const {
      double million = 1000000.0;
      return static_cast<float>(static_cast<double>(mDuration.count()) / million);
    }

    std::string mName;
    Duration mDuration;
    size_t mDepth;
  };

  class Profiler {
  public:
    Profiler(void) : mCurDepth(0) {}

    void pushBlock(const std::string& name);
    //This will pop the next block on the stack, but throw an error if it's not the block you think it is
    void popBlock(const std::string& name);

    const std::vector<ProfileResult> getBlocks() { return mHistory; }
    //Returns first block with given name
    const ProfileResult* getBlock(const std::string& name);

    const std::string& getReport(const std::string& indent);
    const std::vector<ProfileResult>& getHistory() { return mHistory; }

  private:
    ProfileBlock& _getBlock(size_t index);
    Time _getTime() { return std::chrono::high_resolution_clock::now(); }

    size_t mCurDepth;
    std::vector<ProfileBlock> mBlockStack;
    std::vector<ProfileResult> mHistory;
    std::string mReport;
  };

  struct AutoProfileBlock {
    AutoProfileBlock(Profiler& profiler, const std::string& name)
      : mProfiler(profiler)
      , mName(name) {
      //Need to fix this, it isn't thread safe
      //mProfiler.pushBlock(mName);
    }

    ~AutoProfileBlock(void) {
      //mProfiler.popBlock(mName);
    }

    Profiler& mProfiler;
    std::string mName;
  };
}