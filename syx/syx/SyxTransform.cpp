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
    result = SMulAll(result, ToSVector3(mScale));
    result = SQuat::Rotate(ToSQuat(mRot), result);
    result = SAddAll(result, ToSVector3(mPos));
    return result;
  }

  SFloats Transform::SWorldToModel(SFloats point) const {
    SFloats result = point;
    result = SSubAll(result, ToSVector3(mPos));
    result = SQuat::Rotate(SQuat::Inversed(ToSQuat(mRot)), result);
    result = SMulAll(result, SVector3::Reciprocal(ToSVector3(mScale)));
    return result;
  }

  STransformer Transform::SGetModelToWorld() const {
    return STransformer(SQuat::ToMatrix(ToSQuat(mRot))*SMatrix3(ToSVector3(mScale)), ToSVector3(mPos));
  }

  STransformer Transform::SGetModelToWorld(const Transform& child) const {
    //Child transform
    SMatrix3 resultScaleRot = SQuat::ToMatrix(ToSQuat(child.mRot))*SMatrix3(ToSVector3(child.mScale));
    SFloats resultPos = ToSVector3(child.mPos);

    //Concatenate parent scale rotation and translation respectively
    SMatrix3 parentScale(ToSVector3(mScale));
    resultScaleRot = parentScale*resultScaleRot;
    resultPos = parentScale*resultPos;

    SMatrix3 parentRot = SQuat::ToMatrix(ToSQuat(mRot));
    resultScaleRot = parentRot*resultScaleRot;
    resultPos = parentRot*resultPos;

    resultPos = SAddAll(resultPos, ToSVector3(mPos));
    return STransformer(resultScaleRot, resultPos);
  }

  STransformer Transform::SGetWorldToModel() const {
    SMatrix3 invRot = SQuat::ToMatrix(SQuat::Inversed(ToSQuat(mRot)));
    SMatrix3 invScale = SVector3::Reciprocal(ToSVector3(mScale));

    //Calculate fourth column of invScale*invRot*invTranslate
    SFloats resultPoint = SVector3::Neg(ToSVector3(mPos));
    resultPoint = invRot * resultPoint;
    resultPoint = invScale * resultPoint;
    return STransformer(invScale*invRot, resultPoint);
  }

  STransformer Transform::SGetWorldToModel(const Transform& child) const {
    //Inverse Parent transform, position, rotation, scale
    SMatrix3 resultScaleRot = SQuat::ToMatrix(SQuat::Inversed(ToSQuat(mRot)));
    SFloats resultPos = SVector3::Neg(ToSVector3(mPos));
    //Apply inverse parent rotation to inverse parent position
    resultPos = resultScaleRot * resultPos;

    //Apply inverse parent scale to parent position
    SMatrix3 invScale(SVector3::Reciprocal(ToSVector3(mScale)));
    resultScaleRot = invScale * resultScaleRot;
    resultPos = invScale * resultPos;

    //Inverse child transform position, rotation, scale
    resultPos = SSubAll(resultPos, ToSVector3(child.mPos));

    SMatrix3 invChildRot = SQuat::ToMatrix(SQuat::Inversed(ToSQuat(child.mRot)));
    resultPos = invChildRot * resultPos;
    resultScaleRot = invChildRot * resultScaleRot;

    SMatrix3 invChildScale(SVector3::Reciprocal(ToSVector3(child.mScale)));
    resultPos = invChildScale * resultPos;
    resultScaleRot = invChildScale * resultScaleRot;
    return STransformer(resultScaleRot, resultPos);
  }
#else
  STransformer Transform::SGetModelToWorld() const;
  STransformer Transform::SGetModelToWorld(const Transform&) const { return STransformer(); }
  STransformer Transform::SGetWorldToModel() const { return STransformer(); }
  STransformer Transform::SGetWorldToModel(const Transform&) const { return STransformer(); }

  SVector3 Transform::SModelToWorld(const SVector3&) const { return SVector3(); }
  SVector3 Transform::SWorldToModel(const SVector3&) const { return SVector3(); }
