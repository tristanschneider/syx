#pragma once
#include "UniqueID.h"

//Represents a valid unique id claimed through an IDRegistry. Default registry implementation uses the lifetime to unregistrer the id
struct IClaimedUniqueID {
  virtual ~IClaimedUniqueID() = default;

  virtual const UniqueID& operator*() const = 0;
};

//This class provides a threadsafe way to generate new keys and claim well known ones. It is intended to be used across systems to immediately know
//if duplicate ids would be created and respond accordingly.
struct IIDRegistry {
  virtual ~IIDRegistry() = default;

  //Guaranteed to return a new id
  virtual std::unique_ptr<IClaimedUniqueID> generateNewUniqueID() = 0;
  //Attempt to claim a known ID. If the id is available, return a new ClaimedUniqueID. If it was taken, returns null
  virtual std::unique_ptr<IClaimedUniqueID> tryClaimKnownID(const UniqueID& id) = 0;
};

//Default implementation
namespace create {
  std::unique_ptr<IIDRegistry> idRegistry();
}