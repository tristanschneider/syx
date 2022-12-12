#include "IspcMath.h"
#include "Uniforms.h"

struct UniformJacobian {
  //Linear component for both objects, negated for B
  uniform UniformVec3* linear;
  uniform UniformVec3* angularA;
  uniform UniformVec3* angularB;

  uniform float* invConstraintMass;
  uniform float* bias;
  uniform float* lambdaMin;
  uniform float* lambdaMax;
};

struct UniformConstraintCache {
  //Linear and angular components pre-multiplied by inverse mass
  uniform UniformVec3* linearA;
  uniform UniformVec3* linearB;
  uniform UniformVec3* angularA;
  uniform UniformVec3* angularB;
  uniform float* lambdaSum;
};

struct UniformConstraintObject {
  uniform UniformVec3* linearVelocity;
  uniform UniformVec3* angularVelocity;
};

float dot(float ax, float ay, float az, float bx, float by, float bz) {
  return ax*bx + ay*by + az*bz;
}

struct UniformObjectMass {
  uniform float* mass;
  uniform UniformSymmetricMatrix* inertia;
};

export void setupPointConstraint(
  uniform const UniformObjectMass& a,
  uniform const UniformObjectMass& b,
  uniform const UniformVec3& positionA,
  uniform const UniformVec3& positionB,
  uniform const UniformVec3& pointA,
  uniform const UniformVec3& pointB,
  uniform const UniformVec3& normalA,
  uniform UniformJacobian& jacobian,
  uniform UniformConstraintCache& cache,
  uniform uint32 count
) {
  foreach(i = 0 ... count) {
    jacobian.linear->x[i] = -normalA.x[i];
    jacobian.linear->y[i] = -normalA.y[i];
    jacobian.linear->z[i] = -normalA.z[i];

    //Premultiply linear terms by mass
    cache.linearA->x[i] = -normalA.x[i]*a.mass[i];
    cache.linearA->y[i] = -normalA.y[i]*a.mass[i];
    cache.linearA->z[i] = -normalA.z[i]*a.mass[i];

    //Normal is the same for b but reversed
    cache.linearB->x[i] = normalA.x[i]*a.mass[i];
    cache.linearB->y[i] = normalA.y[i]*a.mass[i];
    cache.linearB->z[i] = normalA.z[i]*a.mass[i];

    //Center of mass on A to impulse point
    float rax = pointA.x[i] - positionA.x[i];
    float ray = pointA.y[i] - positionA.y[i];
    float raz = pointA.z[i] - positionA.z[i];
    // rA cross -normalA
    jacobian.angularA->x[i] = cache.angularA->x[i] = -ray*normalA.z[i] + raz*normalA.y[i];
    jacobian.angularA->y[i] = cache.angularA->y[i] = -raz*normalA.x[i] + rax*normalA.z[i];
    jacobian.angularA->z[i] = cache.angularA->z[i] = -rax*normalA.y[i] + ray*normalA.x[i];

    float rbx = pointB.x[i] - positionB.x[i];
    float rby = pointB.y[i] - positionB.y[i];
    float rbz = pointB.z[i] - positionB.z[i];
    // rB cross normalA (which is normalB)
    jacobian.angularB->x[i] = cache.angularB->x[i] = rby*normalA.z[i] - rbz*normalA.y[i];
    jacobian.angularB->y[i] = cache.angularB->y[i] = rbz*normalA.x[i] - rbx*normalA.z[i];
    jacobian.angularB->z[i] = cache.angularB->z[i] = rbx*normalA.y[i] - rby*normalA.x[i];

    //Premultiply angular portion by symmetric inertia matrix
    // [ a, b, c]   [ax]   [ aax + bay + caz ]
    // [ b, d, e] * [ay] = [ bax + day + eaz ]
    // [ c, e, f]   [az]   [ cax + eay + faz ]
    const float ax = cache.angularA->x[i];
    const float ay = cache.angularA->y[i];
    const float az = cache.angularA->z[i];
    cache.angularA->x[i] = ax*a.inertia->a[i] + ay*a.inertia->b[i] + az*a.inertia->c[i];
    cache.angularA->y[i] = ax*a.inertia->b[i] + ay*a.inertia->d[i] + az*a.inertia->e[i];
    cache.angularA->z[i] = ax*a.inertia->c[i] + ay*a.inertia->e[i] + az*a.inertia->f[i];

    const float bx = cache.angularB->x[i];
    const float by = cache.angularB->y[i];
    const float bz = cache.angularB->z[i];
    cache.angularB->x[i] = bx*b.inertia->a[i] + by*b.inertia->b[i] + bz*b.inertia->c[i];
    cache.angularB->y[i] = bx*b.inertia->b[i] + by*b.inertia->d[i] + bz*b.inertia->e[i];
    cache.angularB->z[i] = bx*b.inertia->c[i] + by*b.inertia->e[i] + bz*b.inertia->f[i];

    //Dot product of angular portion with its mass multiplied counterpart for A and B
    const float angularConstraintMass = dot(jacobian.angularA->x[i], jacobian.angularA->y[i], jacobian.angularA->z[i], cache.angularA->x[i], cache.angularA->y[i], cache.angularA->z[i])
      + dot(jacobian.angularB->x[i], jacobian.angularB->y[i], jacobian.angularB->z[i], cache.angularB->x[i], cache.angularB->y[i], cache.angularB->z[i]);
    const float linearConstraintMass = a.mass[i] + b.mass[i];

    jacobian.invConstraintMass[i] = safeDivide(1.0f, linearConstraintMass + angularConstraintMass);
  }
}

