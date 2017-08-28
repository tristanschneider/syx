#pragma once

#define MAX_CONTACTS 4

namespace Syx {
  class Collider;
  class PhysicsObject;

  SAlign struct ContactObject {
    ContactObject() {}
    ContactObject(const Vec3& modelPoint, const Vec3& startingWorld)
      : mModelPoint(modelPoint)
      , mStartingWorld(startingWorld)
      , mCurrentWorld(startingWorld) {
    }

    SAlign Vec3 mModelPoint;
    SAlign Vec3 mStartingWorld;
    SAlign Vec3 mCurrentWorld;
  };

  SAlign struct ContactPoint {
    ContactPoint() {}
    ContactPoint(const ContactObject& a, const ContactObject& b, float penetration)
      : mObjA(a)
      , mObjB(b)
      , mPenetration(penetration)
      , mWarmContact(0.0f)
      , mWarmFriction{0.0f} {
    }
    ContactPoint(const ContactObject& a, const ContactObject& b, const Vec3& normal);

    void draw(PhysicsObject* a, PhysicsObject* b, const Vec3& normal) const;
    //Replace this with given contact but keep warm starts
    void replace(const ContactPoint& c);

    SAlign ContactObject mObjA;
    SAlign ContactObject mObjB;
    float mPenetration;
    float mWarmContact;
    float mWarmFriction[2];
  };

  SAlign class Manifold {
  public:
    Manifold()
      : mA(nullptr)
      , mB(nullptr)
      , mSize(0) {
    }
    Manifold(Collider* a, Collider* b)
      : mA(a)
      , mB(b)
      , mSize(0) {
    }

    void addContact(const ContactPoint& contact, const Vec3& normal);
    //Update penetration info and discard invalid points
    void update();
    void draw();

    SAlign ContactPoint mContacts[MAX_CONTACTS];
    SAlign Vec3 mNormal;
    SAlign Vec3 mTangentA;
    SAlign Vec3 mTangentB;
    Collider* mA;
    Collider* mB;
    size_t mSize;
    SPadClass(sizeof(ContactPoint)*MAX_CONTACTS + sizeof(Vec3)*3 + SPtrSize*2 + sizeof(size_t));

  private:
    void _pushContact(const ContactPoint& contact);
    void _removeContact(size_t index);
    //Attempts to match the new normal with the old. If the difference is too big, the old one is replaced
    void _matchNormal(const Vec3& newNormal);
    //Replaces normals and sets tangents
    void _replaceNormal(const Vec3& newNormal);
    //Finds the best 4 points of the full manifold + contact and discards the other point
    void _addToFull(const ContactPoint& contact);

    //Distance tolerance along given axis until contact is invalidated
    static float sNormalTolerance;
    static float sTangentTolerance;
    //Distance under which a new point will replace the existing one
    static float sMatchTolerance;
    //Deviation of dot product from 1 before new normal replaces old one
    static float sNormalMatchTolerance;
  };
}