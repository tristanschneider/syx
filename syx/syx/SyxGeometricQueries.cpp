#include "Precompile.h"

namespace Syx {
  //http://www.gamedev.net/topic/552906-closest-point-on-triangle/
  Vec3 closestOnTriFromEdges(const Vec3& triA, const Vec3& pToA, const Vec3& aToB, const Vec3& aToC, float* resultABT, float* resultACT, bool* clamped) {
    double a = static_cast<double>(aToB.length2());
    double b = static_cast<double>(aToB.dot(aToC));
    double c = static_cast<double>(aToC.length2());
    double d = static_cast<double>(aToB.dot(pToA));
    double e = static_cast<double>(aToC.dot(pToA));
    double det = static_cast<double>(a)*static_cast<double>(c) - static_cast<double>(b)*static_cast<double>(b);
    double invDet = 1.0/det;

    double v = static_cast<double>(b)*static_cast<double>(e) - static_cast<double>(c)*static_cast<double>(d);
    double u = static_cast<double>(b)*static_cast<double>(d) - static_cast<double>(a)*static_cast<double>(e);

    double s = v;
    double t = u;
    bool clampRef = true;

    if(s + t <= det) {
      if(s < 0.0) {
        if(t < 0.0) {
          if(d < 0.0) {
            s = clamp(-d/a, 0.0, 1.0);
            t = 0.0;
          }
          else {
            s = 0.0;
            t = clamp(-e/c, 0.0, 1.0);
          }
        }
        else {
          s = 0.0;
          t = clamp(-e/c, 0.0, 1.0);
        }
      }
      else if(t < 0.0) {
        s = clamp(-d/a, 0.0, 1.0);
        t = 0.0;
      }
      else {
        s *= invDet;
        t *= invDet;
        clampRef = false;
      }
    }
    else {
      if(s < 0.0) {
        double tmp0 = b + d;
        double tmp1 = c + e;
        if(tmp1 > tmp0) {
          double numer = tmp1 - tmp0;
          double denom = a - 2.0*b + c;
          s = clamp(numer/denom, 0.0, 1.0);
          t = 1.0 - s;
        }
        else {
          t = clamp(-e/c, 0.0, 1.0);
          s = 0.0;
        }
      }
      else if(t < 0.0) {
        if(a + d > b + e) {
          double numer = c + e - b - d;
          double denom = a - 2.0*b + c;
          s = clamp(numer/denom, 0.0, 1.0);
          t = 1.0 - s;
        }
        else {
          s = clamp(-e/c, 0.0, 1.0);
          t = 0.0;
        }
      }
      else {
        double numer = c + e - b - d;
        double denom = a - 2.0*b + c;
        s = clamp(numer/denom, 0.0, 1.0);
        t = 1.0 - s;
      }
    }

    if(resultABT)
      *resultABT = static_cast<float>(s);
    if(resultACT)
      *resultACT = static_cast<float>(t);
    if(clamped)
      *clamped = clampRef;
    return triA + static_cast<float>(s)*aToB + static_cast<float>(t)*aToC;
  }

  Vec3 closestOnTri(const Vec3& p, const Vec3& triA, const Vec3& triB, const Vec3& triC, float* resultABT, float* resultACT, bool* clamped) {
    Vec3 pToA = triA - p;
    Vec3 aToB = triB - triA;
    Vec3 aToC = triC - triA;
    return closestOnTriFromEdges(triA, pToA, aToB, aToC, resultABT, resultACT, clamped);
  }

  Vec3 closestOnRay(const Vec3& p, const Vec3& a, const Vec3& b) {
    return a + Vec3::projVec(p - a, b - a);
  }

  Vec3 closestOnLine(const Vec3& p, const Vec3& a, const Vec3& b) {
    Vec3 aToB = b - a;
    float proj = Vec3::projVecScalar(p - a, aToB);
    proj = clamp(proj, 0.0f, 1.0f);
    return a + aToB*proj;
  }

  float pointRayDist2(const Vec3& p, const Vec3& a, const Vec3& b) {
    Vec3 aToP = p - a;
    Vec3 proj = Vec3::projVec(aToP, b - a);
    return aToP.distance2(proj);
  }

  float pointLineDist2(const Vec3& p, const Vec3& a, const Vec3& b) {
    Vec3 aToB = b - a;
    Vec3 aToP = p - a;
    float proj = Vec3::projVecScalar(aToP, aToB);
    proj = clamp(proj, 0.0f, 1.0f);
    return aToP.distance2(aToB*proj);
  }


  struct TriIndices {
    int a, b, c;
  };