#endif

  Vector3 Transformer::TransformPoint(const Vector3& point) const {
    if(gOptions.mSimdFlags & SyxOptions::SIMD::Transforms) {
      SAlign Vector3 result;
      SFloats sVec = ToSVector3(point);
      SMatrix3 scaleRot = ToSMatrix3(mScaleRot);

      sVec = scaleRot * sVec;
      sVec = SAddAll(sVec, ToSVector3(mPos));

      SVector3::Store(sVec, result);
      return result;
    }
    return mScaleRot*point + mPos;
  }

  Vector3 Transformer::TransformVector(const Vector3& vector) const {
    if(gOptions.mSimdFlags & SyxOptions::SIMD::Transforms) {
      SAlign Vector3 result;
      SFloats sVec = ToSVector3(vector);
      SMatrix3 scaleRot = ToSMatrix3(mScaleRot);
      sVec = scaleRot * sVec;
      SVector3::Store(sVec, result);
      return result;
    }
    return mScaleRot*vector;
  }

  STransformer Transformer::ToSIMDPoint() {
    STransformer result;
    result.mPos = ToSVector3(mPos);
    result.mScaleRot = ToSMatrix3(mScaleRot);
    return result;
  }

  STransformer Transformer::ToSIMDVector() {
    STransformer result;
    result.mScaleRot = ToSMatrix3(mScaleRot);
    return result;
  }

  Vector3 Transformer::GetScale() const {
    return Vector3(mScaleRot.mbx.Length(), mScaleRot.mby.Length(), mScaleRot.mbz.Length());
  }

  Transformer Transformer::Combined(const Transformer& first, const Transformer& second) {
    return Transformer(second.mScaleRot*first.mScaleRot, 
      second.mScaleRot*first.mPos + second.mPos);
  }

  Vector3 Transform::ModelToWorld(const Vector3& point) const {
    Vector3 result = point;
    result.Scale(mScale);
    result = mRot*result;
    result += mPos;
    return result;
  }

  Vector3 Transform::WorldToModel(const Vector3& point) const {
    Vector3 result = point;
    result -= mPos;
    result = mRot.Inversed()*result;
    result.Scale(mScale.Reciprocal());
    return result;
  }

  Transformer Transform::GetModelToWorld() const {
    return Transformer(mRot.ToMatrix()*Matrix3(mScale), mPos);
  }

  Transformer Transform::GetModelToWorld(const Transform& child) const {
    //Child transform
    Matrix3 resultScaleRot = child.mRot.ToMatrix()*Matrix3(child.mScale);
    Vector3 resultPos = child.mPos;

    //Concatenate parent scale rotation and translation respectively
    Matrix3 parentScale(mScale);
    resultScaleRot = parentScale * resultScaleRot;
    resultPos = parentScale * resultPos;

    Matrix3 parentRot = mRot.ToMatrix();
    resultScaleRot = parentRot * resultScaleRot;
    resultPos = parentRot * resultPos;

    resultPos += mPos;
    return Transformer(resultScaleRot, resultPos);
  }

  Transformer Transform::GetWorldToModel() const {
    Matrix3 invRot = mRot.Inversed().ToMatrix();
    Matrix3 invScale = mScale.Reciprocal();

    //Calculate fourth column of invScale*invRot*invTranslate
    Vector3 resultPoint = -mPos;
    resultPoint = invRot * resultPoint;
    resultPoint = invScale * resultPoint;
    return Transformer(invScale*invRot, resultPoint);
  }

  Transformer Transform::GetWorldToModel(const Transform& child) const {
    //Inverse Parent transform, position, rotation, scale
    Matrix3 resultScaleRot = mRot.Inversed().ToMatrix();
    Vector3 resultPos = -mPos;
    //Apply inverse parent rotation to inverse parent position
    resultPos = resultScaleRot * resultPos;

    //Apply inverse parent scale to parent position
    Matrix3 invScale(mScale.Reciprocal());
    resultScaleRot = invScale*resultScaleRot;
    resultPos = invScale*resultPos;

    //Inverse child transform position, rotation, scale
    resultPos -= child.mPos;

    Matrix3 invChildRot = child.mRot.Inversed().ToMatrix();
    resultPos = invChildRot * resultPos;
    resultScaleRot = invChildRot * resultScaleRot;

    Matrix3 invChildScale(child.mScale.Reciprocal());
    resultPos = invChildScale * resultPos;
    resultScaleRot = invChildScale * resultScaleRot;
    return Transformer(resultScaleRot, resultPos);
  }
}