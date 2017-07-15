#include "Precompile.h"
#include "SyxModel.h"
#include "SyxTransform.h"
#include "SyxDebugHelpers.h"

namespace Syx {
#ifdef SENABLED
  SFloats Model::SGetSupport(SFloats dir) const {
    switch(mType) {
      case ModelType::Capsule: return SGetCapsuleSupport(dir);
      case ModelType::Cone: return SGetConeSupport(dir);
      case ModelType::Cube: return SGetCubeSupport(dir);
      case ModelType::Cylinder: return SGetCylinderSupport(dir);
      case ModelType::Mesh: return SGetMeshSupport(dir);
      case ModelType::Sphere: return SGetSphereSupport(dir);
      case ModelType::Triangle: return SGetTriSupport(dir);
      default: SyxAssertError(false, "Invalid shape type");
    }
    return SVector3::Zero;
  }

  SFloats Model::SGetMeshSupport(SFloats dir) const {
    if(mPoints.empty())
      return SVector3::Zero;

    SFloats bestPoint = SLoadAll(&mPoints[0].x);
    SFloats bestDot = SVector3::Dot(bestPoint, dir);

    for(size_t i = 1; i < mPoints.size(); ++i) {
      SFloats curPoint = SLoadAll(&mPoints[i].x);
      SFloats curDot = SVector3::Dot(curPoint, dir);

      if(SIGreaterLower(curDot, bestDot)) {
        bestPoint = curPoint;
        bestDot = curDot;
      }
    }
    return SFloats(bestPoint);
  }

  SFloats Model::SGetCubeSupport(SFloats dir) const {
    //Use selection per component to choose proper extents
    return SSelectIf(SGreaterAll(dir, SVector3::Zero), SVector3::Identity, SVector3::Neg(SVector3::Identity));
  }

  SFloats Model::SGetSphereSupport(SFloats dir) const {
    return SVector3::Normalized(dir);
  }

  SFloats Model::SGetCylinderSupport(SFloats dir) const {
    SFloats posY = SGreaterAll(dir, SVector3::Zero);
    //Primary axis line support
    //The comparison has x and z bits, but those won't matter, since unitY only has y bits
    SFloats result = SSelectIf(posY, SVector3::UnitY, SVector3::Neg(SVector3::UnitY));

    //Secondary axes circle portion
    static const SFloats bitsXZ = SOr(SVector3::BitsX, SVector3::BitsZ);
    result = SAddAll(result, SVector3::Normalized(SAnd(dir, bitsXZ)));
    return result;
  }

  SFloats Model::SGetCapsuleSupport(SFloats dir) const {
    SFloats posY = SGreaterAll(dir, SVector3::Zero);
    //Primary axis line support
    //The comparison has x and z bits, but those won't matter, since unitY only has y bits
    SFloats result = SSelectIf(posY, SVector3::UnitY, SVector3::Neg(SVector3::UnitY));

    //Secondary axes sphere portion
    result = SAddAll(result, SVector3::Normalized(dir));
    return result;
  }

  SFloats Model::SGetConeSupport(SFloats dir) const {
    //Center is at center of mass which is 1/4 from the base to the tip
    //this means the base is 0.5 down and the tip is 1.5 up, since the shape is from -1 to 1
    static const SFloats toTip = SLoadFloats(0.0f, 1.5f, 0.0f, 0.0f);
    static const SFloats toBase = SLoadFloats(0.0f, -0.5f, 0.0f, 0.0f);

    //Primary axis line support subtract base so comparison is done in middle of cone
    SFloats negY = SLessAll(SSubAll(dir, toBase), SVector3::Zero);
    SFloats result = SSelectIf(negY, toBase, toTip);

    //Secondary axis circle if at base, or canceled out by and if at tip
    static const SFloats bitsXZ = SOr(SVector3::BitsX, SVector3::BitsZ);
    result = SAddAll(result, SAnd(negY, SAnd(bitsXZ, dir)));
    return result;
  }

  SFloats Model::SGetTriSupport(SFloats dir) const {
    SFloats sa = SLoadAll(&mPoints[0].x);
    SFloats sb = SLoadAll(&mPoints[1].x);
    SFloats sc = SLoadAll(&mPoints[2].x);
    SFloats da = SVector3::Dot(dir, sa);
    SFloats db = SVector3::Dot(dir, sb);
    SFloats dc = SVector3::Dot(dir, sc);
    if(SIGreaterLower(da, db)) {
      if(SIGreaterLower(da, dc))
        return sa;
      return sc;
    }
    if(SIGreaterLower(db, dc))
      return sb;
    return sc;
  }
#endif