  Vec3 closestOnTetrahedron(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& point) {
    const Vec3* points[4] = {&a, &b, &c, &d};
    //abc bdc adb cda
    TriIndices tris[4] = {{0, 1, 2}, {1, 3, 2}, {0, 3, 1}, {2, 3, 0}};
    float bestDist = std::numeric_limits<float>::max();
    Vec3 bestPoint;
    bool allInside = true;

    for(int i = 0; i < 4; ++i) {
      const TriIndices& tri = tris[i];
      const Vec3& triA = *points[tri.a];
      const Vec3& triB = *points[tri.b];
      const Vec3& triC = *points[tri.c];
      Vec3 normal = triangleNormal(triA, triB, triC);
      float planeDist = halfPlaneSignedDistance(normal, triA, point);

      //Get triangle distance for each triangle that sees the point.
      //If no triangle sees the point it is inside the tetrahedron
      if(planeDist > 0.0f && planeDist) {
        allInside = false;
        Vec3 closest = closestOnTri(point, triA, triB, triC);
        float triDist = closest.distance2(point);

        if(triDist < bestDist) {
          bestDist = triDist;
          bestPoint = closest;
        }
      }
    }

    if(allInside)
      return point;
    return bestPoint;
  }

  /*
    Given the ray defined by point a and its direction p,
    and another ray from point b in direction q
    We want to find the closest points on both rays to each other. We want to find scalars t and s that give us the closest points on each ray, a' and b'
    (Eq 1) a' = a + tp
    (Eq 2) b' = b + sq
    The key observation is that a' - b' is normal to both direction vectors, as if it wasn't, then moving along one of the lines would lead to a smaller distance.
    Knowing this, we get the following two equations:
    (Eq 3) (a' - b').p = 0
    (Eq 4) (a' - b').q = 0
    From here we have two equations and two unknowns (t and s) to solve for
    First we plug equation 1 into 3 to get:
    a.p + tp.p - b.p - sq.p = 0
    Rearrange for t and s
    (Eq 5) t = b.p + sq.p - a.p
                 p.q
    (Eq 6) s = p.a + p.pt - p.b
                 p.q
    Go through the journey of substituting these into 4, let's begin by expanding without substitution, giving:
    (Eq 7) a.q + tp.q - b.q - sq.q = 0
    Substitute 5 into 7, rearrange for s, factor out pp and pq in numerator
    s = p.p(q.(a-b)) - p.q(p.(a-b))
              q.qp.p - p.qp.q
    Substitute 6 into 7, rearrange for t, factour out pp and pq in numerator
    t = q.p(q.(a-b)) - q.q(p.(a-b))
              q.qp.p - p.qp.q
    And there you go, s and t that can be used to get the closest points, a' and b'
  */
  void closestOnRays(const Vec3& aStart, const Vec3& aDir, const Vec3& bStart, const Vec3& bDir, float& ta, float& tb) {
    Vec3 a = aStart;
    Vec3 p = aDir;
    Vec3 b = bStart;
    Vec3 q = bDir;
    Vec3 ab = a - b;
    float pp = p.dot(p);
    float pq = p.dot(q);
    float qq = q.dot(q);
    float qab = q.dot(ab);
    float pab = p.dot(ab);
    float invDenom = qq*pp - pq*pq;
    //If this is so, lines are parallel, and any two lined up will do
    if(std::abs(invDenom) < SYX_EPSILON) {
      tb = 0.0f;
      ta = ab.dot(aDir);
      return;
    }
    invDenom = 1.0f/invDenom;
    ta = (pp*qab - pq*pab)*invDenom;
    tb = (pq*qab - qq*pab)*invDenom;
  }

  //Moller-Trumbore intersection algorithm
  float triangleLineIntersect(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& start, const Vec3& end) {
    Vec3 dir = end - start;
    //Find vectors for two edges sharing V1
    Vec3 e1 = b - a;
    Vec3 e2 = c - a;
    //Begin calculating determinant - also used to calculate u parameter
    Vec3 P = dir.cross(e2);
    //if determinant is near zero, ray lies in plane of triangle or ray is parallel to plane of triangle
    float det = e1.dot(P);
    //NOT CULLING
    if(std::abs(det) < SYX_EPSILON)
      return -1.0f;
    float invDet = 1.0f/det;

    //calculate distance from V1 to ray origin
    Vec3 T = start - a;

    //Calculate u parameter and test bound
    float u = T.dot(P)*invDet;
    //The intersection lies outside of the triangle
    if(u < 0.0f || u > 1.0f)
      return -1.0f;

    //Prepare to test v parameter
    Vec3 Q = T.cross(e1);

    //Calculate V parameter and test bound
    float v = dir.dot(Q)*invDet;
    //The intersection lies outside of the triangle
    if(v < 0.0f || u + v  > 1.0f)
      return -1.0f;

    float t = e2.dot(Q)*invDet;
    //Cut off results past the line segment
    if(t > 1.0f)
      return -1.0f;
    return t;
  }
}