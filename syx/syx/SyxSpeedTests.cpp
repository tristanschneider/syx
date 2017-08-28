#include "Precompile.h"
#include "SyxSpeedTests.h"

#ifdef SENABLED
namespace Syx {
  SpeedTester::SpeedTester(void)
    : mNoIterations(1000)
    , mNoSamples(100000)
    , mSPoints(nullptr)
    , mNPoints(nullptr)
    , mSMats(nullptr)
    , mNMats(nullptr)
    , mSQuats(nullptr)
    , mNQuats(nullptr) {
  }

  SpeedTester::~SpeedTester(void) {
    AlignedFree(mSPoints);
    AlignedFree(mNPoints);
    AlignedFree(mNMats);
    AlignedFree(mSMats);
    AlignedFree(mNQuats);
    AlignedFree(mSQuats);
  }

  void SpeedTester::generateSamples(void) {
    AlignedFree(mSPoints);
    AlignedFree(mNPoints);
    AlignedFree(mNMats);
    AlignedFree(mSMats);
    AlignedFree(mNQuats);
    AlignedFree(mSQuats);

    mSPoints = reinterpret_cast<SFloats*>(AlignedAlloc(sizeof(SFloats)*mNoSamples));
    mNPoints = reinterpret_cast<Vec3*>(AlignedAlloc(sizeof(Vec3)*mNoSamples));
    mNMats = reinterpret_cast<Mat3*>(AlignedAlloc(sizeof(Mat3)*mNoSamples));
    mSMats = reinterpret_cast<SMat3*>(AlignedAlloc(sizeof(SMat3)*mNoSamples));
    mNQuats = reinterpret_cast<Quat*>(AlignedAlloc(sizeof(Quat)*mNoSamples));
    mSQuats = reinterpret_cast<SFloats*>(AlignedAlloc(sizeof(SFloats)*mNoSamples));

    for(size_t i = 0; i < mNoSamples; ++i) {
      mSPoints[i] = sLoadFloats(randFloat(), randFloat(), randFloat());
      mNPoints[i] = Vec3(randFloat(), randFloat(), randFloat());
      mNMats[i] = Mat3(randFloat(), randFloat(), randFloat(), randFloat(), randFloat(), randFloat(), randFloat(), randFloat(), randFloat());
      mSMats[i] = SMat3(randFloat(), randFloat(), randFloat(), randFloat(), randFloat(), randFloat(), randFloat(), randFloat(), randFloat());
      mNQuats[i] = Quat(randFloat(), randFloat(), randFloat(), randFloat());
      mSQuats[i] = sLoadFloats(randFloat(), randFloat(), randFloat(), randFloat());
    }
  }

#define SpeedTestOp(name, total, op)\
  {\
  Profiler profiler;\
  profiler.pushBlock(name);\
  for(size_t j = 0; j < mNoSamples; ++j)\
    op;\
  profiler.popBlock(name);\
  const ProfileResult* block = profiler.getBlock(name);\
  total += block->mDuration.count();\
  }

#define LoopSpeedTest(opA, opB)\
    long long totalA = 0;\
    long long totalB = 0;\
    generateSamples();\
    for(size_t i = 0; i < mNoIterations; ++i)\
      SpeedTestOp("OpA", totalA, opA);\
    generateSamples();\
    for(size_t i = 0; i < mNoIterations; ++i)\
      SpeedTestOp("OpB", totalB, opB);\
    return { totalA, totalB };

