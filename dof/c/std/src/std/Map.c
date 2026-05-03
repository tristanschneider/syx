#include <std/Map.h>

#include <std/Buffer.h>
#include <std/Compare.h>
#include <std/Diagnostics.h>
#include <std/Hash.h>
#include <float.h>
#include <math.h>

uint32_t tryAddKeyMatch(uint32_t flags, bool isMatch) {
  return isMatch ? (flags | STD_MAP_FLAG_KEY_MATCH) : flags;
}

size_t sizeToMask(size_t size) {
  return size ? size - 1 : size;
}

size_t maskToSize(size_t size) {
  return size ? size + 1 : size;
}

size_t computeNewSizeBase2(size_t currentSize, size_t desiredSize) {
  while(currentSize < desiredSize) {
    currentSize = (currentSize << 1) | 1;
  }
  return currentSize;
}

std_ProbeCtx std_map_probe(const void* key, size_t keySize, size_t bucketMask) {
  std_ProbeCtx ctx = {
    .hash = std_hash64((const uint8_t*)key, keySize),
    .bucketMask = bucketMask,
    //Abort immediately if there are no buckets
    .action = bucketMask ? std_MapLookupAction_Continue : std_MapLookupAction_Abort
  };
  ctx.bucket = ctx.hash & bucketMask;
  return ctx;
}

std_MapLookupAction std_map_probe_linear(std_ProbeCtx* ctx) {
  const size_t size = maskToSize(ctx->bucketMask);
  ctx->bucket = (ctx->bucket + 1) % size;
  //Prevent infinite loop when all buckets have been exhausted
  if(++ctx->iterationCount > size) {
    ctx->action = std_MapLookupAction_Abort;
  }
  return ctx->action;
}

size_t std_map_reserve(size_t elementCount, float targetLoadFactor) {
  return (size_t)ceilf((float)elementCount/targetLoadFactor);
}

std_MapLookupAction std_map_next(uint32_t flags) {
  switch(flags) {
    case STD_MAP_SENTRY:
      return std_MapLookupAction_Abort;
    case STD_MAP_OCCUPIED:
      return std_MapLookupAction_FoundExisting;
    default:
      return std_MapLookupAction_Continue;
  }
}

void std_map_tryInsert(std_ProbeCtx* probe, std_ProbeItem item) {
  if(item.flags & STD_MAP_FLAG_AVAILABLE) {
    probe->action = std_MapLookupAction_FoundNew;
    probe->result = item.item;
  }
  else if(item.flags & STD_MAP_FLAG_KEY_MATCH) {
    probe->action = std_MapLookupAction_FoundExisting;
    probe->result = item.item;
  }
  else {
    std_map_probe_linear(probe);
  }
}

void std_map_find(std_ProbeCtx* probe, std_ProbeItem item) {
  const uint32_t match = STD_MAP_OCCUPIED | STD_MAP_FLAG_KEY_MATCH;
  //If this is an occupied bucket and the key matches, it's the element we're looking for
  if((item.flags & match) == match) {
    probe->action = std_MapLookupAction_FoundExisting;
    probe->result = item.item;
  }
  else {
    //Key didn't match, continue unless this is the end of the bucket
    if(item.flags & STD_MAP_FLAG_IN_BUCKET) {
      std_map_probe_linear(probe);
    }
    else {
      probe->action = std_MapLookupAction_Abort;
    }
  }
}

void* std_map_rehash(std_MapRehashCtx* ctx, void* map, size_t bucketCount, size_t currentSize, std_Allocator* alloc) {
  //Exit if the map is already big enough
  const size_t oldSize = currentSize;
  if(bucketCount <= oldSize) {
    return map;
  }
  bucketCount = computeNewSizeBase2(oldSize, bucketCount);

  const size_t bucketSizeBytes = ctx->bytesForBuckets(bucketCount);
  void* newBuckets = std_Allocator_alloc(alloc, bucketSizeBytes);
  if(!newBuckets) {
    return NULL;
  }

  //Clear the target range of elements
  memset(newBuckets, 0, bucketSizeBytes);

  //Insert all the keys from the old map into the new one
  void* oldBuckets = ctx->migrateContents(map, bucketCount, newBuckets);

  //Now that everything is copied over, free the old buffer and replace it
  if(oldBuckets) {
    std_Allocator_dealloc(alloc, oldBuckets);
  }
  return map;
}

std_ProbeItem voidToProbeItem(std_VoidMapPair* pair, uint32_t key) {
  return (std_ProbeItem){
    .flags = tryAddKeyMatch(pair->flags, pair->key == key),
    .item = pair
  };
}

void std_VoidMap_dtor(std_VoidMap* map, std_Allocator* alloc) {
  if(map->buckets) {
    std_Allocator_dealloc(alloc, map->buckets);
    map->buckets = NULL;
  }
}

