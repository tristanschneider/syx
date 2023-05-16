#pragma once

struct TaskRange;
struct GameDB;

namespace GameplayExtract {
  //Copies common position/velocity data from Pos to GPos so gameplay can have parallel read only access
  //to it while physics operates on the real values
  TaskRange extractGameplayData(GameDB db);
  //Turn GLinearImpulse and GAngular impulse into stat effects if nonzero
  TaskRange applyGameplayImpulses(GameDB db);
}