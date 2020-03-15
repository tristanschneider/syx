#include "Precompile.h"
#include "SyxModel.h"
#include "SyxTransform.h"
#include "SyxDebugHelpers.h"
#include "SyxAABBTree.h"

namespace Syx {
#ifdef SENABLED
  SFloats Model::sGetSupport(SFloats dir) const {
    switch(mType) {
      case ModelType::Capsule: return _sGetCapsuleSupport(dir);
      case ModelType::Cone: return _sGetConeSupport(dir);
      case ModelType::Cube: return _sGetCubeSupport(dir);
      case ModelType::Cylinder: return _sGetCylinderSupport(dir);
      case ModelType::Mesh: return _sGetMeshSupport(dir);
      case ModelType::Sphere: return _sGetSphereSupport(dir);
      case ModelType::Triangle: return _sGetTriSupport(dir);
      default: SyxAssertError(false, "Invalid shape type");
    }
    return SVec3::Zero;
  }

  SFloats Model::_sGetMeshSupport(SFloats dir) const {
    if(mPoints.empty())
      return SVec3::Zero;

    SFloats bestPoint = SLoadAll(&mPoints[0].x);
    SFloats bestDot = SVec3::dot(bestPoint, dir);

    for(size_t i = 1; i < mPoints.size(); ++i) {
      SFloats curPoint = SLoadAll(&mPoints[i].x);
      SFloats curDot = SVec3::dot(curPoint, dir);

      if(SIGreaterLower(curDot, bestDot)) {
        bestPoint = curPoint;
        bestDot = curDot;
      }
    }
    return SFloats(bestPoint);
  }

  SFloats Model::_sGetCubeSupport(SFloats dir) const {
    //Use selection per component to choose proper extents
    return sSelectIf(SGreaterAll(dir, SVec3::Zero), SVec3::Identity, SVec3::neg(SVec3::Identity));
  }

  SFloats Model::_sGetSphereSupport(SFloats dir) const {
    return SVec3::normalized(dir);
  }

  SFloats Model::_sGetCylinderSupport(SFloats dir) const {
    SFloats posY = SGreaterAll(dir, SVec3::Zero);
    //Primary axis line support
    //The comparison has x and z bits, but those won't matter, since unitY only has y bits
    SFloats result = sSelectIf(posY, SVec3::UnitY, SVec3::neg(SVec3::UnitY));

    //Secondary axes circle portion
    static const SFloats bitsXZ = SOr(SVec3::BitsX, SVec3::BitsZ);
    result = SAddAll(result, SVec3::normalized(SAnd(dir, bitsXZ)));
    return result;
  }

  SFloats Model::_sGetCapsuleSupport(SFloats dir) const {
    SFloats posY = SGreaterAll(dir, SVec3::Zero);
    //Primary axis line support
    //The comparison has x and z bits, but those won't matter, since unitY only has y bits
    SFloats result = sSelectIf(posY, SVec3::UnitY, SVec3::neg(SVec3::UnitY));

    //Secondary axes sphere portion
    result = SAddAll(result, SVec3::normalized(dir));
    return result;
  }

  SFloats Model::_sGetConeSupport(SFloats dir) const {
    //Center is at center of mass which is 1/4 from the base to the tip
    //this means the base is 0.5 down and the tip is 1.5 up, since the shape is from -1 to 1
    static const SFloats toTip = sLoadFloats(0.0f, 1.5f, 0.0f, 0.0f);
    static const SFloats toBase = sLoadFloats(0.0f, -0.5f, 0.0f, 0.0f);

    //Primary axis line support subtract base so comparison is done in middle of cone
    SFloats negY = SLessAll(SSubAll(dir, toBase), SVec3::Zero);
    SFloats result = sSelectIf(negY, toBase, toTip);

    //Secondary axis circle if at base, or canceled out by and if at tip
    static const SFloats bitsXZ = SOr(SVec3::BitsX, SVec3::BitsZ);
    result = SAddAll(result, SAnd(negY, SAnd(bitsXZ, dir)));
    return result;
  }

