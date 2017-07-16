#pragma once
#include "Precompile.h"

namespace Syx {

#ifdef SENABLED
  struct STransformer {
    STransformer(void) {}
    STransformer(const SMat3& scaleRot, SFloats pos): mScaleRot(scaleRot), mPos(pos) {}

    SFloats TransformPoint(SFloats point);
    SFloats TransformVector(SFloats vector);

    SMat3 mScaleRot;
    SFloats mPos;
  };
#endif

  SAlign struct Transformer {
    Transformer(void) {}
    Transformer(const Mat3& scaleRot, const Vec3& pos): mScaleRot(scaleRot), mPos(pos) {}

    // Returns a transformer that will result in transforming by first then second
    static Transformer Combined(const Transformer& first, const Transformer& second);

    Vec3 TransformPoint(const Vec3& point) const;
    Vec3 TransformVector(const Vec3& vector) const;
    //Get the SIMD version for point or for just vectors, meaning the vecor version won't bother loading position
    STransformer ToSIMDPoint();
    STransformer ToSIMDVector();
    Vec3 GetScale() const;

    SAlign Mat3 mScaleRot;
    SAlign Vec3 mPos;
  };

  struct Transform {
    Transform(const Vec3& scale, const Quat& rot, const Vec3& pos): mScale(scale), mRot(rot), mPos(pos) {}
    Transform(void): mPos(Vec3::Zero), mScale(Vec3::Identity), mRot(Quat::Identity) {}

    //For one offs, use these
    Vec3 ModelToWorld(const Vec3& point) const;
    Vec3 WorldToModel(const Vec3& point) const;

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

    Vec3 mScale;
    Quat mRot;
    Vec3 mPos;
  };
}