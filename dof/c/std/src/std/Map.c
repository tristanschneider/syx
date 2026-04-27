#include <std/Map.h>

#include <std/Buffer.h>
#include <std/Compare.h>
#include <std/Hash.h>
#include <float.h>
#include <math.h>

//True when this bucket is available to accept a new value
#define MAP_FLAG_AVAILABLE ((uint32_t)1)
//True when bucket iteration should continue on this bucket
#define MAP_FLAG_IN_BUCKET ((uint32_t)(1 << 1))
//True when this is the end of the bucket container
#define MAP_FLAG_END ((uint32_t)(1 << 2))

//TODO: consider generic functions that take the flags to direct how find, insert, erase, iterate works
const uint32_t MAP_EMPTY = MAP_FLAG_AVAILABLE;
const uint32_t MAP_TOMBSTONE = MAP_FLAG_AVAILABLE | MAP_FLAG_IN_BUCKET;
const uint32_t MAP_SENTRY = MAP_FLAG_END;
const uint32_t MAP_OCCUPIED = MAP_FLAG_IN_BUCKET;

size_t sizeToMask(size_t size) {
  return size ? size - 1 : size;
}

size_t maskToSize(size_t size) {
  return size ? size + 1 : size;
}

std_ProbeCtx* std_map_probe(std_ProbeCtx* ctx, const void* key, size_t keySize, size_t bucketMask) {
  ctx->hash = std_hash64((const uint8_t*)key, keySize);
  ctx->bucket = ctx->hash & bucketMask;
  ctx->iterationCount = (size_t)0;
  //Return the result unless there are no buckets
  return bucketMask ? ctx : NULL;
}

std_ProbeCtx* std_map_probe_linear(std_ProbeCtx* ctx, size_t bucketMask) {
  const size_t size = maskToSize(bucketMask);
  ctx->bucket = (ctx->bucket + 1) % size;
  //Prevent infinite loop when all buckets have been exhausted
  return ++ctx->iterationCount > size ? NULL : ctx;
}

size_t std_map_assure(size_t bucketCount, size_t elementCount, float targetLoadFactor) {
  size_t resize = std_max((size_t)1, bucketCount);
  while((float)elementCount/(float)resize > targetLoadFactor) {
    resize *= 2;
  }
  return resize == bucketCount ? (size_t)0 : resize;
}

void std_VoidMap_dtor(std_VoidMap* map, std_Allocator* alloc) {
  if(map->buckets) {
    std_Allocator_dealloc(alloc, map->buckets);
    map->buckets = NULL;
  }
}

std_VoidMap* std_VoidMap_reserve(std_VoidMap* map, size_t elementCount, float targetLoadFactor, std_Allocator* alloc) {
  return std_VoidMap_rehash(map, (size_t)ceilf((float)elementCount/targetLoadFactor), alloc);
}

std_VoidMap* std_VoidMap_rehash(std_VoidMap* map, size_t bucketCount, std_Allocator* alloc) {
  //Exit if the map is already big enough
  if(bucketCount <= maskToSize(map->bucketMask)) {
    return map;
  }

  //Add one for sentry
  std_VoidMapPair* newBuckets = std_Allocator_alloc(alloc, (bucketCount + 1)*sizeof(std_VoidMapPair));
  if(!newBuckets) {
    return NULL;
  }

  //Clear the target range of elements
  memset(newBuckets, 0, bucketCount*sizeof(std_VoidMapPair));
  //Set the sentry
  newBuckets[bucketCount] = (std_VoidMapPair){
    .key = 0,
    .flags = MAP_SENTRY,
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

  //Now that everything is copied over, free the old buffer and replace it
  if(map->buckets) {
    std_Allocator_dealloc(alloc, map->buckets);
  }
  *map = newMap;
  return map;
}

std_VoidMapPair* std_VoidMap_tryInsert(std_VoidMap* map, uint32_t key, void* value) {
  std_ProbeCtx ctx = { 0 };
  std_ProbeCtx* probe = std_map_probe(&ctx, &key, sizeof(key), map->bucketMask);
  std_VoidMapPair* result = NULL;

  while(probe) {
    result = &map->buckets[probe->bucket];
    //Stop if an available slot is found
    if(result->flags & MAP_FLAG_AVAILABLE) {
      break;
    }
    //Stop if the key matches, continue if the bucket is taken with an unrelated key
    if(result->key == key) {
      result->value = value;
      return result;
    }

    probe = std_map_probe_linear(probe, map->bucketMask);
  }

  if(result) {
    result->key = key;
    result->value = value;
    result->flags = MAP_OCCUPIED;
    map->elementCount++;
  }
  return result;
}

std_VoidMapPair* std_VoidMap_insert(std_VoidMap* map, uint32_t key, void* value, float targetLoadFactor, std_Allocator* alloc) {
  std_VoidMap* rmap = std_VoidMap_reserve(map, map->elementCount + 1, targetLoadFactor, alloc);
  return rmap ? std_VoidMap_tryInsert(rmap, key, value) : NULL;
}

//TODO: common function for key lookup
bool std_VoidMap_erase(std_VoidMap* map, uint32_t key) {
  std_ProbeCtx ctx = { 0 };
  std_ProbeCtx* probe = std_map_probe(&ctx, &key, sizeof(key), map->bucketMask);

  while(probe) {
    std_VoidMapPair* pair = &map->buckets[probe->bucket];
    //If the element is found, erase it and stop searching.
    if(pair->flags == MAP_OCCUPIED && pair->key == key) {
      pair->flags = MAP_TOMBSTONE;
      pair->key = 0;
      pair->value = NULL;
      map->elementCount--;
      return true;
    }

    //Keep searching until the end of the bucket
    probe = (pair->flags & MAP_FLAG_AVAILABLE) ? std_map_probe_linear(probe, map->bucketMask) : NULL;
  }
  return false;
}

void std_VoidMap_clear(std_VoidMap* map) {
  const std_VoidMapPair empty = {
    .value = NULL,
    .key = 0,
    .flags = MAP_EMPTY
  };

  std_VoidMapPair* it = std_VoidMap_begin(map);
  while(it) {
    *it = empty;
    it = std_VoidMap_next(it);
  }

  map->elementCount = 0;
}

std_VoidMapPair* std_VoidMap_find(std_VoidMap* map, uint32_t key) {
  std_ProbeCtx ctx = { 0 };
  std_ProbeCtx* probe = std_map_probe(&ctx, &key, sizeof(key), map->bucketMask);

  while(probe) {
    std_VoidMapPair* pair = &map->buckets[probe->bucket];
    //If the element is found, erase it and stop searching.
    if(pair->flags == MAP_OCCUPIED && pair->key == key) {
      return pair;
    }

    //Keep searching until the end of the bucket
    probe = (pair->flags & MAP_FLAG_AVAILABLE) ? std_map_probe_linear(probe, map->bucketMask) : NULL;
  }

  return NULL;
}

std_VoidMapPair* std_VoidMap_begin(std_VoidMap* map) {
  std_VoidMapPair* result = map->buckets;
  return result && result->flags == MAP_OCCUPIED ? result : std_VoidMap_next(result);
}

std_VoidMapPair* std_VoidMap_next(std_VoidMapPair* pair) {
  while(true) {
    if(pair->flags == MAP_SENTRY) {
      return NULL;
    }
    if(pair->flags == MAP_OCCUPIED) {
      return pair;
    }
    ++pair;
  }
  return NULL;
}

size_t std_VoidMap_size(std_VoidMap* map) {
  return map->elementCount;
}