  Model::Model(const Vector3Vec& points, const Vector3Vec& triangles, bool environment)
    : mType(environment ? ModelType::Environment : ModelType::Mesh)
    , mPoints(points)
    , mTriangles(triangles)
    , mAABB(points) {
  }

  //All primitives are from -1.0 to 1.0 so I don't need to multiply by 0.5 when getting support points
  Model::Model(int type)
    : mType(type)
    , mAABB(-Vector3::Identity, Vector3::Identity) {
    switch(mType) {
      case ModelType::Triangle:
        mPoints.resize(3);
        break;
      case ModelType::Capsule:
        mAABB = AABB(Vec3(-1.0f, -2.0f, -1.0f), Vec3(1.0f, 2.0f, 1.0f));
        break;
    }
  }


  Vector3 Model::GetSupport(const Vector3& dir) const {
    switch(mType) {
      case ModelType::Capsule: return GetCapsuleSupport(dir);
      case ModelType::Cone: return GetConeSupport(dir);
      case ModelType::Cube: return GetCubeSupport(dir);
      case ModelType::Cylinder: return GetCylinderSupport(dir);
      case ModelType::Mesh: return GetMeshSupport(dir);
      case ModelType::Sphere: return GetSphereSupport(dir);
      case ModelType::Triangle: return GetTriSupport(dir);
      default: SyxAssertError(false, "Invalid shape type");
    }
    return Vector3::Zero;
  }

  Vector3 Model::GetMeshSupport(const Vector3& dir) const {
    if(mPoints.empty())
      return Vector3::Zero;

    float bestDot = mPoints[0].Dot(dir);
    const Vector3* bestPoint = &mPoints[0];

    for(const Vector3& point : mPoints) {
      float curDot = point.Dot(dir);
      if(curDot > bestDot) {
        bestDot = curDot;
        bestPoint = &point;
      }
    }

    return *bestPoint;
  }

  Vector3 Model::GetCubeSupport(const Vector3& dir) const {
    Vector3 result;
    for(int i = 0; i < 3; ++i)
      result[i] = dir[i] > 0.0f ? 1.0f : -1.0f;
    return result;
  }

  Vector3 Model::GetSphereSupport(const Vector3& dir) const {
    return dir.SafeNormalized();
  }

  //I'll implement these later
  Vector3 Model::GetCylinderSupport(const Vector3& dir) const {
    return dir;
  }

  Vector3 Model::GetCapsuleSupport(const Vector3& dir) const {
    Vec3 sphere = dir.y > 0.0f ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(0.0f, -1.0f, 0.0f);
    return sphere + dir.SafeNormalized();
  }

  Vector3 Model::GetConeSupport(const Vector3& dir) const {
    return dir;
  }

  Vector3 Model::GetTriSupport(const Vector3& dir) const {
    float a = mPoints[0].Dot(dir);
    float b = mPoints[1].Dot(dir);
    float c = mPoints[2].Dot(dir);
    if(a > b) {
      if(a > c)
        return mPoints[0];
      return mPoints[2];
    }
    if(b > c)
      return mPoints[1];
    return mPoints[2];
  }

  void Model::Draw(const Transformer& toWorld) const {
    DebugDrawer& d = DebugDrawer::Get();
    //Primitives are double the size of their scale to make support points slightly simpler
    float primScale = 2.0f;

    switch(mType) {
      case ModelType::Sphere: d.DrawSphere(toWorld.mPos, toWorld.GetScale().x, toWorld.mScaleRot.mbx.Normalized(), toWorld.mScaleRot.mby.Normalized()); break;
      case ModelType::Cube: d.DrawCube(toWorld.mPos, toWorld.GetScale()*primScale, toWorld.mScaleRot.mbx.Normalized(), toWorld.mScaleRot.mby.Normalized()); break;
      case ModelType::Composite: {
        for(const ModelInstance& inst : mInstances) {
          inst.GetModel().Draw(Transformer::Combined(inst.mModelToWorld, toWorld)); 
        }
        break;
      }
      case ModelType::Environment:
      case ModelType::Mesh: {
        for(size_t i = 0; i + 2 < mTriangles.size(); i += 3)
          DrawTriangle(toWorld.TransformPoint(mTriangles[i]), toWorld.TransformPoint(mTriangles[i + 1]), toWorld.TransformPoint(mTriangles[i + 2]));
        break;
      }
      case ModelType::Capsule: DrawCapsule(toWorld.mPos, toWorld.mScaleRot); break;
      case ModelType::Cone:
      case ModelType::Cylinder:
        SyxAssertWarning(false, "Not implemented");
    }
  }