//Constraint for force being applied at a point meaning a linear and angular component for both sides
//J = jacobian, V = velocity vector, b = bias term, M = Jacobian mass
//Calculate lambda = -(JV + b)*M^1
export void solvePointConstraint(
  uniform const UniformJacobian& jacobian,
  uniform UniformConstraintCache& cache,
  uniform UniformConstraintObject& a,
  uniform UniformConstraintObject& b,
  uniform uint32 count
) {
  foreach(i = 0 ... count) {
    float jv = dot(jacobian.linear->x[i], jacobian.linear->y[i], jacobian.linear->z[i], a.linearVelocity->x[i], a.linearVelocity->y[i], a.linearVelocity->z[i]);
    //Subtract because normal is flipped for B, which is the same result but negative
    jv -= dot(jacobian.linear->x[i], jacobian.linear->y[i], jacobian.linear->z[i], b.linearVelocity->x[i], b.linearVelocity->y[i], b.linearVelocity->z[i]);
    jv += dot(jacobian.angularA->x[i], jacobian.angularA->y[i], jacobian.angularA->z[i], a.angularVelocity->x[i], a.angularVelocity->y[i], a.angularVelocity->z[i]);
    jv += dot(jacobian.angularB->x[i], jacobian.angularB->y[i], jacobian.angularB->z[i], b.angularVelocity->x[i], b.angularVelocity->y[i], b.angularVelocity->z[i]);

    float lambda = -(jv + jacobian.bias[i])*jacobian.invConstraintMass[i];

    float oldSum = cache.lambdaSum[i];
    //Accumulate the current lambda, then clamp the total.
    cache.lambdaSum[i] += lambda;
    cache.lambdaSum[i] = clamp(cache.lambdaSum[i], jacobian.lambdaMin[i], jacobian.lambdaMax[i]);
    //Apply the clamped difference back to this current lambda. Unchanged if nothing was clamped
    lambda = cache.lambdaSum[i] - oldSum;

    //Apply impulse
    cache.linearA->x[i] += a.linearVelocity->x[i]*lambda;
    cache.linearA->y[i] += a.linearVelocity->y[i]*lambda;
    cache.linearA->z[i] += a.linearVelocity->z[i]*lambda;

    cache.linearB->x[i] += b.linearVelocity->x[i]*lambda;
    cache.linearB->y[i] += b.linearVelocity->y[i]*lambda;
    cache.linearB->z[i] += b.linearVelocity->z[i]*lambda;

    cache.angularA->x[i] += a.angularVelocity->x[i]*lambda;
    cache.angularA->y[i] += a.angularVelocity->y[i]*lambda;
    cache.angularA->z[i] += a.angularVelocity->z[i]*lambda;

    cache.angularB->x[i] += b.angularVelocity->x[i]*lambda;
    cache.angularB->y[i] += b.angularVelocity->y[i]*lambda;
    cache.angularB->z[i] += b.angularVelocity->z[i]*lambda;
  }
}