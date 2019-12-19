// Author: Martin C. Frith 2019
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MCF_SIMD_HH
#define MCF_SIMD_HH

#include <immintrin.h>

namespace mcf {

#if defined __SSE2__

static inline __m128i simdLoad128(const void *p) {
  return _mm_loadu_si128((const __m128i *)p);
}

#endif

#if defined __AVX2__

typedef __m256i SimdInt;

const int simdBytes = 32;

static inline SimdInt simdZero() {
  return _mm256_setzero_si256();
}

static inline SimdInt simdLoad(const void *p) {
  return _mm256_loadu_si256((const SimdInt *)p);
}

static inline void simdStore(void *p, SimdInt x) {
  _mm256_storeu_si256((SimdInt *)p, x);
}

static inline SimdInt simdOr(SimdInt x, SimdInt y) {
  return _mm256_or_si256(x, y);
}

static inline SimdInt simdBlend(SimdInt x, SimdInt y, SimdInt mask) {
  return _mm256_blendv_epi8(x, y, mask);
}

const int simdLen = 8;
const int simdLen2 = 16;

static inline SimdInt simdSet(int i7, int i6, int i5, int i4,
			      int i3, int i2, int i1, int i0) {
  return _mm256_set_epi32(i7, i6, i5, i4, i3, i2, i1, i0);
}

static inline SimdInt simdSet2(short iF, short iE, short iD, short iC,
			       short iB, short iA, short i9, short i8,
			       short i7, short i6, short i5, short i4,
			       short i3, short i2, short i1, short i0) {
  return _mm256_set_epi16(iF, iE, iD, iC, iB, iA, i9, i8,
			  i7, i6, i5, i4, i3, i2, i1, i0);
}

static inline SimdInt simdSet1(char jF, char jE, char jD, char jC,
			       char jB, char jA, char j9, char j8,
			       char j7, char j6, char j5, char j4,
			       char j3, char j2, char j1, char j0,
			       char iF, char iE, char iD, char iC,
			       char iB, char iA, char i9, char i8,
			       char i7, char i6, char i5, char i4,
			       char i3, char i2, char i1, char i0) {
  return _mm256_set_epi8(jF, jE, jD, jC, jB, jA, j9, j8,
			 j7, j6, j5, j4, j3, j2, j1, j0,
			 iF, iE, iD, iC, iB, iA, i9, i8,
			 i7, i6, i5, i4, i3, i2, i1, i0);
}

static inline SimdInt simdFill(int x) {
  return _mm256_set1_epi32(x);
}

static inline SimdInt simdFill2(short x) {
  return _mm256_set1_epi16(x);
}

static inline SimdInt simdFill1(char x) {
  return _mm256_set1_epi8(x);
}

static inline SimdInt simdGt(SimdInt x, SimdInt y) {
  return _mm256_cmpgt_epi32(x, y);
}

static inline SimdInt simdGt2(SimdInt x, SimdInt y) {
  return _mm256_cmpgt_epi16(x, y);
}

static inline SimdInt simdGt1(SimdInt x, SimdInt y) {
  return _mm256_cmpgt_epi8(x, y);
}

static inline SimdInt simdAdd(SimdInt x, SimdInt y) {
  return _mm256_add_epi32(x, y);
}

static inline SimdInt simdAdd2(SimdInt x, SimdInt y) {
  return _mm256_add_epi16(x, y);
}

static inline SimdInt simdAdd1(SimdInt x, SimdInt y) {
  return _mm256_add_epi8(x, y);
}

static inline SimdInt simdSub(SimdInt x, SimdInt y) {
  return _mm256_sub_epi32(x, y);
}

static inline SimdInt simdSub2(SimdInt x, SimdInt y) {
  return _mm256_sub_epi16(x, y);
}

static inline SimdInt simdSubs1(SimdInt x, SimdInt y) {
  return _mm256_subs_epi8(x, y);
}

static inline SimdInt simdLeft(SimdInt x, int bits) {
  return _mm256_slli_epi32(x, bits);
}

static inline SimdInt simdMax(SimdInt x, SimdInt y) {
  return _mm256_max_epi32(x, y);
}

static inline SimdInt simdMax2(SimdInt x, SimdInt y) {
  return _mm256_max_epi16(x, y);
}

static inline SimdInt simdMax1(SimdInt x, SimdInt y) {
  return _mm256_max_epi8(x, y);
}

static inline int simdHorizontalMax(SimdInt x) {
  __m128i z = _mm256_castsi256_si128(x);
  z = _mm_max_epi32(z, _mm256_extracti128_si256(x, 1));
  z = _mm_max_epi32(z, _mm_shuffle_epi32(z, 0x4E));
  z = _mm_max_epi32(z, _mm_shuffle_epi32(z, 0xB1));
  return _mm_cvtsi128_si32(z);
}

static inline int simdHorizontalMax2(SimdInt x) {
  __m128i z = _mm256_castsi256_si128(x);
  z = _mm_max_epi16(z, _mm256_extracti128_si256(x, 1));
  z = _mm_sub_epi16(_mm_set1_epi16(32767), z);
  z = _mm_minpos_epu16(z);
  return 32767 - _mm_extract_epi16(z, 0);
}

static inline int simdHorizontalMax1(SimdInt x) {
  __m128i z = _mm256_castsi256_si128(x);
  z = _mm_max_epi8(z, _mm256_extracti128_si256(x, 1));
  z = _mm_sub_epi8(_mm_set1_epi8(127), z);
  z = _mm_min_epu8(z, _mm_srli_epi16(z, 8));
  z = _mm_minpos_epu16(z);
  return 127 - _mm_extract_epi16(z, 0);
}

static inline SimdInt simdBytesToInts(__m128i x) {
  return _mm256_cvtepi8_epi32(x);
}

static inline SimdInt simdBytesToInts2(__m128i x) {
  return _mm256_cvtepi8_epi16(x);
}

static inline SimdInt simdChoose1(SimdInt items, SimdInt choices) {
  return _mm256_shuffle_epi8(items, choices);
}

#elif defined __SSE4_1__

typedef __m128i SimdInt;

const int simdBytes = 16;

static inline SimdInt simdZero() {
  return _mm_setzero_si128();
}

static inline SimdInt simdLoad(const void *p) {
  return _mm_loadu_si128((const SimdInt *)p);
}

static inline void simdStore(void *p, SimdInt x) {
  _mm_storeu_si128((SimdInt *)p, x);
}

static inline SimdInt simdOr(SimdInt x, SimdInt y) {
  return _mm_or_si128(x, y);
}

static inline SimdInt simdBlend(SimdInt x, SimdInt y, SimdInt mask) {
  return _mm_blendv_epi8(x, y, mask);  // SSE4.1
}

const int simdLen = 4;
const int simdLen2 = 8;

static inline SimdInt simdSet(int i3, int i2, int i1, int i0) {
  return _mm_set_epi32(i3, i2, i1, i0);
}

static inline SimdInt simdSet2(short i7, short i6, short i5, short i4,
			       short i3, short i2, short i1, short i0) {
  return _mm_set_epi16(i7, i6, i5, i4, i3, i2, i1, i0);
}

static inline SimdInt simdSet1(char iF, char iE, char iD, char iC,
			       char iB, char iA, char i9, char i8,
			       char i7, char i6, char i5, char i4,
			       char i3, char i2, char i1, char i0) {
  return _mm_set_epi8(iF, iE, iD, iC, iB, iA, i9, i8,
		      i7, i6, i5, i4, i3, i2, i1, i0);
}

static inline SimdInt simdFill(int x) {
  return _mm_set1_epi32(x);
}

static inline SimdInt simdFill2(short x) {
  return _mm_set1_epi16(x);
}

static inline SimdInt simdFill1(char x) {
  return _mm_set1_epi8(x);
}

static inline SimdInt simdGt(SimdInt x, SimdInt y) {
  return _mm_cmpgt_epi32(x, y);
}

static inline SimdInt simdGt2(SimdInt x, SimdInt y) {
  return _mm_cmpgt_epi16(x, y);
}

static inline SimdInt simdGt1(SimdInt x, SimdInt y) {
  return _mm_cmpgt_epi8(x, y);
}

static inline SimdInt simdAdd(SimdInt x, SimdInt y) {
  return _mm_add_epi32(x, y);
}

static inline SimdInt simdAdd2(SimdInt x, SimdInt y) {
  return _mm_add_epi16(x, y);
}

static inline SimdInt simdAdd1(SimdInt x, SimdInt y) {
  return _mm_add_epi8(x, y);
}

static inline SimdInt simdSub(SimdInt x, SimdInt y) {
  return _mm_sub_epi32(x, y);
}

static inline SimdInt simdSub2(SimdInt x, SimdInt y) {
  return _mm_sub_epi16(x, y);
}

static inline SimdInt simdSubs1(SimdInt x, SimdInt y) {
  return _mm_subs_epi8(x, y);
}

static inline SimdInt simdLeft(SimdInt x, int bits) {
  return _mm_slli_epi32(x, bits);
}

static inline SimdInt simdMax(SimdInt x, SimdInt y) {
  return _mm_max_epi32(x, y);  // SSE4.1
}

static inline SimdInt simdMax2(SimdInt x, SimdInt y) {
  return _mm_max_epi16(x, y);
}

static inline SimdInt simdMax1(SimdInt x, SimdInt y) {
  return _mm_max_epi8(x, y);  // SSE4.1
}

static inline int simdHorizontalMax(SimdInt x) {
  x = simdMax(x, _mm_shuffle_epi32(x, 0x4E));
  x = simdMax(x, _mm_shuffle_epi32(x, 0xB1));
  return _mm_cvtsi128_si32(x);
}

static inline int simdHorizontalMax2(SimdInt x) {
  x = simdSub2(simdFill2(32767), x);
  x = _mm_minpos_epu16(x);  // SSE4.1
  return 32767 - _mm_extract_epi16(x, 0);
}

static inline int simdHorizontalMax1(SimdInt x) {
  x = _mm_sub_epi8(simdFill1(127), x);
  x = _mm_min_epu8(x, _mm_srli_epi16(x, 8));
  x = _mm_minpos_epu16(x);  // SSE4.1
  return 127 - _mm_extract_epi16(x, 0);
}

static inline SimdInt simdBytesToInts(SimdInt x) {
  return _mm_cvtepi8_epi32(x);  // SSE4.1
}

static inline SimdInt simdBytesToInts2(SimdInt x) {
  return _mm_cvtepi8_epi16(x);  // SSE4.1
}

static inline SimdInt simdChoose1(SimdInt items, SimdInt choices) {
  return _mm_shuffle_epi8(items, choices);
}

#else

typedef int SimdInt;
const int simdLen = 1;
static inline int simdSet(int x) { return x; }
static inline int simdFill(int x) { return x; }
static inline int simdLoad(const int *p) { return *p; }
static inline void simdStore(int *p, int x) { *p = x; }
static inline int simdGt(int x, int y) { return x > y; }
static inline int simdAdd(int x, int y) { return x + y; }
static inline int simdSub(int x, int y) { return x - y; }
static inline int simdMax(int x, int y) { return x > y ? x : y; }
static inline int simdBlend(int x, int y, int mask) { return mask ? y : x; }
static inline int simdHorizontalMax(int a) { return a; }

#endif

}

#endif
