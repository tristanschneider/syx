#pragma once
#include "Precompile.h"

namespace Syx {

#ifdef SENABLED
  struct STransformer {
    STransformer(void) {}
    STransformer(const SMatrix3& scaleRot, SFloats pos): mScaleRot(scaleRot), mPos(pos) {}

    SFloats TransformPoint(SFloats point);
    SFloats TransformVector(SFloats vector);

    SMatrix3 mScaleRot;
    SFloats mPos;
  };
#endif

  SAlign struct Transformer {
    Transformer(void) {}
    Transformer(const Matrix3& scaleRot, const Vector3& pos): mScaleRot(scaleRot), mPos(pos) {}

    // Returns a transformer that will result in transforming by first then second
    static Transformer Combined(const Transformer& first, const Transformer& second);

    Vector3 TransformPoint(const Vector3& point) const;
    Vector3 TransformVector(const Vector3& vector) const;
    //Get the SIMD version for point or for just vectors, meaning the vecor version won't bother loading position
    STransformer ToSIMDPoint();
    STransformer ToSIMDVector();
    Vector3 GetScale() const;

    SAlign Matrix3 mScaleRot;
    SAlign Vector3 mPos;
  };

  struct Transform {
    Transform(const Vector3& scale, const Quat& rot, const Vector3& pos): mScale(scale), mRot(rot), mPos(pos) {}
    Transform(void): mPos(Vector3::Zero), mScale(Vector3::Identity), mRot(Quat::Identity) {}

    //For one offs, use these
    Vector3 ModelToWorld(const Vector3& point) const;
    Vector3 WorldToModel(const Vector3& point) const;

    //For batch translations, get one of these and use it for a bunch of points
    Transformer GetModelToWorld() const;
    Transformer GetModelToWorld(const Transform& child) const;
    Transformer GetWorldToModel() const;
    Transformer GetWorldToModel(const Transform& child) const;

#ifdef SENABLED
    STransformer SGetModelToWorld() const;
    STransformer SGetModelToWorld(const Transform& child) const;
    STransformer SGetWorldToModel() const;
    STransformer SGetWorldToModel(const Transform& child) const;

    SFloats SModelToWorld(SFloats point) const;
    SFloats SWorldToModel(SFloats point) const;
#endif

    Vector3 mScale;
    Quat mRot;
    Vector3 mPos;
  };
}