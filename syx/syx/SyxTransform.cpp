#include "Precompile.h"
#include "SyxTransform.h"
//#define STRANSFORMS

namespace Syx {
#ifdef SENABLED
  //These two can be replaced with 4x4 SIMD matrix multiplication once I do that
  SFloats STransformer::TransformPoint(SFloats point) {
    //4x4 homogeneous matrix multiplication with a point
    return SAddAll(mScaleRot*point, mPos);
  }

  SFloats STransformer::TransformVector(SFloats vector) {
    //3x3 matrix multiplication with a vector
    return mScaleRot*vector;
  }

  SFloats Transform::SModelToWorld(SFloats point) const {
    SFloats result = point;
    result = SMulAll(result, ToSVec3(mScale));
    result = SQuat::Rotate(ToSQuat(mRot), result);
    result = SAddAll(result, ToSVec3(mPos));
    return result;
  }

  SFloats Transform::SWorldToModel(SFloats point) const {
    SFloats result = point;
    result = SSubAll(result, ToSVec3(mPos));
    result = SQuat::Rotate(SQuat::Inversed(ToSQuat(mRot)), result);
    result = SMulAll(result, SVec3::Reciprocal(ToSVec3(mScale)));
    return result;
  }

  STransformer Transform::SGetModelToWorld() const {
    return STransformer(SQuat::ToMatrix(ToSQuat(mRot))*SMat3(ToSVec3(mScale)), ToSVec3(mPos));
  }

  STransformer Transform::SGetModelToWorld(const Transform& child) const {
    //Child transform
    SMat3 resultScaleRot = SQuat::ToMatrix(ToSQuat(child.mRot))*SMat3(ToSVec3(child.mScale));
    SFloats resultPos = ToSVec3(child.mPos);

    //Concatenate parent scale rotation and translation respectively
    SMat3 parentScale(ToSVec3(mScale));
    resultScaleRot = parentScale*resultScaleRot;
    resultPos = parentScale*resultPos;

    SMat3 parentRot = SQuat::ToMatrix(ToSQuat(mRot));
    resultScaleRot = parentRot*resultScaleRot;
    resultPos = parentRot*resultPos;

    resultPos = SAddAll(resultPos, ToSVec3(mPos));
    return STransformer(resultScaleRot, resultPos);
  }

  STransformer Transform::SGetWorldToModel() const {
    SMat3 invRot = SQuat::ToMatrix(SQuat::Inversed(ToSQuat(mRot)));
    SMat3 invScale = SVec3::Reciprocal(ToSVec3(mScale));

    //Calculate fourth column of invScale*invRot*invTranslate
    SFloats resultPoint = SVec3::Neg(ToSVec3(mPos));
    resultPoint = invRot * resultPoint;
    resultPoint = invScale * resultPoint;
    return STransformer(invScale*invRot, resultPoint);
  }

  STransformer Transform::SGetWorldToModel(const Transform& child) const {
    //Inverse Parent transform, position, rotation, scale
    SMat3 resultScaleRot = SQuat::ToMatrix(SQuat::Inversed(ToSQuat(mRot)));
    SFloats resultPos = SVec3::Neg(ToSVec3(mPos));
    //Apply inverse parent rotation to inverse parent position
    resultPos = resultScaleRot * resultPos;

    //Apply inverse parent scale to parent position
    SMat3 invScale(SVec3::Reciprocal(ToSVec3(mScale)));
    resultScaleRot = invScale * resultScaleRot;
    resultPos = invScale * resultPos;

    //Inverse child transform position, rotation, scale
    resultPos = SSubAll(resultPos, ToSVec3(child.mPos));

    SMat3 invChildRot = SQuat::ToMatrix(SQuat::Inversed(ToSQuat(child.mRot)));
    resultPos = invChildRot * resultPos;
    resultScaleRot = invChildRot * resultScaleRot;

    SMat3 invChildScale(SVec3::Reciprocal(ToSVec3(child.mScale)));
    resultPos = invChildScale * resultPos;
    resultScaleRot = invChildScale * resultScaleRot;
    return STransformer(resultScaleRot, resultPos);
  }
#else
  STransformer Transform::SGetModelToWorld() const;
  STransformer Transform::SGetModelToWorld(const Transform&) const { return STransformer(); }
  STransformer Transform::SGetWorldToModel() const { return STransformer(); }
  STransformer Transform::SGetWorldToModel(const Transform&) const { return STransformer(); }

  SVec3 Transform::SModelToWorld(const SVec3&) const { return SVec3(); }
  SVec3 Transform::SWorldToModel(const SVec3&) const { return SVec3(); }
#endif

