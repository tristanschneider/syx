#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <std/Allocator.h>

//True when this bucket is available to accept a new value
#define STD_MAP_FLAG_AVAILABLE ((uint32_t)1)
//True when bucket iteration should continue on this bucket
#define STD_MAP_FLAG_IN_BUCKET ((uint32_t)(1 << 1))
//True when this is the end of the bucket container
#define STD_MAP_FLAG_END ((uint32_t)(1 << 2))
//Set by the caller into generic method to indicate the key matches
#define STD_MAP_FLAG_KEY_MATCH ((uint32_t)(1 << 3))

#define STD_MAP_EMPTY STD_MAP_FLAG_AVAILABLE
#define STD_MAP_TOMBSTONE (STD_MAP_FLAG_AVAILABLE | STD_MAP_FLAG_IN_BUCKET)
#define STD_MAP_SENTRY STD_MAP_FLAG_END
#define STD_MAP_OCCUPIED STD_MAP_FLAG_IN_BUCKET

#define STD_MAP_LOAD_FACTOR 0.6f

enum std_MapLookupAction {
  std_MapLookupAction_Continue = 0,
  std_MapLookupAction_FoundExisting,
  std_MapLookupAction_FoundNew,
  std_MapLookupAction_Abort,
};
typedef enum std_MapLookupAction std_MapLookupAction;

struct std_ProbeCtx {
  size_t bucket;
  size_t bucketMask;
  size_t iterationCount;
  void* result;
  std_MapLookupAction action;
};
typedef struct std_ProbeCtx std_ProbeCtx;

//Generalization of the storage type accessed by probing
struct std_ProbeItem {
  //MAP_FLAG bitset
  uint32_t flags;
  //The desired value in `result` when finding a match through probe operations
  void* item;
};
typedef struct std_ProbeItem std_ProbeItem;

struct std_MapRehashCtx {
  //Get the number of bytes required to store the indicated number of buckets.
  size_t(*bytesForBuckets)(size_t bucketCount);
  //Migrate the contents from the original map into this new bucket buffer
  //Memory will be zeroed before this is called.
  //Returns the previous buffer
  void*(*migrateContents)(void* map, size_t bucketCount, void* buckets);
};
typedef struct std_MapRehashCtx std_MapRehashCtx;

//Storage-agnostic methods for operating on maps
//Start a probing operation at the bucket corresponding to the given key.
std_ProbeCtx std_map_probe(const void* key, size_t keySize, size_t bucketMask);
//Start a probing operation at the bucket indicatd by its index
std_ProbeCtx std_map_probeIndex(size_t bucketIndex, size_t bucketMask);
std_MapLookupAction std_map_probe_linear(std_ProbeCtx* ctx);
std_MapLookupAction std_map_next(uint32_t flags);
void std_map_tryInsert(std_ProbeCtx* probe, std_ProbeItem item);
void std_map_find(std_ProbeCtx* probe, std_ProbeItem item);
//Returns the number of buckets needed to respect the given load factor for the desired number of elements
size_t std_map_reserve(size_t elementCount, float targetLoadFactor);
//Rehash the given map if needed, allocating new storage and copying the contents over using the ctx functions.
//Returns the map if succsesful, otherwise null.
void* std_map_rehash(std_MapRehashCtx* ctx, void* map, size_t bucketCount, size_t currentSize, std_Allocator* alloc);

//A bucket can be trimmed if it ends in a tombstone.
//This goes right and tries to find that.
//If it finds an occupied element it sets the action to Abort
//If it finds a bucket that can be trimmed it returns FoundExisting
void std_map_tryTrimBucket(std_ProbeCtx* probe, uint32_t flags);
//Intended to be used after tryTrimBucket has returned FoundExisting
//Clears all tombstones off the end of the bucket until abort is returned.
void std_map_trimBucket(std_ProbeCtx* probe, uint32_t* flags);

//Implementation of uint32_t -> void* map using tombstones
struct std_VoidMapPair {
  void* value;
  uint32_t key;
  uint32_t flags;
};
typedef struct std_VoidMapPair std_VoidMapPair;

struct std_VoidMapInsertPair {
  std_VoidMapPair* inserted;
  bool isNew;
};
typedef struct std_VoidMapInsertPair std_VoidMapInsertPair;

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
std_VoidMapInsertPair std_VoidMap_tryInsert(std_VoidMap* map, uint32_t key, void* value);
//Shorthand for reserve+tryInsert
std_VoidMapInsertPair std_VoidMap_insert(std_VoidMap* map, uint32_t key, void* value, float targetLoadFactor, std_Allocator* alloc);
//Erase the given element, returning true if it existed
bool std_VoidMap_eraseKey(std_VoidMap* map, uint32_t key);
bool std_VoidMap_eraseIt(std_VoidMap* map, std_VoidMapPair* it);
void std_VoidMap_clear(std_VoidMap* map);

std_VoidMapPair* std_VoidMap_find(std_VoidMap* map, uint32_t key);
std_VoidMapPair* std_VoidMap_begin(std_VoidMap* map);
std_VoidMapPair* std_VoidMap_next(std_VoidMapPair* pair);

size_t std_VoidMap_size(std_VoidMap* map);

