#include "Precompile.h"
#include "SyxSpeedTests.h"

#ifdef SENABLED
namespace Syx {
  SpeedTester::SpeedTester(void) : m_noIterations(1000), m_noSamples(100000),
    m_sPoints(nullptr), m_nPoints(nullptr), m_sMats(nullptr), m_nMats(nullptr), m_sQuats(nullptr), m_nQuats(nullptr) {
  }

  SpeedTester::~SpeedTester(void) {
    AlignedFree(m_sPoints);
    AlignedFree(m_nPoints);
    AlignedFree(m_nMats);
    AlignedFree(m_sMats);
    AlignedFree(m_nQuats);
    AlignedFree(m_sQuats);
  }

  void SpeedTester::GenerateSamples(void) {
    AlignedFree(m_sPoints);
    AlignedFree(m_nPoints);
    AlignedFree(m_nMats);
    AlignedFree(m_sMats);
    AlignedFree(m_nQuats);
    AlignedFree(m_sQuats);

    m_sPoints = reinterpret_cast<SFloats*>(AlignedAlloc(sizeof(SFloats)*m_noSamples));
    m_nPoints = reinterpret_cast<Vec3*>(AlignedAlloc(sizeof(Vec3)*m_noSamples));
    m_nMats = reinterpret_cast<Mat3*>(AlignedAlloc(sizeof(Mat3)*m_noSamples));
    m_sMats = reinterpret_cast<SMat3*>(AlignedAlloc(sizeof(SMat3)*m_noSamples));
    m_nQuats = reinterpret_cast<Quat*>(AlignedAlloc(sizeof(Quat)*m_noSamples));
    m_sQuats = reinterpret_cast<SFloats*>(AlignedAlloc(sizeof(SFloats)*m_noSamples));

    for(size_t i = 0; i < m_noSamples; ++i) {
      m_sPoints[i] = SLoadFloats(RandFloat(), RandFloat(), RandFloat());
      m_nPoints[i] = Vec3(RandFloat(), RandFloat(), RandFloat());
      m_nMats[i] = Mat3(RandFloat(), RandFloat(), RandFloat(), RandFloat(), RandFloat(), RandFloat(), RandFloat(), RandFloat(), RandFloat());
      m_sMats[i] = SMat3(RandFloat(), RandFloat(), RandFloat(), RandFloat(), RandFloat(), RandFloat(), RandFloat(), RandFloat(), RandFloat());
      m_nQuats[i] = Quat(RandFloat(), RandFloat(), RandFloat(), RandFloat());
      m_sQuats[i] = SLoadFloats(RandFloat(), RandFloat(), RandFloat(), RandFloat());
    }
  }

#define SpeedTestOp(name, total, op)\
  {\
  Profiler profiler;\
  profiler.PushBlock(name);\
  for(size_t j = 0; j < m_noSamples; ++j)\
    op;\
  profiler.PopBlock(name);\
  const ProfileResult* block = profiler.GetBlock(name);\
  total += block->m_duration.count();\
  }

#define LoopSpeedTest(opA, opB)\
    long long totalA = 0;\
    long long totalB = 0;\
    GenerateSamples();\
    for(size_t i = 0; i < m_noIterations; ++i)\
      SpeedTestOp("OpA", totalA, opA);\
    GenerateSamples();\
    for(size_t i = 0; i < m_noIterations; ++i)\
      SpeedTestOp("OpB", totalB, opB);\
    return { totalA, totalB };

