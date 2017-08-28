#pragma once

#ifdef SENABLED
namespace Syx {
  typedef std::pair<long long, long long> TimePair;

  class SpeedTester {
  public:
    SpeedTester(void);
    ~SpeedTester(void);
    //Copying doesn't really make sense, so do this to surpress can't genereate default whatever errors
    SpeedTester(const SpeedTester&) = delete;
    SpeedTester& operator=(const SpeedTester&) = delete;

    //Test against same thing to make sure cache isn't interfering with timings. 
    TimePair sDotVsSdot();

    TimePair rotVecSMatVsSQuat();
    TimePair rotVecSQuatVsNQuat();
    TimePair rotVecSMatVsNMat();
    TimePair rotVecTransSMatVsNMat();
    TimePair sDotVsNdot();
    TimePair matMatMultSVsN();
    TimePair matMatMultTransSVsN();
    TimePair quatQuatMultSVsN();

    std::vector<std::pair<std::string, TimePair>> testAll();

  private:
    //Clear old samples and generate new ones
    void generateSamples();

    const size_t mNoSamples;
    const size_t mNoIterations;
    //Pointers so they can be reallocated and thrown out of the cache
    SFloats* mSPoints;
    Vec3* mNPoints;
    SMat3* mSMats;
    Mat3* mNMats;
    SFloats* mSQuats;
    Quat* mNQuats;
  };

}
#endif