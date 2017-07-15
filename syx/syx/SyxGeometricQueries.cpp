#include "Precompile.h"

namespace Syx {
  //http://www.gamedev.net/topic/552906-closest-point-on-triangle/
  Vector3 ClosestOnTriFromEdges(const Vector3& triA, const Vector3& pToA, const Vector3& aToB, const Vector3& aToC, float* resultABT, float* resultACT, bool* clamped) {
    double a = static_cast<double>(aToB.Length2());
    double b = static_cast<double>(aToB.Dot(aToC));
    double c = static_cast<double>(aToC.Length2());
    double d = static_cast<double>(aToB.Dot(pToA));
    double e = static_cast<double>(aToC.Dot(pToA));
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
            s = Clamp(-d/a, 0.0, 1.0);
            t = 0.0;
          }
          else {
            s = 0.0;
            t = Clamp(-e/c, 0.0, 1.0);
          }
        }
        else {
          s = 0.0;
          t = Clamp(-e/c, 0.0, 1.0);
        }
      }
      else if(t < 0.0) {
        s = Clamp(-d/a, 0.0, 1.0);
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
          s = Clamp(numer/denom, 0.0, 1.0);
          t = 1.0 - s;
        }
        else {
          t = Clamp(-e/c, 0.0, 1.0);
          s = 0.0;
        }
      }
      else if(t < 0.0) {
        if(a + d > b + e) {
          double numer = c + e - b - d;
          double denom = a - 2.0*b + c;
          s = Clamp(numer/denom, 0.0, 1.0);
          t = 1.0 - s;
        }
        else {
          s = Clamp(-e/c, 0.0, 1.0);
          t = 0.0;
        }
      }
      else {
        double numer = c + e - b - d;
        double denom = a - 2.0*b + c;
        s = Clamp(numer/denom, 0.0, 1.0);
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

  Vector3 ClosestOnTri(const Vector3& p, const Vector3& triA, const Vector3& triB, const Vector3& triC, float* resultABT, float* resultACT, bool* clamped) {
    Vector3 pToA = triA - p;
    Vector3 aToB = triB - triA;
    Vector3 aToC = triC - triA;
    return ClosestOnTriFromEdges(triA, pToA, aToB, aToC, resultABT, resultACT, clamped);
  }

  Vector3 ClosestOnRay(const Vector3& p, const Vector3& a, const Vector3& b) {
    return a + Vector3::ProjVec(p - a, b - a);
  }

  Vector3 ClosestOnLine(const Vector3& p, const Vector3& a, const Vector3& b) {
    Vector3 aToB = b - a;
    float proj = Vector3::ProjVecScalar(p - a, aToB);
    proj = Clamp(proj, 0.0f, 1.0f);
    return a + aToB*proj;
  }

  float PointRayDist2(const Vector3& p, const Vector3& a, const Vector3& b) {
    Vector3 aToP = p - a;
    Vector3 proj = Vector3::ProjVec(aToP, b - a);
    return aToP.Distance2(proj);
  }

  float PointLineDist2(const Vector3& p, const Vector3& a, const Vector3& b) {
    Vector3 aToB = b - a;
    Vector3 aToP = p - a;
    float proj = Vector3::ProjVecScalar(aToP, aToB);
    proj = Clamp(proj, 0.0f, 1.0f);
    return aToP.Distance2(aToB*proj);
  }


  struct TriIndices {
    int a, b, c;
  };

  Vector3 ClosestOnTetrahedron(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d, const Vector3& point) {
    const Vector3* points[4] = {&a, &b, &c, &d};
    //abc bdc adb cda
    TriIndices tris[4] = {{0, 1, 2}, {1, 3, 2}, {0, 3, 1}, {2, 3, 0}};
    float bestDist = std::numeric_limits<float>::max();
    Vector3 bestPoint;
    bool allInside = true;

    for(int i = 0; i < 4; ++i) {
      const TriIndices& tri = tris[i];
      const Vector3& triA = *points[tri.a];
      const Vector3& triB = *points[tri.b];
      const Vector3& triC = *points[tri.c];
      Vector3 normal = TriangleNormal(triA, triB, triC);
      float planeDist = HalfPlaneSignedDistance(normal, triA, point);

      //Get triangle distance for each triangle that sees the point.
      //If no triangle sees the point it is inside the tetrahedron
      if(planeDist > 0.0f && planeDist) {
        allInside = false;
        Vector3 closestOnTri = ClosestOnTri(point, triA, triB, triC);
        float triDist = closestOnTri.Distance2(point);

        if(triDist < bestDist) {
          bestDist = triDist;
          bestPoint = closestOnTri;
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
  void ClosestOnRays(const Vector3& aStart, const Vector3& aDir, const Vector3& bStart, const Vector3& bDir, float& ta, float& tb) {
    Vector3 a = aStart;
    Vector3 p = aDir;
    Vector3 b = bStart;
    Vector3 q = bDir;
    Vector3 ab = a - b;
    float pp = p.Dot(p);
    float pq = p.Dot(q);
    float qq = q.Dot(q);
    float qab = q.Dot(ab);
    float pab = p.Dot(ab);
    float invDenom = qq*pp - pq*pq;
    //If this is so, lines are parallel, and any two lined up will do
    if(std::abs(invDenom) < SYX_EPSILON) {
      tb = 0.0f;
      ta = ab.Dot(aDir);
      return;
    }
    invDenom = 1.0f/invDenom;
    ta = (pp*qab - pq*pab)*invDenom;
    tb = (pq*qab - qq*pab)*invDenom;
  }

  //Moller-Trumbore intersection algorithm
  float TriangleLineIntersect(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& start, const Vector3& end) {
    Vector3 dir = end - start;
    //Find vectors for two edges sharing V1
    Vector3 e1 = b - a;
    Vector3 e2 = c - a;
    //Begin calculating determinant - also used to calculate u parameter
    Vector3 P = dir.Cross(e2);
    //if determinant is near zero, ray lies in plane of triangle or ray is parallel to plane of triangle
    float det = e1.Dot(P);
    //NOT CULLING
    if(std::abs(det) < SYX_EPSILON)
      return -1.0f;
    float invDet = 1.0f/det;

    //calculate distance from V1 to ray origin
    Vector3 T = start - a;

    //Calculate u parameter and test bound
    float u = T.Dot(P)*invDet;
    //The intersection lies outside of the triangle
    if(u < 0.0f || u > 1.0f)
      return -1.0f;

    //Prepare to test v parameter
    Vector3 Q = T.Cross(e1);

    //Calculate V parameter and test bound
    float v = dir.Dot(Q)*invDet;
    //The intersection lies outside of the triangle
    if(v < 0.0f || u + v  > 1.0f)
      return -1.0f;

    float t = e2.Dot(Q)*invDet;
    //Cut off results past the line segment
    if(t > 1.0f)
      return -1.0f;
    return t;
  }
}