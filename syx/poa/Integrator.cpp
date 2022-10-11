#include "Integrator.h"

#include "test.h"

// Components maybe like this, might not actually expose the components to this library though
// Tags can be used to reduce some of the copy paste redundancy with templaces for each element
struct X {};
struct Y {};
struct Z {};

template<class Element>
struct FloatComponent {
  float mValue = 0.0f;
};
static_assert(sizeof(FloatComponent<X>) == sizeof(float));
static_assert(sizeof(FloatComponent<Y>) == sizeof(float));
static_assert(sizeof(FloatComponent<Z>) == sizeof(float));

template<class Element>
using PositionComponent = FloatComponent<Element>;
template<class Element>
using VelocityComponent = FloatComponent<Element>;

void integratorTest() {
  constexpr int32_t SIZE = 5;
  std::array<PositionComponent<X>, SIZE> posX;
  std::array<PositionComponent<Y>, SIZE> posY;
  std::array<PositionComponent<Z>, SIZE> posZ;
  std::array<VelocityComponent<X>, SIZE> velocityX;
  std::array<VelocityComponent<Y>, SIZE> velocityY;
  std::array<VelocityComponent<Z>, SIZE> velocityZ;
  float dt = 1.0f/20.0f;

  ispc::integrateLinearPosition((float*)posX.data(), (float*)velocityX.data(), dt, SIZE);
  ispc::integrateLinearPosition((float*)posY.data(), (float*)velocityY.data(), dt, SIZE);
  ispc::integrateLinearPosition((float*)posZ.data(), (float*)velocityZ.data(), dt, SIZE);
}
