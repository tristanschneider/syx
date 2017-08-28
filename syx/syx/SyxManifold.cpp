#include "Precompile.h"
#include "SyxManifold.h"
#include "SyxPhysicsObject.h"

namespace Syx {
  ContactPoint::ContactPoint(const ContactObject& a, const ContactObject& b, const Vec3& normal):
    mObjA(a), mObjB(b), mPenetration((b.mStartingWorld - a.mStartingWorld).dot(normal)), mWarmContact(0.0f), mWarmFriction{0.0f} {
  }

  void ContactPoint::draw(PhysicsObject* a, PhysicsObject* b, const Vec3& normal) const {
    DebugDrawer& d = DebugDrawer::get();
    Vec3 ca = a->getTransform().modelToWorld(mObjA.mModelPoint);
    Vec3 cb = b->getTransform().modelToWorld(mObjB.mModelPoint);

    d.setColor(1.0f, 0.0f, 0.0f);
    drawCube(ca, 0.1f);
    d.drawPoint(mObjA.mStartingWorld, 0.1f);
    d.drawVector(ca, normal);

    d.setColor(0.0f, 0.0f, 1.0f);
    drawCube(cb, 0.1f);
    d.drawPoint(mObjB.mStartingWorld, 0.1f);
    d.drawVector(cb, -normal);
  }

  void ContactPoint::replace(const ContactPoint& c) {
    mObjA = c.mObjA;
    mObjB = c.mObjB;
    mPenetration = c.mPenetration;
  }

  float Manifold::sNormalTolerance = 0.03f;
  float Manifold::sTangentTolerance = 0.05f;
  float Manifold::sMatchTolerance = 0.01f;
  float Manifold::sNormalMatchTolerance = 0.01f;

  void Manifold::_replaceNormal(const Vec3& newNormal) {
    mNormal = newNormal;
    //Normal is only replaced if it is sufficiently different, so no need to try to make friction axes similar
    mNormal.getBasis(mTangentA, mTangentB);
  }

  void Manifold::_matchNormal(const Vec3& newNormal) {
    float dot = mNormal.dot(newNormal);
    if(1.0f - dot > sNormalMatchTolerance) {
      _replaceNormal(newNormal);
      //Since the normal changed, penetration values need to be updated
      update();
    }
  }

  void Manifold::addContact(const ContactPoint& contact, const Vec3& normal) {
    if(Interface::getOptions().mDebugFlags & SyxOptions::DrawNewContact)
      drawCube(contact.mObjA.mStartingWorld, 0.1f);

    if(!mSize) {
      _pushContact(contact);
      _replaceNormal(normal);
      return;
    }

    _matchNormal(normal);

    //Look for contacts that could be replaced (within thresholds)
    for(size_t i = 0; i < mSize; ++i) {
      ContactPoint& p = mContacts[i];
      //They'll either both be within or not, so I don't need to check both a and b. Arbitrarily check a
      //Using world or model space for this test has its tradeoffs. World space will give more consistent results
      if(p.mObjA.mStartingWorld.distance2(contact.mObjA.mStartingWorld) < sMatchTolerance) {
        mContacts[i].replace(contact);
        return;
      }
    }

    if(mSize < MAX_CONTACTS)
      _pushContact(contact);
    else
      _addToFull(contact);
  }

