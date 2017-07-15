
#pragma once
//Defined if SIMD is enabled
#define SENABLED

#include "SyxAssert.h"

#define AssertAlignment(pSource) SyxAssertError(reinterpret_cast<size_t>(&pSource) % 16 == 0);
#define SAlignment 16

#ifndef SENABLED
//These are used all over the place but don't need to do anything without SIMD
#define SAlign
#define AlignedAlloc(size) malloc(size)
#define AlignedFree free
#else

#include <xmmintrin.h>


#ifndef _WIN32
#define Align(alignment) __attribute__((aligned(alignment)))
#define FInline inline
#else
#define Align(alignment) __declspec(align(alignment))
#define FInline __forceinline
#endif
#define SAlign Align(SAlignment)

#define SPadClass(size) char mPadding[SAlignment - ((size) % SAlignment)];
#define SVectorSize sizeof(std::vector<void*>)
#define SVecListSize sizeof(VecList<void*>)
#define SMapSize sizeof(std::unordered_map<void*, void*>)
#define SSetSize sizeof(std::unordered_set<void*>)
#define SPtrSize sizeof(void*)

#define AlignedAlloc(size) _aligned_malloc(size, SAlignment)
#define AlignedFree _aligned_free

//Lower refers to a[0] while upper refers to a[3]
//Lower opertations mean it is only performed on a[0], and a is returned in the other elements
#define SAddLower _mm_add_ss
#define SAddAll _mm_add_ps
#define SSubLower _mm_sub_ss
#define SSubAll _mm_sub_ps
#define SMulLower _mm_mul_ss
#define SMulAll _mm_mul_ps
#define SDivLower _mm_div_ss
#define SDivAll _mm_div_ps
#define SSqrtLower _mm_sqrt_ss
#define SSqrtAll _mm_sqrt_ps
#define SInverseLower _mm_rcp_ss
#define SInverseAll _mm_rcp_ps
#define SInverseSqrtLower _mm_rsqrt_ss
#define SInverseSqrtAll _mm_rsqrt_ps
#define SMinLower _mm_min_ss
#define SMinAll _mm_min_ps
#define SMaxLower _mm_max_ss
#define SMaxAll _mm_max_ps

//Do a comparison and store all bits on for true, all off for false
#define SEqualLower _mm_cmpeq_ss
#define SEqualAll _mm_cmpeq_ps
#define SNotEqualLower _mm_cmpneq_ss
#define SNotEqualAll _mm_cmpneq_ps

#define SLessUpper _mm_cmplt_ss
#define SLessAll _mm_cmplt_ps
#define SLessEqualLower _mm_cmple_ss
#define SLessEqualAll _mm_cmple_ps

#define SGreaterLower _mm_cmpgt_ss
#define SGreaterAll _mm_cmpgt_ps
#define SGreaterEqualLower _mm_cmpge_ss
#define SGreaterEqualAll _mm_cmpge_ps

#define SIEqualLower _mm_comieq_ss
#define SILessLower _mm_comilt_ss
#define SILessEqualLower _mm_comile_ss
#define SIGreaterLower _mm_comigt_ss
#define SIGreaterEqualLower _mm_comige_ss
#define SINotEqualLower _mm_comineq_ss

#define SShuffleMask(a, mask) _mm_shuffle_ps(a, a, mask)
#define SShuffle2Mask(a, b, mask) _mm_shuffle_ps(a, b, mask)
#define SMask(x, y, z, w) _MM_SHUFFLE(w, z, y, x)
//[a[x],a[y],b[z],b[w]]
#define SShuffle2(a, b, x, y, z, w) _mm_shuffle_ps(a, b, _MM_SHUFFLE(w,z,y,x))
#define SShuffle(toShuffle, x, y, z, w) SShuffle2(toShuffle, toShuffle, x, y, z, w)
//4 bit mask from high bits of each float
//sign(a3)<<3 | sign(a2)<<2 | sign(a1)<<1 | sign(a0)
#define SMoveMask _mm_movemask_ps
//[b0,a1,a2,a3]
#define SMoveLower _mm_move_ss

//[a2,b2,a3,b3]
#define SUnpackUpper _mm_unpackhi_ps
//[a0,b0,a1,b1]
#define SUnpackLower _mm_unpacklo_ps
//[a0,a1,p0,p1]
#define SLoadUpper _mm_loadh_pi
//[p0,p1,a2,a3]
#define SLoadLower _mm_loadl_pi
//[p,0,0,0]
#define SLoadLowerOne _mm_load_ss
#define SLoadAll _mm_load_ps
#define SLoadSplat _mm_load_ps1

//a[0]
#define SStoreLower _mm_store_ss
#define SStoreAll _mm_store_ps

#define SSetZero _mm_setzero_ps
#define SSetAll _mm_setr_ps
#define SSetSplat _mm_set_ps1
#define SSetLower _mm_set_ss

#define SAnd _mm_and_ps
#define SAndNot _mm_andnot_ps
#define SOr _mm_or_ps
#define SXor _mm_xor_ps

namespace Syx {
  typedef __m128 SFloats;

  inline SFloats SLoadFloats(float x, float y, float z, float w = 0.0f) {
    SAlign float store[4] = {x, y, z, w};
    return SLoadAll(store);
  }

  inline SFloats SLoadSplatFloats(float all) {
    SAlign float store = all;
    return SLoadSplat(&store);
  }

  inline SFloats SAbsAll(SFloats in) {
    SFloats negIn = SSubAll(SSetZero(), in);
    return SMaxAll(in, negIn);
  }

  inline SFloats SClampAll(SFloats min, SFloats max, SFloats value) {
    return SMaxAll(SMinAll(max, value), min);
  }

  //Select ifVec if all bits in condition are set, else select elseVec. This applies per element
  //result = condition ? ifVec : elseVec;
  inline SFloats SSelectIf(SFloats condition, SFloats ifVec, SFloats elseVec) {
    return SOr(SAnd(condition, ifVec), SAndNot(condition, elseVec));
  }

  //All Combines assume each element of the vector is equal, so it doesn't matter which index is mixed in to the result
  //[a a b b]
  inline SFloats SCombine(SFloats a, SFloats b) {
    return SShuffle2(a, b, 0, 0, 0, 0);
  }

  //[a b c c]
  inline SFloats SCombine(SFloats a, SFloats b, SFloats c) {
    SFloats ab = SCombine(a, b);
    return SShuffle2(ab, c, 0, 2, 0, 0);
  }

  //[a b c d]
  inline SFloats SCombine(SFloats a, SFloats b, SFloats c, const SFloats& d) {
    SFloats ccdd = SCombine(c, d);
    SFloats cbcd = SShuffle2(b, ccdd, 0, 0, 1, 2);
    return SMoveLower(cbcd, a);
  }

  inline SFloats SSplatIndex(SFloats v, int index) {
    switch(index) {
      case 0: return SShuffle(v, 0, 0, 0, 0); break;
      case 1: return SShuffle(v, 1, 1, 1, 1); break;
      case 2: return SShuffle(v, 2, 2, 2, 2); break;
      case 3: return SShuffle(v, 3, 3, 3, 3); break;
      default:
        SyxAssertError(false, "invalid index");
        return SFloats();
    }
  }

  //Zero out all terms but the given index
  inline SFloats SMaskOtherIndices(SFloats v, int index) {
    SAlign int mask[4] = {0};
    mask[index] = ~mask[index];
    return SAnd(v, SLoadAll(reinterpret_cast<float*>(&mask[0])));
  }
};

#endif