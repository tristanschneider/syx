#include "IspcMath.h"
#include "Uniforms.h"

struct Matrix {
  float a, b, c,
        d, e, f,
        g, h, i;
};

struct SymmetricMatrix {
  float a, b, c,
           d, e,
              f;
};

Matrix quat_to_matrix(float<4> q) {
  Matrix result = {
    1.0f - 2.0f*q.y*q.y - 2.0f*q.z*q.z, 2.0f*q.x*q.y - 2.0f*q.z*q.w, 2.0f*q.x*q.z + 2.0f*q.y*q.w,
    2.0f*q.x*q.y + 2.0f*q.z*q.w, 1.0f - 2.0f*q.x*q.x - 2.0f*q.z*q.z, 2.0f*q.y*q.z - 2.0f*q.x*q.w,
    2.0f*q.x*q.z - 2.0f*q.y*q.w, 2.0f*q.y*q.z + 2.0f*q.x*q.w, 1.0f - 2.0f*q.x*q.x - 2.0f*q.y*q.y
  };
  return result;
}

//localinertia is a vec3 local space inverse inertia, rot is a rotation matrix, result is inertia tensor (matrix)
//newInertia = rot.scaled(localInertia) * rot.transposed();
//Scaled part
//[a, b, c]  [x, 0, 0] [ax, by, cz]
//[d, e, f]* [0, y, 0]=[dx, ey, fz]
//[g, h, i]  [0, 0, z] [gx, hy, iz]
// Multiply by transpose of itself part
//[ax, by, cz]   [a, d, g]   [aax + bby + ccz, adx + bey + cfz, agx + bhy + ciz]
//[dx, ey, fz] * [b, e, h] = [adx + bey + cfz, ddx + eey + ffz, dgx + ehy + fiz]
//[gx, hy, iz]   [c, f, i]   [agx + bhy + ciz, dgx + ehy + fiz, ggx + hhy + iiz]
// Result is symmetric
//[ax, by, cz]   [a, d, g]   [aax + bby + ccz, adx + bey + cfz, agx + bhy + ciz]
//[dx, ey, fz] * [b, e, h] = [               , ddx + eey + ffz, dgx + ehy + fiz]
//[gx, hy, iz]   [c, f, i]   [               ,                , ggx + hhy + iiz]
export void recomputeInertiaTensor(
  const uniform UniformConstQuat& orientation,
  const uniform UniformConstVec3& localInertia,
  uniform UniformSymmetricMatrix& inertia,
  uniform uint32 count) {
  foreach(i = 0 ... count) {
    const float<4> o = { orientation.i[i], orientation.j[i], orientation.k[i], orientation.w[i] };
    //Convert rotation to matrix
    Matrix rot = quat_to_matrix(o);
    //Compute rot.scaled(localInertia) * rot.transposed()
    const float x = localInertia.x[i];
    const float y = localInertia.y[i];
    const float z = localInertia.z[i];
    inertia.a[i] = rot.a*rot.a*x + rot.b*rot.b*y + rot.c*rot.c*z; inertia.b[i] = rot.a*rot.d*x + rot.b*rot.e*y + rot.c*rot.f*z; inertia.c[i] = rot.a*rot.g*x + rot.b*rot.h*y + rot.c*rot.i*z;
                                                                  inertia.d[i] = rot.d*rot.d*x + rot.e*rot.e*y + rot.f*rot.f*z; inertia.e[i] = rot.d*rot.g*x + rot.e*rot.h*y + rot.f*rot.i*z;
                                                                                                                                inertia.f[i] = rot.g*rot.g*x + rot.h*rot.h*y + rot.i*rot.i*z;
  }
}

//Compute inertia and mass of spheres with a given radius.
//Should be followed up by invertMass
export void computeSphereMass(
  const uniform float radius[],
  uniform float resultMass[],
  uniform UniformVec3& resultInertia,
  uniform uint32 count
) {
  const float unitMass = 4.0f*PI/3.0f;
  const float inertiaScalar = 2.0f/5.0f;
  foreach(i = 0 ... count) {
    const float radiusSq = radius[i]*radius[i];
    //4pi(r^3)
    resultMass[i] = unitMass*radiusSq*radius[i];

    //(2m(r^2))/5
    const float inertia = inertiaScalar*radiusSq*resultMass[i];
    resultInertia.x[i] = inertia;
    resultInertia.y[i] = inertia;
    resultInertia.z[i] = inertia;
  }
}

//Turn input mass into 1/mass*density. Same operation for mass and components of local inertia
export void invertMass(
  uniform float mass[],
  const uniform float density[],
  uniform uint32 count
) {
  foreach(i = 0 ... count) {
    mass[i] = safeDivide(1.0f, mass[i]*density[i]);
  }
}