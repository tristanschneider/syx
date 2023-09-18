#pragma once

class IAppBuilder;

namespace GameplayExtract {
  //Copies common position/velocity data from Pos to GPos so gameplay can have parallel read only access
  //to it while physics operates on the real values
  void extractGameplayData(IAppBuilder& builder);
  //Turn GLinearImpulse and GAngular impulse into stat effects if nonzero
  void applyGameplayImpulses(IAppBuilder& builder);
}