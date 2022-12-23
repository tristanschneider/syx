#include "Precompile.h"
#include "Simulation.h"

#include "Queries.h"

void Simulation::update(GameDatabase& db) {
  using namespace Tags;
  Queries::viewEachRow<FloatRow<Rot, Angle>>(db, [](FloatRow<Rot, Angle>& row) {
    for(float& a : row.mElements) {
      a += 0.001f;
    }
  });
}