  AABB Model::GetWorldAABB(const Transformer& toWorld) const {
    switch(mType) {
      case ModelType::Composite: return GetCompositeWorldAABB(toWorld);
      case ModelType::Environment: return GetEnvironmentWorldAABB(toWorld);
      case ModelType::Sphere: return GetSphereWorldAABB(toWorld);
      default: return GetBaseWorldAABB(toWorld);
    }
  }

  void Model::SetTriangle(const Vector3& a, const Vector3& b, const Vector3& c) {
    mPoints[0] = a;
    mPoints[1] = b;
    mPoints[2] = c;
  }

  AABB Model::GetCompositeWorldAABB(const Transformer& toWorld) const {
    AABB result = mInstances[0].GetAABB().Transform(toWorld);
    //Don't transform the master bounding box, do all the little ones, will likely be considerably smaller
    for(size_t i = 1; i < mInstances.size(); ++i) {
      result = AABB::Combined(result, mInstances[i].GetAABB().Transform(toWorld));
    }
    return result;
  }

  AABB Model::GetEnvironmentWorldAABB(const Transformer& toWorld) const {
    AABB result;
    result.Init(toWorld.TransformPoint(mPoints[0]));
    //Environment isn't going anywhere, so get the tightest fit we can
    for(size_t i = 1; i < mPoints.size(); ++i) {
      result.Add(toWorld.TransformPoint(mPoints[i]));
    }
    return result;
  }

  AABB Model::GetSphereWorldAABB(const Transformer& toWorld) const {
    Vector3 radius(toWorld.mScaleRot.mbx.Length());
    return AABB(toWorld.mPos - radius, toWorld.mPos + radius);
  }

  AABB Model::GetBaseWorldAABB(const Transformer& toWorld) const {
    return mAABB.Transform(toWorld);
  }

  //http://www.geometrictools.com/Documentation/PolyhedralMassProperties.pdf
#define Subexpressions(w0f, w1f, w2f, f1 , f2 , f3 , g0 , g1 , g2)\
    double f1, f2, f3, g0, g1, g2;\
    {\
      double w0 = double(w0f); double w1 = double(w1f); double w2 = double(w2f);\
      temp0 = w0+w1; f1 = temp0+w2; temp1 = w0*w0; temp2 = temp1+w1*temp0;\
      f2 = temp2+w2*f1; f3 = w0*temp1+w1*temp2+w2*f2;\
      g0 = f2+w0*(f1+w0); g1 = f2+w1*(f1+w1); g2 = f2+w2*(f1+w2);\
    }

  void Model::ComputeMasses(const Vector3Vec& triangles, const Vector3& scale, float& mass, Vector3& centerOfMass, Matrix3& inertia) const {
    const double mult[10] = {1.0f/6.0f, 1.0f/24.0f, 1.0f/24.0f, 1.0f/24.0f, 1.0f/60.0f, 1.0f/60.0f, 1.0f/60.0f, 1.0f/120.0f, 1.0f/120.0f, 1.0f/120.0f};

    //Integral
    //order: 1, x, y, z, xˆ2, yˆ2, zˆ2, xy, yz, zx
    double intg[10] = {0.0f};

    for(size_t i = 0; i + 2 < triangles.size(); i += 3) {
      Vector3 a = Vector3::Scale(triangles[i], scale);
      Vector3 b = Vector3::Scale(triangles[i + 1], scale);
      Vector3 c = Vector3::Scale(triangles[i + 2], scale);

      //This is fine with float precision as they don't get too ridiculous
      Vector3 edgeAB = b - a;
      Vector3 edgeAC = c - a;
      Vector3 normal = edgeAB.Cross(edgeAC);
      double nx = double(normal.x);
      double ny = double(normal.y);
      double nz = double(normal.z);

      //compute integral terms
      //Used in Subexpressions
      double temp0, temp1, temp2;
      Subexpressions(a.x, b.x, c.x, f1x, f2x, f3x, g0x, g1x, g2x);
      Subexpressions(a.y, b.y, c.y, f1y, f2y, f3y, g0y, g1y, g2y);
      Subexpressions(a.z, b.z, c.z, f1z, f2z, f3z, g0z, g1z, g2z);

      //update integrals
      intg[0] += nx*f1x;
      intg[1] += nx*f2x; intg[2] += ny*f2y; intg[3] += nz*f2z;
      intg[4] += nx*f3x; intg[5] += ny*f3y; intg[6] += nz*f3z;
      intg[7] += nx*(a.y*g0x+b.y*g1x+c.y*g2x);
      intg[8] += ny*(a.z*g0y+b.z*g1y+c.z*g2y);
      intg[9] += nz*(a.x*g0z+b.x*g1z+c.x*g2z);
    }

    for(int i = 0; i < 10; i++)
      intg[i] *= mult[i];
    double massD = intg[0];
    //center of mass
    double cmx = SafeDivide(intg[1], massD, static_cast<double>(SYX_EPSILON));
    double cmy = SafeDivide(intg[2], massD, static_cast<double>(SYX_EPSILON));
    double cmz = SafeDivide(intg[3], massD, static_cast<double>(SYX_EPSILON));
    //inertia tensor relative to center of mass
    inertia[0][0] = float(intg[5] + intg[6] - massD*(cmy*cmy + cmz*cmz));
    inertia[1][1] = float(intg[4] + intg[6] - massD*(cmz*cmz + cmx*cmx));
    inertia[2][2] = float(intg[4] + intg[5] - massD*(cmx*cmx + cmy*cmy));
    //Because matrix is symmetrical
    inertia[0][1] = inertia[1][0] = float(-(intg[7] - massD*cmx*cmy));
    inertia[1][2] = inertia[2][1] = float(-(intg[8] - massD*cmy*cmz));
    inertia[0][2] = inertia[2][0] = float(-(intg[9] - massD*cmz*cmx));

    centerOfMass = Vector3(float(cmx), float(cmy), float(cmz));
    mass = float(massD);
  }