  Mat3 getRotationMatrix(void) {
    float r[9] = { 0.612361f, 0.729383f, 0.198896f,
                   0.779353f, 0.334213f, 0.569723f,
                   0.176786f, 0.126579f, 0.15549f };

    return Mat3(r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]);
  }

  TimePair SpeedTester::sDotVsSdot() {
    //LoopSpeedTest(m_sPoints[j] = SVec3::dot(m_sPoints[j], (m_sPoints[j]), m_sPoints[j] = m_sPoints[j].dot(m_sPoints[j]));
    return { 0,0 };
  }

  TimePair SpeedTester::rotVecSMatVsSQuat(void) {
    Mat3 r = getRotationMatrix();
    SMat3 sb = toSMat3(r);
    SFloats q = sb.toQuat();

    //LoopSpeedTest(m_sPoints[j] = sb*m_sPoints[j], m_sPoints[j] = q.Rotate(m_sPoints[j]));
    return { 0,0 };
  }

  TimePair SpeedTester::rotVecSQuatVsNQuat(void) {
    Mat3 r = getRotationMatrix();
    SMat3 sb = toSMat3(r);
    SFloats sq = sb.toQuat();
    Quat nq = r.toQuat();

    //LoopSpeedTest(m_sPoints[j] = sq.Rotate(m_sPoints[j]), m_nPoints[j] = nq*m_nPoints[j]);
    return { 0,0 };
  }

  TimePair SpeedTester::rotVecSMatVsNMat(void) {
    Mat3 nr = getRotationMatrix();
    SMat3 sr = toSMat3(nr);

    LoopSpeedTest(mSPoints[j] = sr*mSPoints[j], mNPoints[j] = nr*mNPoints[j]);
  }

  TimePair SpeedTester::rotVecTransSMatVsNMat(void) {
    Mat3 nr = getRotationMatrix();
    SMat3 sr = toSMat3(nr);

    LoopSpeedTest(mSPoints[j] = sr.transposedMultiply(mSPoints[j]), mNPoints[j] = nr.transposedMultiply(mNPoints[j]));
  }

  TimePair SpeedTester::sDotVsNdot(void) {
    //LoopSpeedTest(m_sPoints[j] = m_sPoints[j].dot(m_sPoints[j]), m_nPoints[j] = Vec3(m_nPoints[j].dot(m_nPoints[j])));
    return { 0,0 };
  }

  TimePair SpeedTester::matMatMultSVsN(void) {
    LoopSpeedTest(mSMats[j] = mSMats[j] * mSMats[j], mNMats[j] = mNMats[j] * mNMats[j]);
  }

  TimePair SpeedTester::matMatMultTransSVsN(void) {
    LoopSpeedTest(mSMats[j] = mSMats[j].transposedMultiply(mSMats[j]), mNMats[j] = mNMats[j].transposedMultiply(mNMats[j]));
  }

  TimePair SpeedTester::quatQuatMultSVsN(void) {
    //LoopSpeedTest(m_sQuats[j] = m_sQuats[j]*m_sQuats[j], m_nQuats[j] = m_nQuats[j]*m_nQuats[j]);
    return { 0,0 };
  }

  std::vector<std::pair<std::string, TimePair>> SpeedTester::testAll(void) {
    std::vector<std::pair<std::string, TimePair>> result;
    result.push_back({ "Base Test Should have same times", sDotVsSdot() });
    result.push_back({ "Base Test Should have same times", sDotVsSdot() });
    result.push_back({ "Vector Rotation SMat vs SFloats", rotVecSMatVsSQuat() });
    result.push_back({ "Vector Rotation SFloats vs NQuat", rotVecSQuatVsNQuat() });
    result.push_back({ "Vector Rotation SMat vs NMat", rotVecSMatVsNMat() });
    result.push_back({ "Vector Rotation Transposed SMat vs NMat", rotVecTransSMatVsNMat() });
    result.push_back({ "SIMD Dot product vs normal", sDotVsNdot() });
    result.push_back({ "SIMD Matrix multiplication vs normal", matMatMultSVsN() });
    result.push_back({ "SIMD Matrix multiplication transposed vs normal", matMatMultTransSVsN() });
    result.push_back({ "SIMD Quaternion multiplication vs normal", quatQuatMultSVsN() });
    return result;
  }

}
#endif