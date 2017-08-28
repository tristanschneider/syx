#pragma once
#include "Precompile.h"

namespace Syx {

#ifdef SENABLED
  struct STransformer {
    STransformer(void) {}
    STransformer(const SMat3& scaleRot, SFloats pos): mScaleRot(scaleRot), mPos(pos) {}

    SFloats transformPoint(SFloats point);
    SFloats transformVector(SFloats vector);

    SMat3 mScaleRot;
    SFloats mPos;
  };
#endif

  SAlign struct Transformer {
    Transformer(void) {}
    Transformer(const Mat3& scaleRot, const Vec3& pos): mScaleRot(scaleRot), mPos(pos) {}

    // Returns a transformer that will result in transforming by first then second
    static Transformer combined(const Transformer& first, const Transformer& second);

    Vec3 transformPoint(const Vec3& point) const;
    Vec3 transformVector(const Vec3& vector) const;
    //Get the SIMD version for point or for just vectors, meaning the vecor version won't bother loading position
    STransformer toSIMDPoint();
    STransformer toSIMDVector();
    Vec3 getScale() const;

    SAlign Mat3 mScaleRot;
    SAlign Vec3 mPos;
  };

  struct Transform {
    Transform(const Vec3& scale, const Quat& rot, const Vec3& pos): mScale(scale), mRot(rot), mPos(pos) {}
    Transform(void): mPos(Vec3::Zero), mScale(Vec3::Identity), mRot(Quat::Identity) {}

    //For one offs, use these
    Vec3 modelToWorld(const Vec3& point) const;
    Vec3 worldToModel(const Vec3& point) const;

    //For batch translations, get one of these and use it for a bunch of points
    Transformer getModelToWorld() const;
    Transformer getModelToWorld(const Transform& child) const;
    Transformer getWorldToModel() const;
    Transformer getWorldToModel(const Transform& child) const;

#ifdef SENABLED
    STransformer sgetModelToWorld() const;
    STransformer sGetModelToWorld(const Transform& child) const;
    STransformer sGetworldToModel() const;
    STransformer sGetworldToModel(const Transform& child) const;

    SFloats sModelToWorld(SFloats point) const;
    SFloats sworldToModel(SFloats point) const;
#endif

    Vec3 mScale;
    Quat mRot;
    Vec3 mPos;
  };
}