#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <std/Allocator.h>

enum std_MapLookupAction {
};

struct std_ProbeCtx {
  uint64_t hash;
  size_t bucket;
  size_t iterationCount;
};
typedef struct std_ProbeCtx std_ProbeCtx;

//Storage-agnostic methods for operating on maps
//Takes a zero-initialized context and returns the probe indicating the matching bucket or null if there are none.
std_ProbeCtx* std_map_probe(std_ProbeCtx* ctx, const void* key, size_t keySize, size_t bucketMask);
std_ProbeCtx* std_map_probe_linear(std_ProbeCtx* ctx, size_t bucketMask);

//Assure the amount of buckets needed for the element count given the target load factor
//Returns nonzero if the bucket count needs to change and returns enough buckets for the target load factor.
size_t std_map_assure(size_t bucketCount, size_t elementCount, float targetLoadFactor);

//Implementation of size_t -> void* map using tombstones
struct std_VoidMapPair {
  void* value;
  uint32_t key;
  uint32_t flags;
};
typedef struct std_VoidMapPair std_VoidMapPair;

//May be zero initialized
struct std_VoidMap {
  std_VoidMapPair* buckets;
  //Number of elements in the map
  size_t elementCount;
  //Mask for bucket size which is always a power of 2. &= ensures a valid bucket index.
  size_t bucketMask;
};
typedef struct std_VoidMap std_VoidMap;

void std_VoidMap_dtor(std_VoidMap* map, std_Allocator* alloc);

//Ensure the map has enough space to contain the given number of elements while staying under the target load factor
//Returns the input map if the space could be reserved
std_VoidMap* std_VoidMap_reserve(std_VoidMap* map, size_t elementCount, float targetLoadFactor, std_Allocator* alloc);
//Ensures the map has at least this many buckets
std_VoidMap* std_VoidMap_rehash(std_VoidMap* map, size_t bucketCount, std_Allocator* alloc);

//Insert into the map ignoring the target load factor. Caller is expected to have reserved the required space.
//Returns null if insertion is impossible.
std_VoidMapPair* std_VoidMap_tryInsert(std_VoidMap* map, uint32_t key, void* value);
//Shorthand for reserve+tryInsert
std_VoidMapPair* std_VoidMap_insert(std_VoidMap* map, uint32_t key, void* value, float targetLoadFactor, std_Allocator* alloc);
//Erase the given element, returning true if it existed
bool std_VoidMap_erase(std_VoidMap* map, uint32_t key);
void std_VoidMap_clear(std_VoidMap* map);

std_VoidMapPair* std_VoidMap_find(std_VoidMap* map, uint32_t key);
std_VoidMapPair* std_VoidMap_begin(std_VoidMap* map);
std_VoidMapPair* std_VoidMap_next(std_VoidMapPair* pair);

size_t std_VoidMap_size(std_VoidMap* map);