std_VoidMap* std_VoidMap_reserve(std_VoidMap* map, size_t elementCount, float targetLoadFactor, std_Allocator* alloc) {
  return std_VoidMap_rehash(map, std_map_reserve(elementCount, targetLoadFactor), alloc);
}

size_t std_VoidMap_bytesForBuckets(size_t bucketCount) {
  return (bucketCount + 1)*sizeof(std_VoidMapPair);
}

void* std_VoidMap_migrateContents(void* vMap, size_t bucketCount, void* buckets) {
  std_VoidMapPair* newBuckets = (std_VoidMapPair*)buckets;
  std_VoidMap* map = (std_VoidMap*)vMap;

  //Set the sentry
  newBuckets[bucketCount] = (std_VoidMapPair){
    .key = 0,
    .flags = STD_MAP_SENTRY,
    .value = NULL
  };

  std_VoidMap newMap = {
    .buckets = newBuckets,
    .elementCount = map->elementCount,
    .bucketMask = sizeToMask(bucketCount)
  };

  //Insert all the keys from the old map into the new one
  std_VoidMapPair* it = std_VoidMap_begin(map);
  while(it) {
    std_VoidMap_tryInsert(&newMap, it->key, it->value);
    it = std_VoidMap_next(it);
  }

  void* result = map->buckets;
  map->buckets = buckets;
  map->bucketMask = sizeToMask(bucketCount);
  return result;
}

std_VoidMap* std_VoidMap_rehash(std_VoidMap* map, size_t bucketCount, std_Allocator* alloc) {
  std_MapRehashCtx ctx = {
    .bytesForBuckets = &std_VoidMap_bytesForBuckets,
    .migrateContents = &std_VoidMap_migrateContents,
  };

  return (std_VoidMap*)std_map_rehash(&ctx, map, bucketCount, maskToSize(map->bucketMask), alloc);
}

std_VoidMapPair* std_VoidMap_tryInsert(std_VoidMap* map, uint32_t key, void* value) {
  std_ProbeCtx probe = std_map_probe(&key, sizeof(key), map->bucketMask);

  while(probe.action == std_MapLookupAction_Continue) {
    std_map_tryInsert(&probe, voidToProbeItem(&map->buckets[probe.bucket], key));
  }

  //Assign the new value if a bucket was found, return without modifying if it already existed
  std_VoidMapPair* result = (std_VoidMapPair*)probe.result;
  if(result && probe.action == std_MapLookupAction_FoundNew) {
    result->key = key;
    result->value = value;
    result->flags = STD_MAP_OCCUPIED;
    map->elementCount++;
  }
  return result;
}

std_VoidMapPair* std_VoidMap_insert(std_VoidMap* map, uint32_t key, void* value, float targetLoadFactor, std_Allocator* alloc) {
  std_VoidMap* rmap = std_VoidMap_reserve(map, map->elementCount + 1, targetLoadFactor, alloc);
  return rmap ? std_VoidMap_tryInsert(rmap, key, value) : NULL;
}

bool std_VoidMap_erase(std_VoidMap* map, uint32_t key) {
  std_VoidMapPair* pair = std_VoidMap_find(map, key);
  if(pair) {
    pair->flags = STD_MAP_TOMBSTONE;
    pair->key = 0;
    pair->value = NULL;
    map->elementCount--;
    return true;
  }
  return false;
}

void std_VoidMap_clear(std_VoidMap* map) {
  const std_VoidMapPair empty = {
    .value = NULL,
    .key = 0,
    .flags = STD_MAP_EMPTY
  };

  std_VoidMapPair* it = std_VoidMap_begin(map);
  while(it) {
    *it = empty;
    it = std_VoidMap_next(it);
  }

  map->elementCount = 0;
}

std_VoidMapPair* std_VoidMap_find(std_VoidMap* map, uint32_t key) {
  std_ProbeCtx probe = std_map_probe(&key, sizeof(key), map->bucketMask);

  while(probe.action == std_MapLookupAction_Continue) {
    std_map_find(&probe, voidToProbeItem(&map->buckets[probe.bucket], key));
  }

  return (std_VoidMapPair*)probe.result;
}

std_VoidMapPair* std_VoidMap_begin(std_VoidMap* map) {
  std_VoidMapPair* result = map->buckets;
  return result && result->flags == STD_MAP_OCCUPIED ? result : std_VoidMap_next(result);
}

std_VoidMapPair* std_VoidMap_next(std_VoidMapPair* pair) {
  if(!pair) {
    return NULL;
  }
  //Not allowed to advance past the end
  STD_ASSERT(pair->flags != STD_MAP_SENTRY);

  while(true) {
    ++pair;
    switch(std_map_next(pair->flags)) {
      case std_MapLookupAction_FoundExisting:
        return pair;
      case std_MapLookupAction_Abort:
        return NULL;
    }
  }
}

size_t std_VoidMap_size(std_VoidMap* map) {
  return map->elementCount;
}
