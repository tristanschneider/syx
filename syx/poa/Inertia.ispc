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
  const uniform UniformQuat& orientation,
  const uniform UniformVec3& localInertia,
  uniform UniformSymmetricMatrix& inertia,
  uniform uint32 count) {
  uniform int columnCount = 3;
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