  void Model::ComputeMasses(const Vector3& scale, float& mass, Vector3& centerOfMass, Matrix3& inertia) const {
    ComputeMasses(mTriangles, scale, mass, centerOfMass, inertia);
  }

  MassInfo Model::ComputeMeshMasses(const Vector3& scale) const {
    MassInfo result;
    Matrix3 inMat;
    ComputeMasses(scale, result.mMass, result.mCenterOfMass, inMat);
    inMat.Diagonalize();
    result.mInertia = inMat.GetDiagonal();
    return result;
  }

  MassInfo Model::ComputeCubeMasses(const Vector3& scale) const {
    MassInfo result;
    result.mCenterOfMass = Vector3::Zero;
    result.mMass = scale.x*scale.y*scale.z;
    AABB bb(-scale, scale);
    result.mInertia = result.mMass*bb.GetInertia();
    return result;
  }

  MassInfo Model::ComputeSphereMasses(const Vector3& scale) const {
    MassInfo result;
    result.mCenterOfMass = Vector3::Zero;
    //4pir^3/3
    float massMult = 4.0f*SYX_PI/3.0f;
    float radiusSq = scale.x*scale.x;
    result.mMass = massMult*radiusSq*scale.x;

    //2mr^2/5
    float inertiaMult = 2.0f/5.0f;
    result.mInertia = Vector3(inertiaMult*radiusSq*result.mMass);
    return result;
  }

  MassInfo Model::ComputeCapsuleMasses(const Vector3& scale) const {
    //https://www.gamedev.net/articles/programming/math-and-physics/capsule-inertia-tensor-r3856
    const float third = 1.0f/3.0f;
    const float eighth = 1.0f/8.0f;
    const float twelfth = 1.0f/12.0f;
    const float twoFifths = 2.0f/5.0f;

    const float& radius = scale.x;
    float cylinderHeight = scale.y*2.0f;
    float radius2 = radius*radius;
    float cylinderMass = SYX_PI*cylinderHeight*radius2;
    float hemisphereMass = SYX_2_PI*third*radius2*radius;

    MassInfo result;
    result.mCenterOfMass = Vec3::Zero;
    result.mMass = 2.0f*hemisphereMass + cylinderMass;
    //Cylinder
    result.mInertia.y = radius2*cylinderMass*0.5f;
    result.mInertia.x = result.mInertia.z = result.mInertia.y*0.5f + cylinderMass*cylinderHeight*cylinderHeight*twelfth;
    //Hemispheres
    float t0 = hemisphereMass*radius2*twoFifths;
    float t1 = 2.0f*(t0 + hemisphereMass*(scale.y*scale.y + 3.0f*eighth*cylinderHeight*radius));
    result.mInertia.x += t1;
    result.mInertia.z += t1;
    return result;
  }

