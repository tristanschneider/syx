#include "Precompile.h"
#include "SyxManifold.h"
#include "SyxPhysicsObject.h"

namespace Syx {
  ContactPoint::ContactPoint(const ContactObject& a, const ContactObject& b, const Vector3& normal):
    mObjA(a), mObjB(b), mPenetration((b.mStartingWorld - a.mStartingWorld).Dot(normal)), mWarmContact(0.0f), mWarmFriction{0.0f} {
  }

  void ContactPoint::Draw(PhysicsObject* a, PhysicsObject* b, const Vector3& normal) const {
    DebugDrawer& d = DebugDrawer::Get();
    Vector3 ca = a->GetTransform().ModelToWorld(mObjA.mModelPoint);
    Vector3 cb = b->GetTransform().ModelToWorld(mObjB.mModelPoint);

    d.SetColor(1.0f, 0.0f, 0.0f);
    DrawCube(ca, 0.1f);
    d.DrawPoint(mObjA.mStartingWorld, 0.1f);
    d.DrawVector(ca, normal);

    d.SetColor(0.0f, 0.0f, 1.0f);
    DrawCube(cb, 0.1f);
    d.DrawPoint(mObjB.mStartingWorld, 0.1f);
    d.DrawVector(cb, -normal);
  }

  void ContactPoint::Replace(const ContactPoint& c) {
    mObjA = c.mObjA;
    mObjB = c.mObjB;
    mPenetration = c.mPenetration;
  }

  float Manifold::sNormalTolerance = 0.03f;
  float Manifold::sTangentTolerance = 0.05f;
  float Manifold::sMatchTolerance = 0.01f;
  float Manifold::sNormalMatchTolerance = 0.01f;

  void Manifold::ReplaceNormal(const Vector3& newNormal) {
    mNormal = newNormal;
    //Normal is only replaced if it is sufficiently different, so no need to try to make friction axes similar
    mNormal.GetBasis(mTangentA, mTangentB);
  }

  void Manifold::MatchNormal(const Vector3& newNormal) {
    float dot = mNormal.Dot(newNormal);
    if(1.0f - dot > sNormalMatchTolerance) {
      ReplaceNormal(newNormal);
      //Since the normal changed, penetration values need to be updated
      Update();
    }
  }

  void Manifold::AddContact(const ContactPoint& contact, const Vector3& normal) {
    if(Interface::GetOptions().mDebugFlags & SyxOptions::DrawNewContact)
      DrawCube(contact.mObjA.mStartingWorld, 0.1f);

    if(!mSize) {
      PushContact(contact);
      ReplaceNormal(normal);
      return;
    }

    MatchNormal(normal);

    //Look for contacts that could be replaced (within thresholds)
    for(size_t i = 0; i < mSize; ++i) {
      ContactPoint& p = mContacts[i];
      //They'll either both be within or not, so I don't need to check both a and b. Arbitrarily check a
      //Using world or model space for this test has its tradeoffs. World space will give more consistent results
      if(p.mObjA.mStartingWorld.Distance2(contact.mObjA.mStartingWorld) < sMatchTolerance) {
        mContacts[i].Replace(contact);
        return;
      }
    }

    if(mSize < MAX_CONTACTS)
      PushContact(contact);
    else
      AddToFull(contact);
  }

