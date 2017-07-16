#pragma once

#ifdef SENABLED
namespace Syx
{
  typedef std::pair<long long, long long> TimePair;

  class SpeedTester
  {
  public:
    SpeedTester(void);
    ~SpeedTester(void);
    //Copying doesn't really make sense, so do this to surpress can't genereate default whatever errors
    SpeedTester(const SpeedTester&) = delete;
    SpeedTester& operator=(const SpeedTester&) = delete;

    //Test against same thing to make sure cache isn't interfering with timings. 
    TimePair SDotVsSDot(void);

    TimePair RotVecSMatVsSQuat(void);
    TimePair RotVecSQuatVsNQuat(void);
    TimePair RotVecSMatVsNMat(void);
    TimePair RotVecTransSMatVsNMat(void);
    TimePair SDotVsNDot(void);
    TimePair MatMatMultSVsN(void);
    TimePair MatMatMultTransSVsN(void);
    TimePair QuatQuatMultSVsN(void);

    std::vector<std::pair<std::string, TimePair>> TestAll(void);

  private:
    //Clear old samples and generate new ones
    void GenerateSamples(void);

    const size_t m_noSamples;
    const size_t m_noIterations;
    //Pointers so they can be reallocated and thrown out of the cache
    SFloats* m_sPoints;
    Vec3* m_nPoints;
    SMat3* m_sMats;
    Mat3* m_nMats;
    SFloats* m_sQuats;
    Quat* m_nQuats;
  };

}
#endif