  SFloats Model::_sGetTriSupport(SFloats dir) const {
    SFloats sa = SLoadAll(&mPoints[0].x);
    SFloats sb = SLoadAll(&mPoints[1].x);
    SFloats sc = SLoadAll(&mPoints[2].x);
    SFloats da = SVec3::dot(dir, sa);
    SFloats db = SVec3::dot(dir, sb);
    SFloats dc = SVec3::dot(dir, sc);
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

  Model::Model(int type, const Vec3Vec& points, const Vec3Vec& triangles, std::unique_ptr<Broadphase> broadphase, const AABB& aabb)
    : mType(type)
    , mPoints(points)
    , mTriangles(triangles)
    , mAABB(aabb) {
  }


  Model::Model(const Vec3Vec& points, const Vec3Vec& triangles, bool environment)
    : Model(environment ? ModelType::Environment : ModelType::Mesh, points, triangles, Create::aabbTree(), AABB(points)) {
  }

  //All primitives are from -1.0 to 1.0 so I don't need to multiply by 0.5 when getting support points
  Model::Model(int type)
    : Model(type, {}, {}, Create::aabbTree(), AABB(-Vec3::Identity, Vec3::Identity)) {
    switch(mType) {
      case ModelType::Triangle:
        mPoints.resize(3);
        break;
      case ModelType::Capsule:
        mAABB = AABB(Vec3(-1.0f, -2.0f, -1.0f), Vec3(1.0f, 2.0f, 1.0f));
        break;
    }
  }

  Model::~Model() = default;

  Vec3 Model::getSupport(const Vec3& dir) const {
    switch(mType) {
      case ModelType::Capsule: return _getCapsuleSupport(dir);
      case ModelType::Cone: return _getConeSupport(dir);
      case ModelType::Cube: return _getCubeSupport(dir);
      case ModelType::Cylinder: return _getCylinderSupport(dir);
      case ModelType::Mesh: return _getMeshSupport(dir);
      case ModelType::Sphere: return _getSphereSupport(dir);
      case ModelType::Triangle: return _getTriSupport(dir);
      default: SyxAssertError(false, "Invalid shape type");
    }
    return Vec3::Zero;
  }

  Vec3 Model::_getMeshSupport(const Vec3& dir) const {
    if(mPoints.empty())
      return Vec3::Zero;

    float bestDot = mPoints[0].dot(dir);
    const Vec3* bestPoint = &mPoints[0];

    for(const Vec3& point : mPoints) {
      float curDot = point.dot(dir);
      if(curDot > bestDot) {
        bestDot = curDot;
        bestPoint = &point;
      }
    }

    return *bestPoint;
  }

  Vec3 Model::_getCubeSupport(const Vec3& dir) const {
    Vec3 result;
    for(int i = 0; i < 3; ++i)
      result[i] = dir[i] > 0.0f ? 1.0f : -1.0f;
    return result;
  }

  Vec3 Model::_getSphereSupport(const Vec3& dir) const {
    return dir.safeNormalized();
  }

  //I'll implement these later
  Vec3 Model::_getCylinderSupport(const Vec3& dir) const {
    return dir;
  }

  Vec3 Model::_getCapsuleSupport(const Vec3& dir) const {
    Vec3 sphere = dir.y > 0.0f ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(0.0f, -1.0f, 0.0f);
    return sphere + dir.safeNormalized();
  }

  Vec3 Model::_getConeSupport(const Vec3& dir) const {
    return dir;
  }

  Vec3 Model::_getTriSupport(const Vec3& dir) const {
    float a = mPoints[0].dot(dir);
    float b = mPoints[1].dot(dir);
    float c = mPoints[2].dot(dir);
    if(a > b) {
      if(a > c)
        return mPoints[0];
      return mPoints[2];
    }
    if(b > c)
      return mPoints[1];
    return mPoints[2];
  }

  void Model::draw(const Transformer& toWorld) const {
    DebugDrawer& d = DebugDrawer::get();
    //Primitives are double the size of their scale to make support points slightly simpler
    float primScale = 2.0f;

    switch(mType) {
      case ModelType::Sphere: d.drawSphere(toWorld.mPos, toWorld.getScale().x, toWorld.mScaleRot.mbx.normalized(), toWorld.mScaleRot.mby.normalized()); break;
      case ModelType::Cube: d.drawCube(toWorld.mPos, toWorld.getScale()*primScale, toWorld.mScaleRot.mbx.normalized(), toWorld.mScaleRot.mby.normalized()); break;
      case ModelType::Composite: {
        for(const ModelInstance& inst : mInstances) {
          inst.getModel().draw(Transformer::combined(inst.mModelToWorld, toWorld)); 
        }
        break;
      }
      case ModelType::Environment:
      case ModelType::Mesh: {
        for(size_t i = 0; i + 2 < mTriangles.size(); i += 3)
          drawTriangle(toWorld.transformPoint(mTriangles[i]), toWorld.transformPoint(mTriangles[i + 1]), toWorld.transformPoint(mTriangles[i + 2]));
        break;
      }
      case ModelType::Capsule: drawCapsule(toWorld.mPos, toWorld.mScaleRot); break;
      case ModelType::Cone:
      case ModelType::Cylinder:
        SyxAssertWarning(false, "Not implemented");
    }
  }

  AABB Model::getWorldAABB(const Transformer& toWorld) const {
    switch(mType) {
      case ModelType::Composite: return _getCompositeWorldAABB(toWorld);
      case ModelType::Environment: return _getEnvironmentWorldAABB(toWorld);
      case ModelType::Sphere: return _getSphereWorldAABB(toWorld);
      default: return _getBaseWorldAABB(toWorld);
    }
  }

  void Model::setTriangle(const Vec3& a, const Vec3& b, const Vec3& c) {
    mPoints[0] = a;
    mPoints[1] = b;
    mPoints[2] = c;
  }

  AABB Model::_getCompositeWorldAABB(const Transformer& toWorld) const {
    AABB result = mInstances[0].getAABB().transform(toWorld);
    //Don't transform the master bounding box, do all the little ones, will likely be considerably smaller
    for(size_t i = 1; i < mInstances.size(); ++i) {
      result = AABB::combined(result, mInstances[i].getAABB().transform(toWorld));
    }
    return result;
  }

  AABB Model::_getEnvironmentWorldAABB(const Transformer& toWorld) const {
    AABB result;
    result.init(toWorld.transformPoint(mPoints[0]));
    //Environment isn't going anywhere, so get the tightest fit we can
    for(size_t i = 1; i < mPoints.size(); ++i) {
      result.add(toWorld.transformPoint(mPoints[i]));
    }
    return result;
  }

  AABB Model::_getSphereWorldAABB(const Transformer& toWorld) const {
    Vec3 radius(toWorld.mScaleRot.mbx.length());
    return AABB(toWorld.mPos - radius, toWorld.mPos + radius);
  }

  AABB Model::_getBaseWorldAABB(const Transformer& toWorld) const {
    return mAABB.transform(toWorld);
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

  void Model::_computeMasses(const Vec3Vec& triangles, const Vec3& scale, float& mass, Vec3& centerOfMass, Mat3& inertia) const {
    const double mult[10] = {1.0f/6.0f, 1.0f/24.0f, 1.0f/24.0f, 1.0f/24.0f, 1.0f/60.0f, 1.0f/60.0f, 1.0f/60.0f, 1.0f/120.0f, 1.0f/120.0f, 1.0f/120.0f};

    //Integral
    //order: 1, x, y, z, xˆ2, yˆ2, zˆ2, xy, yz, zx
    double intg[10] = {0.0f};

    for(size_t i = 0; i + 2 < triangles.size(); i += 3) {
      Vec3 a = Vec3::scale(triangles[i], scale);
      Vec3 b = Vec3::scale(triangles[i + 1], scale);
      Vec3 c = Vec3::scale(triangles[i + 2], scale);

      //This is fine with float precision as they don't get too ridiculous
      Vec3 edgeAB = b - a;
      Vec3 edgeAC = c - a;
      Vec3 normal = edgeAB.cross(edgeAC);
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
    double cmx = safeDivide(intg[1], massD, static_cast<double>(SYX_EPSILON));
    double cmy = safeDivide(intg[2], massD, static_cast<double>(SYX_EPSILON));
    double cmz = safeDivide(intg[3], massD, static_cast<double>(SYX_EPSILON));
    //inertia tensor relative to center of mass
    inertia[0][0] = float(intg[5] + intg[6] - massD*(cmy*cmy + cmz*cmz));
    inertia[1][1] = float(intg[4] + intg[6] - massD*(cmz*cmz + cmx*cmx));
    inertia[2][2] = float(intg[4] + intg[5] - massD*(cmx*cmx + cmy*cmy));
    //Because matrix is symmetrical
    inertia[0][1] = inertia[1][0] = float(-(intg[7] - massD*cmx*cmy));
    inertia[1][2] = inertia[2][1] = float(-(intg[8] - massD*cmy*cmz));
    inertia[0][2] = inertia[2][0] = float(-(intg[9] - massD*cmz*cmx));

    centerOfMass = Vec3(float(cmx), float(cmy), float(cmz));
    mass = float(massD);
  }

  void Model::_computeMasses(const Vec3& scale, float& mass, Vec3& centerOfMass, Mat3& inertia) const {
    _computeMasses(mTriangles, scale, mass, centerOfMass, inertia);
  }

  MassInfo Model::_computeMeshMasses(const Vec3& scale) const {
    MassInfo result;
    Mat3 inMat;
    _computeMasses(scale, result.mMass, result.mCenterOfMass, inMat);
    inMat.diagonalize();
    result.mInertia = inMat.getDiagonal();
    return result;
  }

  MassInfo Model::_computeCubeMasses(const Vec3& scale) const {
    MassInfo result;
    result.mCenterOfMass = Vec3::Zero;
    result.mMass = scale.x*scale.y*scale.z;
    AABB bb(-scale, scale);
    result.mInertia = result.mMass*bb.getInertia();
    return result;
  }

  MassInfo Model::_computeSphereMasses(const Vec3& scale) const {
    MassInfo result;
    result.mCenterOfMass = Vec3::Zero;
    //4pir^3/3
    float massMult = 4.0f*SYX_PI/3.0f;
    float radiusSq = scale.x*scale.x;
    result.mMass = massMult*radiusSq*scale.x;

    //2mr^2/5
    float inertiaMult = 2.0f/5.0f;
    result.mInertia = Vec3(inertiaMult*radiusSq*result.mMass);
    return result;
  }

  MassInfo Model::_computeCapsuleMasses(const Vec3& scale) const {
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

  MassInfo Model::_computeCompositeMasses(const Vec3& scale) const {
    MassInfo result;
    result.mCenterOfMass = Vec3::Zero;
    result.mMass = 0.0f;

    //Would be nice to have a buffer in cache and not be static hacky
    static std::vector<MassInfo> infos;
    infos.clear();

    for(const ModelInstance& subInst : mInstances) {
      Vec3 subScale = Vec3::scale(scale, subInst.getLocalTransform().mScale);
      MassInfo instMass = subInst.getModel()._computeMasses(subScale);
      result.mMass += instMass.mMass;
      //Store center of mass of instance relative to parent for later
      instMass.mCenterOfMass = Vec3::scale(subInst.getLocalTransform().mPos, scale) + instMass.mCenterOfMass;
      //COM is weighted average of each body's center of mass
      result.mCenterOfMass += instMass.mMass*instMass.mCenterOfMass;
      infos.push_back(instMass);
    }
    result.mCenterOfMass = Vec3::safeDivide(result.mCenterOfMass, result.mMass);

    Mat3 inertia = Mat3::Zero;
    for(size_t i = 0; i < mInstances.size(); ++i) {
      const ModelInstance& subInst = mInstances[i];
      const MassInfo& instMass = infos[i];
      Mat3 localInertia(instMass.mInertia);
      localInertia = tensorTransform(localInertia, subInst.getLocalTransform().mRot.toMatrix());
      // Transform inertia which was calculated at submodel's center of mass, out to its position relative to the root
      localInertia = tensorTransform(localInertia, instMass.mCenterOfMass - result.mCenterOfMass, instMass.mMass);
      inertia += localInertia;
    }

    inertia.diagonalize();
    result.mInertia = inertia.getDiagonal();
    return result;
  }

  MassInfo Model::_computeEnvironmentMasses(const Vec3&) const {
    //Environments aren't supposed to move, so infinite mass.
    MassInfo zero;
    zero.mCenterOfMass = zero.mInertia = Vec3::Zero;
    zero.mMass = 0.0f;
    return zero;
  }

  MassInfo Model::_computeMasses(const Vec3& scale) const {
    switch(mType) {
      case ModelType::Sphere: return _computeSphereMasses(scale);
      case ModelType::Cube: return _computeCubeMasses(scale);
      case ModelType::Capsule: return _computeCapsuleMasses(scale);
      case ModelType::Mesh: return _computeMeshMasses(scale);
      case ModelType::Composite: return _computeCompositeMasses(scale);
      case ModelType::Environment: return _computeEnvironmentMasses(scale);
      default: Interface::log("Tried to calculate mass for a model that whose mass calculation isn't implemented yet"); break;
    }
    return MassInfo();
  }

  void Model::_offset(const Vec3& offset) {
    for(Vec3& p : mPoints)
      p += offset;
    for(Vec3& p : mTriangles)
      p += offset;
    for(ModelInstance& inst : mInstances) {
      Transform t = inst.getLocalTransform();
      t.mPos += offset;
      inst.setSubmodelInstLocalTransform(t);
    }
    mAABB.move(offset);
  }

  Model::Model(const Model& rhs)
    : mPoints(rhs.mPoints)
    , mTriangles(rhs.mTriangles)
    , mAABB(rhs.mAABB)
    , mType(rhs.mType)
    , mHandle(rhs.mHandle)
    , mInstances(rhs.mInstances)
    , mSubmodels(rhs.mSubmodels)
    , mBroadphase(Create::aabbTree()) {
  }

  Model& Model::operator=(const Model& rhs) {
    mPoints = rhs.mPoints;
    mTriangles = rhs.mTriangles;
    mAABB = rhs.mAABB;
    mType = rhs.mType;
    mHandle = rhs.mHandle;
    mInstances = rhs.mInstances;
    mSubmodels = rhs.mSubmodels;
    mBroadphase = Create::aabbTree();
    return *this;
  }

  void Model::initComposite(const CompositeModelParam& param, const HandleMap<Model>& modelMap) {
    mSubmodels.resize(param.mSubmodels.size());
    mInstances.reserve(param.mInstances.size());

    for(auto pair : param.mSubmodels) {
      mSubmodels[pair.first] = pair.second.toModel();
    }

    for(size_t i = 0; i < param.mInstances.size(); ++i) {
      const CompositeModelParam::SubmodelInstance& instance = param.mInstances[i];
      Model* model;
      if(instance.mLocal)
        model = &mSubmodels[instance.mHandle];
      else {
        model = modelMap.get(instance.mHandle);
        SyxAssertError(model, "Tried to add submodel that didn't exist");
      }
      ModelInstance newInstance(model);
      newInstance.setSubmodelInstLocalTransform(instance.mTransform);

      //Accumulate aabbs of all submodel instances
      if(i)
        mAABB = AABB::combined(mAABB, newInstance.getAABB());
      else
        mAABB = newInstance.getAABB();

      mInstances.push_back(newInstance);
    }
  }

  void Model::initEnvironment() {
    //Should use allocator that is in cache instead of static nonsense
    static std::vector<InsertParam> params;
    params.clear();

    for(size_t i = 0; i + 2 < mTriangles.size(); i += 3) {
      AABB bb;
      bb.init(mTriangles[i]);
      bb.add(mTriangles[i + 1]);
      bb.add(mTriangles[i + 2]);
      params.push_back(InsertParam(BoundingVolume(bb), reinterpret_cast<void*>(i)));
      //Store the handle to be used for collider pairs in the unused fourth component
      Handle handle = ModelInstance::sHandleGen.next();
      mTriangles[i].w = *reinterpret_cast<float*>(&handle);
    }
    mBroadphase->buildStatic(params);
  }
}