  void Manifold::AddToFull(const ContactPoint& contact) {
    //Put everything in here for convenient iteration
    const ContactPoint* points[5] = {&mContacts[0], &mContacts[1], &mContacts[2], &mContacts[3], &contact};

    //Get furthest pair of points
    std::pair<int, int> bestPair;
    float bestDist = 0.0f;
    for(int i = 0; i < 5; ++i)
      for(int j = i + 1; j < 5; ++j) {
        float curDist = points[i]->mObjA.mStartingWorld.Distance2(points[j]->mObjA.mStartingWorld);
        if(curDist > bestDist) {
          bestDist = curDist;
          bestPair = {i, j};
        }
      }

    //Make these furthest points the first two elements
    std::swap(points[0], points[bestPair.first]);
    std::swap(points[1], points[bestPair.second]);

    //Find third point that maximizes triangle area
    Vector3 lineStart = points[0]->mObjA.mStartingWorld;
    Vector3 line = points[1]->mObjA.mStartingWorld - lineStart;
    float bestArea = 0.0f;
    int bestThird = 0;
    for(int i = 2; i < 5; ++i) {
      float curArea = (points[i]->mObjA.mStartingWorld - lineStart).Cross(line).Length2();
      if(curArea > bestArea) {
        bestArea = curArea;
        bestThird = i;
      }
    }

    //Bring best third point to the third slot
    std::swap(points[2], points[bestThird]);

    //Find furthest point from triangle
    Vector3 pA, pB, pC;
    GetOutwardTriPlanes(points[0]->mObjA.mStartingWorld, points[1]->mObjA.mStartingWorld, points[2]->mObjA.mStartingWorld, pA, pB, pC, true);
    //Get distance from each plane for index 3
    float dA = pA.Dot4(points[3]->mObjA.mStartingWorld);
    float dB = pB.Dot4(points[3]->mObjA.mStartingWorld);
    float dC = pC.Dot4(points[3]->mObjA.mStartingWorld);
    float distA = std::max(dA, std::max(dB, dC));

    //Get distance from each plane for index 4
    dA = pA.Dot4(points[4]->mObjA.mStartingWorld);
    dB = pB.Dot4(points[4]->mObjA.mStartingWorld);
    dC = pC.Dot4(points[4]->mObjA.mStartingWorld);
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

  void Manifold::Update(void) {
    Transformer tA = mA->GetOwner()->GetTransform().GetModelToWorld();
    Transformer tB = mB->GetOwner()->GetTransform().GetModelToWorld();

    for(size_t i = 0; i < mSize;) {
      ContactPoint& p = mContacts[i];
      Vector3 aWorld = tA.TransformPoint(p.mObjA.mModelPoint);

      //If either has drifted out of the tolerance range, remove it
      Vector3 aDrift = aWorld - p.mObjA.mStartingWorld;
      if(std::abs(aDrift.Dot(mNormal)) > sNormalTolerance ||
        std::abs(aDrift.Dot(mTangentA)) > sTangentTolerance ||
        std::abs(aDrift.Dot(mTangentB)) > sTangentTolerance) {
        RemoveContact(i);
        continue;
      }

      Vector3 bWorld = tB.TransformPoint(p.mObjB.mModelPoint);
      Vector3 bDrift = bWorld - p.mObjB.mStartingWorld;
      if(std::abs(bDrift.Dot(mNormal)) > sNormalTolerance ||
        std::abs(bDrift.Dot(mTangentA)) > sTangentTolerance ||
        std::abs(bDrift.Dot(mTangentB)) > sTangentTolerance) {
        RemoveContact(i);
        continue;
      }

      //If we got here, this contact is still good, update penetration and world values
      p.mPenetration = (bWorld - aWorld).Dot(mNormal);
      p.mObjA.mCurrentWorld = aWorld;
      p.mObjB.mCurrentWorld = bWorld;
      ++i;
    }
  }

  void Manifold::PushContact(const ContactPoint& contact) {
    SyxAssertError(mSize < MAX_CONTACTS);
    mContacts[mSize++] = contact;
  }

  void Manifold::RemoveContact(size_t index) {
    SyxAssertError(mSize > 0);
    //Swap remove
    if(mSize > 1)
      mContacts[index] = mContacts[mSize - 1];
    --mSize;
  }

  void Manifold::Draw(void) {
    DebugDrawer& d = DebugDrawer::Get();
    Vector3 center = Vector3::Zero;
    for(size_t i = 0; i < mSize; ++i) {
      mContacts[i].Draw(mA->GetOwner(), mB->GetOwner(), mNormal);
      center += mContacts[i].mObjA.mCurrentWorld;
    }

    if(mSize) {
      center /= static_cast<float>(mSize);
      d.SetColor(1.0f, 0.0f, 0.0f);
      d.DrawVector(center, mNormal);
      d.SetColor(0.0f, 1.0f, 0.0f);
      d.DrawVector(center, mTangentA);
      d.SetColor(0.0f, 0.0f, 1.0f);
      d.DrawVector(center, mTangentB);
    }
  }
}