  Vec3 Transformer::TransformPoint(const Vec3& point) const {
    if(gOptions.mSimdFlags & SyxOptions::SIMD::Transforms) {
      SAlign Vec3 result;
      SFloats sVec = ToSVec3(point);
      SMat3 scaleRot = ToSMat3(mScaleRot);

      sVec = scaleRot * sVec;
      sVec = SAddAll(sVec, ToSVec3(mPos));

      SVec3::Store(sVec, result);
      return result;
    }
    return mScaleRot*point + mPos;
  }

  Vec3 Transformer::TransformVector(const Vec3& vector) const {
    if(gOptions.mSimdFlags & SyxOptions::SIMD::Transforms) {
      SAlign Vec3 result;
      SFloats sVec = ToSVec3(vector);
      SMat3 scaleRot = ToSMat3(mScaleRot);
      sVec = scaleRot * sVec;
      SVec3::Store(sVec, result);
      return result;
    }
    return mScaleRot*vector;
  }

  STransformer Transformer::ToSIMDPoint() {
    STransformer result;
    result.mPos = ToSVec3(mPos);
    result.mScaleRot = ToSMat3(mScaleRot);
    return result;
  }

  STransformer Transformer::ToSIMDVector() {
    STransformer result;
    result.mScaleRot = ToSMat3(mScaleRot);
    return result;
  }

  Vec3 Transformer::GetScale() const {
    return Vec3(mScaleRot.mbx.Length(), mScaleRot.mby.Length(), mScaleRot.mbz.Length());
  }

  Transformer Transformer::Combined(const Transformer& first, const Transformer& second) {
    return Transformer(second.mScaleRot*first.mScaleRot, 
      second.mScaleRot*first.mPos + second.mPos);
  }

  Vec3 Transform::ModelToWorld(const Vec3& point) const {
    Vec3 result = point;
    result.Scale(mScale);
    result = mRot*result;
    result += mPos;
    return result;
  }

  Vec3 Transform::WorldToModel(const Vec3& point) const {
    Vec3 result = point;
    result -= mPos;
    result = mRot.Inversed()*result;
    result.Scale(mScale.Reciprocal());
    return result;
  }

  Transformer Transform::GetModelToWorld() const {
    return Transformer(mRot.ToMatrix()*Mat3(mScale), mPos);
  }

  Transformer Transform::GetModelToWorld(const Transform& child) const {
    //Child transform
    Mat3 resultScaleRot = child.mRot.ToMatrix()*Mat3(child.mScale);
    Vec3 resultPos = child.mPos;

    //Concatenate parent scale rotation and translation respectively
    Mat3 parentScale(mScale);
    resultScaleRot = parentScale * resultScaleRot;
    resultPos = parentScale * resultPos;

    Mat3 parentRot = mRot.ToMatrix();
    resultScaleRot = parentRot * resultScaleRot;
    resultPos = parentRot * resultPos;

    resultPos += mPos;
    return Transformer(resultScaleRot, resultPos);
  }

  Transformer Transform::GetWorldToModel() const {
    Mat3 invRot = mRot.Inversed().ToMatrix();
    Mat3 invScale = mScale.Reciprocal();

    //Calculate fourth column of invScale*invRot*invTranslate
    Vec3 resultPoint = -mPos;
    resultPoint = invRot * resultPoint;
    resultPoint = invScale * resultPoint;
    return Transformer(invScale*invRot, resultPoint);
  }

  Transformer Transform::GetWorldToModel(const Transform& child) const {
    //Inverse Parent transform, position, rotation, scale
    Mat3 resultScaleRot = mRot.Inversed().ToMatrix();
    Vec3 resultPos = -mPos;
    //Apply inverse parent rotation to inverse parent position
    resultPos = resultScaleRot * resultPos;

    //Apply inverse parent scale to parent position
    Mat3 invScale(mScale.Reciprocal());
    resultScaleRot = invScale*resultScaleRot;
    resultPos = invScale*resultPos;

    //Inverse child transform position, rotation, scale
    resultPos -= child.mPos;

    Mat3 invChildRot = child.mRot.Inversed().ToMatrix();
    resultPos = invChildRot * resultPos;
    resultScaleRot = invChildRot * resultScaleRot;

    Mat3 invChildScale(child.mScale.Reciprocal());
    resultPos = invChildScale * resultPos;
    resultScaleRot = invChildScale * resultScaleRot;
    return Transformer(resultScaleRot, resultPos);
  }
}