  MassInfo Model::ComputeCompositeMasses(const Vector3& scale) const {
    MassInfo result;
    result.mCenterOfMass = Vector3::Zero;
    result.mMass = 0.0f;

    //Would be nice to have a buffer in cache and not be static hacky
    static std::vector<MassInfo> infos;
    infos.clear();

    for(const ModelInstance& subInst : mInstances) {
      Vector3 subScale = Vector3::Scale(scale, subInst.GetLocalTransform().mScale);
      MassInfo instMass = subInst.GetModel().ComputeMasses(subScale);
      result.mMass += instMass.mMass;
      //Store center of mass of instance relative to parent for later
      instMass.mCenterOfMass = Vector3::Scale(subInst.GetLocalTransform().mPos, scale) + instMass.mCenterOfMass;
      //COM is weighted average of each body's center of mass
      result.mCenterOfMass += instMass.mMass*instMass.mCenterOfMass;
      infos.push_back(instMass);
    }
    result.mCenterOfMass = Vector3::SafeDivide(result.mCenterOfMass, result.mMass);

    Matrix3 inertia = Matrix3::Zero;
    for(size_t i = 0; i < mInstances.size(); ++i) {
      const ModelInstance& subInst = mInstances[i];
      const MassInfo& instMass = infos[i];
      Matrix3 localInertia(instMass.mInertia);
      localInertia = TensorTransform(localInertia, subInst.GetLocalTransform().mRot.ToMatrix());
      // Transform inertia which was calculated at submodel's center of mass, out to its position relative to the root
      localInertia = TensorTransform(localInertia, instMass.mCenterOfMass - result.mCenterOfMass, instMass.mMass);
      inertia += localInertia;
    }

    inertia.Diagonalize();
    result.mInertia = inertia.GetDiagonal();
    return result;
  }

  MassInfo Model::ComputeEnvironmentMasses(const Vector3&) const {
    //Environments aren't supposed to move, so infinite mass.
    MassInfo zero;
    zero.mCenterOfMass = zero.mInertia = Vector3::Zero;
    zero.mMass = 0.0f;
    return zero;
  }

  MassInfo Model::ComputeMasses(const Vector3& scale) const {
    switch(mType) {
      case ModelType::Sphere: return ComputeSphereMasses(scale);
      case ModelType::Cube: return ComputeCubeMasses(scale);
      case ModelType::Capsule: return ComputeCapsuleMasses(scale);
      case ModelType::Mesh: return ComputeMeshMasses(scale);
      case ModelType::Composite: return ComputeCompositeMasses(scale);
      case ModelType::Environment: return ComputeEnvironmentMasses(scale);
      default: Interface::Log("Tried to calculate mass for a model that whose mass calculation isn't implemented yet"); break;
    }
    return MassInfo();
  }

  void Model::Offset(const Vector3& offset) {
    for(Vector3& p : mPoints)
      p += offset;
    for(Vector3& p : mTriangles)
      p += offset;
    for(ModelInstance& inst : mInstances) {
      Transform t = inst.GetLocalTransform();
      t.mPos += offset;
      inst.SetSubmodelInstLocalTransform(t);
    }
    mAABB.Move(offset);
  }

  void Model::InitComposite(const CompositeModelParam& param, const HandleMap<Model>& modelMap) {
    mSubmodels.resize(param.mSubmodels.size());
    mInstances.reserve(param.mInstances.size());

    for(auto pair : param.mSubmodels) {
      mSubmodels[pair.first] = pair.second.ToModel();
    }

    for(size_t i = 0; i < param.mInstances.size(); ++i) {
      const CompositeModelParam::SubmodelInstance& instance = param.mInstances[i];
      Model* model;
      if(instance.mLocal)
        model = &mSubmodels[instance.mHandle];
      else {
        model = modelMap.Get(instance.mHandle);
        SyxAssertError(model, "Tried to add submodel that didn't exist");
      }
      ModelInstance newInstance(model);
      newInstance.SetSubmodelInstLocalTransform(instance.mTransform);

      //Accumulate aabbs of all submodel instances
      if(i)
        mAABB = AABB::Combined(mAABB, newInstance.GetAABB());
      else
        mAABB = newInstance.GetAABB();

      mInstances.push_back(newInstance);
    }
  }

  void Model::InitEnvironment() {
    //Should use allocator that is in cache instead of static nonsense
    static std::vector<InsertParam> params;
    params.clear();

    for(size_t i = 0; i + 2 < mTriangles.size(); i += 3) {
      AABB bb;
      bb.Init(mTriangles[i]);
      bb.Add(mTriangles[i + 1]);
      bb.Add(mTriangles[i + 2]);
      params.push_back(InsertParam(BoundingVolume(bb), reinterpret_cast<void*>(i)));
      //Store the handle to be used for collider pairs in the unused fourth component
      Handle handle = ModelInstance::sHandleGen.Next();
      mTriangles[i].w = *reinterpret_cast<float*>(&handle);
    }
    mBroadphase.BuildStatic(params);
  }
}