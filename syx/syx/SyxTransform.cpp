#include "Precompile.h"
#include "SyxTransform.h"
//#define STRANSFORMS

namespace Syx {
#ifdef SENABLED
  //These two can be replaced with 4x4 SIMD matrix multiplication once I do that
  SFloats STransformer::transformPoint(SFloats point) {
    //4x4 homogeneous matrix multiplication with a point
    return SAddAll(mScaleRot*point, mPos);
  }

  SFloats STransformer::transformVector(SFloats vector) {
    //3x3 matrix multiplication with a vector
    return mScaleRot*vector;
  }

  SFloats Transform::sModelToWorld(SFloats point) const {
    SFloats result = point;
    result = SMulAll(result, toSVec3(mScale));
    result = SQuat::rotate(toSQuat(mRot), result);
    result = SAddAll(result, toSVec3(mPos));
    return result;
  }

  SFloats Transform::sworldToModel(SFloats point) const {
    SFloats result = point;
    result = SSubAll(result, toSVec3(mPos));
    result = SQuat::rotate(SQuat::inversed(toSQuat(mRot)), result);
    result = SMulAll(result, SVec3::reciprocal(toSVec3(mScale)));
    return result;
  }

  STransformer Transform::sgetModelToWorld() const {
    return STransformer(SQuat::toMatrix(toSQuat(mRot))*SMat3(toSVec3(mScale)), toSVec3(mPos));
  }

  STransformer Transform::sGetModelToWorld(const Transform& child) const {
    //Child transform
    SMat3 resultScaleRot = SQuat::toMatrix(toSQuat(child.mRot))*SMat3(toSVec3(child.mScale));
    SFloats resultPos = toSVec3(child.mPos);

    //Concatenate parent scale rotation and translation respectively
    SMat3 parentScale(toSVec3(mScale));
    resultScaleRot = parentScale*resultScaleRot;
    resultPos = parentScale*resultPos;

    SMat3 parentRot = SQuat::toMatrix(toSQuat(mRot));
    resultScaleRot = parentRot*resultScaleRot;
    resultPos = parentRot*resultPos;

    resultPos = SAddAll(resultPos, toSVec3(mPos));
    return STransformer(resultScaleRot, resultPos);
  }

  STransformer Transform::sGetworldToModel() const {
    SMat3 invRot = SQuat::toMatrix(SQuat::inversed(toSQuat(mRot)));
    SMat3 invScale = SVec3::reciprocal(toSVec3(mScale));

    //Calculate fourth column of invScale*invRot*invTranslate
    SFloats resultPoint = SVec3::neg(toSVec3(mPos));
    resultPoint = invRot * resultPoint;
    resultPoint = invScale * resultPoint;
    return STransformer(invScale*invRot, resultPoint);
  }

  STransformer Transform::sGetworldToModel(const Transform& child) const {
    //Inverse Parent transform, position, rotation, scale
    SMat3 resultScaleRot = SQuat::toMatrix(SQuat::inversed(toSQuat(mRot)));
    SFloats resultPos = SVec3::neg(toSVec3(mPos));
    //Apply inverse parent rotation to inverse parent position
    resultPos = resultScaleRot * resultPos;

    //Apply inverse parent scale to parent position
    SMat3 invScale(SVec3::reciprocal(toSVec3(mScale)));
    resultScaleRot = invScale * resultScaleRot;
    resultPos = invScale * resultPos;

    //Inverse child transform position, rotation, scale
    resultPos = SSubAll(resultPos, toSVec3(child.mPos));

    SMat3 invChildRot = SQuat::toMatrix(SQuat::inversed(toSQuat(child.mRot)));
    resultPos = invChildRot * resultPos;
    resultScaleRot = invChildRot * resultScaleRot;

    SMat3 invChildScale(SVec3::reciprocal(toSVec3(child.mScale)));
    resultPos = invChildScale * resultPos;
    resultScaleRot = invChildScale * resultScaleRot;
    return STransformer(resultScaleRot, resultPos);
  }
#else
  STransformer Transform::sgetModelToWorld() const;
  STransformer Transform::sGetModelToWorld(const Transform&) const { return STransformer(); }
  STransformer Transform::sGetworldToModel() const { return STransformer(); }
  STransformer Transform::sGetworldToModel(const Transform&) const { return STransformer(); }

  SVec3 Transform::sModelToWorld(const SVec3&) const { return SVec3(); }
  SVec3 Transform::sworldToModel(const SVec3&) const { return SVec3(); }
#endif