  void Manifold::_addToFull(const ContactPoint& contact) {
    //Put everything in here for convenient iteration
    const ContactPoint* points[5] = {&mContacts[0], &mContacts[1], &mContacts[2], &mContacts[3], &contact};

    //Get furthest pair of points
    std::pair<int, int> bestPair;
    float bestDist = 0.0f;
    for(int i = 0; i < 5; ++i)
      for(int j = i + 1; j < 5; ++j) {
        float curDist = points[i]->mObjA.mStartingWorld.distance2(points[j]->mObjA.mStartingWorld);
        if(curDist > bestDist) {
          bestDist = curDist;
          bestPair = {i, j};
        }
      }

    //Make these furthest points the first two elements
    std::swap(points[0], points[bestPair.first]);
    std::swap(points[1], points[bestPair.second]);

    //Find third point that maximizes triangle area
    Vec3 lineStart = points[0]->mObjA.mStartingWorld;
    Vec3 line = points[1]->mObjA.mStartingWorld - lineStart;
    float bestArea = 0.0f;
    int bestThird = 0;
    for(int i = 2; i < 5; ++i) {
      float curArea = (points[i]->mObjA.mStartingWorld - lineStart).cross(line).length2();
      if(curArea > bestArea) {
        bestArea = curArea;
        bestThird = i;
      }
    }

    //Bring best third point to the third slot
    std::swap(points[2], points[bestThird]);

    //Find furthest point from triangle
    Vec3 pA, pB, pC;
    getOutwardTriPlanes(points[0]->mObjA.mStartingWorld, points[1]->mObjA.mStartingWorld, points[2]->mObjA.mStartingWorld, pA, pB, pC, true);
    //Get distance from each plane for index 3
    float dA = pA.dot4(points[3]->mObjA.mStartingWorld);
    float dB = pB.dot4(points[3]->mObjA.mStartingWorld);
    float dC = pC.dot4(points[3]->mObjA.mStartingWorld);
    float distA = std::max(dA, std::max(dB, dC));

    //Get distance from each plane for index 4
    dA = pA.dot4(points[4]->mObjA.mStartingWorld);
    dB = pB.dot4(points[4]->mObjA.mStartingWorld);
    dC = pC.dot4(points[4]->mObjA.mStartingWorld);
    float distB = std::max(dA, std::max(dB, dC));

    //Put the further point in slot 4
    float fourthDist = distA;
    if(distB > distA) {
      std::swap(points[3], points[4]);
      fourthDist = distB;
    }

    //Copy points to a temporary buffer so when we copy them to the destination we don't ruin things
    ContactPoint temp[4] = {*points[0], *points[1], *points[2], *points[3]};

    //Don't include the fourth point if it's within the triangle formed by the first three, as then it would be redundant
    int endIndex = fourthDist > 0.0f ? 4 : 3;
    //Finally, store the resulting four points in our manifold
    for(int i = 0; i < endIndex; ++i)
      mContacts[i] = temp[i];
  }

  void Manifold::update(void) {
    Transformer tA = mA->getOwner()->getTransform().getModelToWorld();
    Transformer tB = mB->getOwner()->getTransform().getModelToWorld();

    for(size_t i = 0; i < mSize;) {
      ContactPoint& p = mContacts[i];
      Vec3 aWorld = tA.transformPoint(p.mObjA.mModelPoint);

      //If either has drifted out of the tolerance range, remove it
      Vec3 aDrift = aWorld - p.mObjA.mStartingWorld;
      if(std::abs(aDrift.dot(mNormal)) > sNormalTolerance ||
        std::abs(aDrift.dot(mTangentA)) > sTangentTolerance ||
        std::abs(aDrift.dot(mTangentB)) > sTangentTolerance) {
        _removeContact(i);
        continue;
      }

      Vec3 bWorld = tB.transformPoint(p.mObjB.mModelPoint);
      Vec3 bDrift = bWorld - p.mObjB.mStartingWorld;
      if(std::abs(bDrift.dot(mNormal)) > sNormalTolerance ||
        std::abs(bDrift.dot(mTangentA)) > sTangentTolerance ||
        std::abs(bDrift.dot(mTangentB)) > sTangentTolerance) {
        _removeContact(i);
        continue;
      }

      //If we got here, this contact is still good, update penetration and world values
      p.mPenetration = (bWorld - aWorld).dot(mNormal);
      p.mObjA.mCurrentWorld = aWorld;
      p.mObjB.mCurrentWorld = bWorld;
      ++i;
    }
  }

  void Manifold::_pushContact(const ContactPoint& contact) {
    SyxAssertError(mSize < MAX_CONTACTS);
    mContacts[mSize++] = contact;
  }

  void Manifold::_removeContact(size_t index) {
    SyxAssertError(mSize > 0);
    //Swap remove
    if(mSize > 1)
      mContacts[index] = mContacts[mSize - 1];
    --mSize;
  }

  void Manifold::draw(void) {
    DebugDrawer& d = DebugDrawer::get();
    Vec3 center = Vec3::Zero;
    for(size_t i = 0; i < mSize; ++i) {
      mContacts[i].draw(mA->getOwner(), mB->getOwner(), mNormal);
      center += mContacts[i].mObjA.mCurrentWorld;
    }

    if(mSize) {
      center /= static_cast<float>(mSize);
      d.setColor(1.0f, 0.0f, 0.0f);
      d.drawVector(center, mNormal);
      d.setColor(0.0f, 1.0f, 0.0f);
      d.drawVector(center, mTangentA);
      d.setColor(0.0f, 0.0f, 1.0f);
      d.drawVector(center, mTangentB);
    }
  }
}