  Mat3 GetRotationMatrix(void) {
    float r[9] = { 0.612361f, 0.729383f, 0.198896f,
                   0.779353f, 0.334213f, 0.569723f,
                   0.176786f, 0.126579f, 0.15549f };

    return Mat3(r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]);
  }

  TimePair SpeedTester::SDotVsSDot(void) {
    //LoopSpeedTest(m_sPoints[j] = SVec3::Dot(m_sPoints[j], (m_sPoints[j]), m_sPoints[j] = m_sPoints[j].Dot(m_sPoints[j]));
    return { 0,0 };
  }

  TimePair SpeedTester::RotVecSMatVsSQuat(void) {
    Mat3 r = GetRotationMatrix();
    SMat3 sb = ToSMat3(r);
    SFloats q = sb.ToQuat();

    //LoopSpeedTest(m_sPoints[j] = sb*m_sPoints[j], m_sPoints[j] = q.Rotate(m_sPoints[j]));
    return { 0,0 };
  }

  TimePair SpeedTester::RotVecSQuatVsNQuat(void) {
    Mat3 r = GetRotationMatrix();
    SMat3 sb = ToSMat3(r);
    SFloats sq = sb.ToQuat();
    Quat nq = r.ToQuat();

    //LoopSpeedTest(m_sPoints[j] = sq.Rotate(m_sPoints[j]), m_nPoints[j] = nq*m_nPoints[j]);
    return { 0,0 };
  }

  TimePair SpeedTester::RotVecSMatVsNMat(void) {
    Mat3 nr = GetRotationMatrix();
    SMat3 sr = ToSMat3(nr);

    LoopSpeedTest(m_sPoints[j] = sr*m_sPoints[j], m_nPoints[j] = nr*m_nPoints[j]);
  }

  TimePair SpeedTester::RotVecTransSMatVsNMat(void) {
    Mat3 nr = GetRotationMatrix();
    SMat3 sr = ToSMat3(nr);

    LoopSpeedTest(m_sPoints[j] = sr.TransposedMultiply(m_sPoints[j]), m_nPoints[j] = nr.TransposedMultiply(m_nPoints[j]));
  }

  TimePair SpeedTester::SDotVsNDot(void) {
    //LoopSpeedTest(m_sPoints[j] = m_sPoints[j].Dot(m_sPoints[j]), m_nPoints[j] = Vec3(m_nPoints[j].Dot(m_nPoints[j])));
    return { 0,0 };
  }

  TimePair SpeedTester::MatMatMultSVsN(void) {
    LoopSpeedTest(m_sMats[j] = m_sMats[j] * m_sMats[j], m_nMats[j] = m_nMats[j] * m_nMats[j]);
  }

  TimePair SpeedTester::MatMatMultTransSVsN(void) {
    LoopSpeedTest(m_sMats[j] = m_sMats[j].TransposedMultiply(m_sMats[j]), m_nMats[j] = m_nMats[j].TransposedMultiply(m_nMats[j]));
  }

  TimePair SpeedTester::QuatQuatMultSVsN(void) {
    //LoopSpeedTest(m_sQuats[j] = m_sQuats[j]*m_sQuats[j], m_nQuats[j] = m_nQuats[j]*m_nQuats[j]);
    return { 0,0 };
  }

  std::vector<std::pair<std::string, TimePair>> SpeedTester::TestAll(void) {
    std::vector<std::pair<std::string, TimePair>> result;
    result.push_back({ "Base Test Should have same times", SDotVsSDot() });
    result.push_back({ "Base Test Should have same times", SDotVsSDot() });
    result.push_back({ "Vector Rotation SMat vs SFloats", RotVecSMatVsSQuat() });
    result.push_back({ "Vector Rotation SFloats vs NQuat", RotVecSQuatVsNQuat() });
    result.push_back({ "Vector Rotation SMat vs NMat", RotVecSMatVsNMat() });
    result.push_back({ "Vector Rotation Transposed SMat vs NMat", RotVecTransSMatVsNMat() });
    result.push_back({ "SIMD Dot product vs normal", SDotVsNDot() });
    result.push_back({ "SIMD Matrix multiplication vs normal", MatMatMultSVsN() });
    result.push_back({ "SIMD Matrix multiplication transposed vs normal", MatMatMultTransSVsN() });
    result.push_back({ "SIMD Quaternion multplication vs normal", QuatQuatMultSVsN() });
    return result;
  }

}
#endif