  Vec3 Transformer::transformPoint(const Vec3& point) const {
    if(gOptions.mSimdFlags & SyxOptions::SIMD::Transforms) {
      SAlign Vec3 result;
      SFloats sVec = toSVec3(point);
      SMat3 scaleRot = toSMat3(mScaleRot);

      sVec = scaleRot * sVec;
      sVec = SAddAll(sVec, toSVec3(mPos));

      SVec3::store(sVec, result);
      return result;
    }
    return mScaleRot*point + mPos;
  }

  Vec3 Transformer::transformVector(const Vec3& vector) const {
    if(gOptions.mSimdFlags & SyxOptions::SIMD::Transforms) {
      SAlign Vec3 result;
      SFloats sVec = toSVec3(vector);
      SMat3 scaleRot = toSMat3(mScaleRot);
      sVec = scaleRot * sVec;
      SVec3::store(sVec, result);
      return result;
    }
    return mScaleRot*vector;
  }

  STransformer Transformer::toSIMDPoint() {
    STransformer result;
    result.mPos = toSVec3(mPos);
    result.mScaleRot = toSMat3(mScaleRot);
    return result;
  }

  STransformer Transformer::toSIMDVector() {
    STransformer result;
    result.mScaleRot = toSMat3(mScaleRot);
    return result;
  }

  Vec3 Transformer::getScale() const {
    return Vec3(mScaleRot.mbx.length(), mScaleRot.mby.length(), mScaleRot.mbz.length());
  }

  Transformer Transformer::combined(const Transformer& first, const Transformer& second) {
    return Transformer(second.mScaleRot*first.mScaleRot, 
      second.mScaleRot*first.mPos + second.mPos);
  }

  Vec3 Transform::modelToWorld(const Vec3& point) const {
    Vec3 result = point;
    result.scale(mScale);
    result = mRot*result;
    result += mPos;
    return result;
  }

  Vec3 Transform::worldToModel(const Vec3& point) const {
    Vec3 result = point;
    result -= mPos;
    result = mRot.inversed()*result;
    result.scale(mScale.reciprocal());
    return result;
  }

  Transformer Transform::getModelToWorld() const {
    return Transformer(mRot.toMatrix()*Mat3(mScale), mPos);
  }

  Transformer Transform::getModelToWorld(const Transform& child) const {
    //Child transform
    Mat3 resultScaleRot = child.mRot.toMatrix()*Mat3(child.mScale);
    Vec3 resultPos = child.mPos;

    //Concatenate parent scale rotation and translation respectively
    Mat3 parentScale(mScale);
    resultScaleRot = parentScale * resultScaleRot;
    resultPos = parentScale * resultPos;

    Mat3 parentRot = mRot.toMatrix();
    resultScaleRot = parentRot * resultScaleRot;
    resultPos = parentRot * resultPos;

    resultPos += mPos;
    return Transformer(resultScaleRot, resultPos);
  }

  Transformer Transform::getWorldToModel() const {
    Mat3 invRot = mRot.inversed().toMatrix();
    Mat3 invScale = mScale.reciprocal();

    //Calculate fourth column of invScale*invRot*invTranslate
    Vec3 resultPoint = -mPos;
    resultPoint = invRot * resultPoint;
    resultPoint = invScale * resultPoint;
    return Transformer(invScale*invRot, resultPoint);
  }

  Transformer Transform::getWorldToModel(const Transform& child) const {
    //Inverse Parent transform, position, rotation, scale
    Mat3 resultScaleRot = mRot.inversed().toMatrix();
    Vec3 resultPos = -mPos;
    //Apply inverse parent rotation to inverse parent position
    resultPos = resultScaleRot * resultPos;

    //Apply inverse parent scale to parent position
    Mat3 invScale(mScale.reciprocal());
    resultScaleRot = invScale*resultScaleRot;
    resultPos = invScale*resultPos;

    //Inverse child transform position, rotation, scale
    resultPos -= child.mPos;

    Mat3 invChildRot = child.mRot.inversed().toMatrix();
    resultPos = invChildRot * resultPos;
    resultScaleRot = invChildRot * resultScaleRot;

    Mat3 invChildScale(child.mScale.reciprocal());
    resultPos = invChildScale * resultPos;
    resultScaleRot = invChildScale * resultScaleRot;
    return Transformer(resultScaleRot, resultPos);
  }
}