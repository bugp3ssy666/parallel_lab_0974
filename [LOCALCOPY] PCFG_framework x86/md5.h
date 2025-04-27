#include <iostream>
#include <string>
#include <cstring>

#include <xmmintrin.h>   // SSE
#include <emmintrin.h>   // SSE2
#include <smmintrin.h>  // SSE4.1 (for _mm_blendv_epi8)

using namespace std;

// 定义了 Byte，便于使用
typedef unsigned char Byte;
// 定义了 32 比特
typedef unsigned int bit32;

typedef __m128i vec_u32; // SSE equivalent of uint32x4_t

// MD5 的一系列参数。参数是固定的，其实你不需要看懂这些
#define s11 7
#define s12 12
#define s13 17
#define s14 22
#define s21 5
#define s22 9
#define s23 14
#define s24 20
#define s31 4
#define s32 11
#define s33 16
#define s34 23
#define s41 6
#define s42 10
#define s43 15
#define s44 21

/**
 * @Basic MD5 functions.
 *
 * @param there bit32.
 *
 * @return one bit32.
 */
// 定义了一系列 MD5 中的具体函数
// 这四个计算函数是需要你进行 SIMD 并行化的
// 可以看到，FGHI 四个函数都涉及一系列位运算，在数据上是对齐的，非常容易实现 SIMD 的并行化

#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

// *SIMD 版本 >>>
#define F_SIMD(x, y, z) _mm_or_si128(_mm_and_si128(x, y), _mm_and_si128(_mm_xor_si128(x, _mm_set1_epi32(-1)), z))
#define G_SIMD(x, y, z) _mm_or_si128(_mm_and_si128(x, z), _mm_and_si128(y, _mm_xor_si128(z, _mm_set1_epi32(-1))))
#define H_SIMD(x, y, z) _mm_xor_si128(x, _mm_xor_si128(y, z))
#define I_SIMD(x, y, z) _mm_xor_si128(y, _mm_or_si128(x, _mm_xor_si128(z, _mm_set1_epi32(-1))))

/**
 * @Rotate Left.
 *
 * @param {num} the raw number.
 *
 * @param {n} rotate left n.
 *
 * @return the number after rotated left.
 */
// 定义了一系列 MD5 中的具体函数
// 这五个计算函数（ROTATELEFT/FF/GG/HH/II）和之前的 FGHI 一样，都是需要你进行 SIMD 并行化的
// 但是你需要注意的是 #define 的功能及其效果，可以发现这里的 FGHI 是没有返回值的，为什么呢？你可以查询 #define 的含义和用法
#define ROTATELEFT(num, n) (((num) << (n)) | ((num) >> (32-(n))))

#define FF(a, b, c, d, x, s, ac) { \
  (a) += F ((b), (c), (d)) + (x) + ac; \
  (a) = ROTATELEFT ((a), (s)); \
  (a) += (b); \
}

#define GG(a, b, c, d, x, s, ac) { \
  (a) += G ((b), (c), (d)) + (x) + ac; \
  (a) = ROTATELEFT ((a), (s)); \
  (a) += (b); \
}
#define HH(a, b, c, d, x, s, ac) { \
  (a) += H ((b), (c), (d)) + (x) + ac; \
  (a) = ROTATELEFT ((a), (s)); \
  (a) += (b); \
}
#define II(a, b, c, d, x, s, ac) { \
  (a) += I ((b), (c), (d)) + (x) + ac; \
  (a) = ROTATELEFT ((a), (s)); \
  (a) += (b); \
}

// *SIMD 版本 (包含掩码判断) >>>
#define ROTATELEFT_SIMD(num, n) \
    _mm_or_si128(_mm_slli_epi32(num, n), _mm_srli_epi32(num, 32 - (n)))

#define FF_SIMD(a, b, c, d, x, s, ac, mask) { \
  __m128i tmp = _mm_add_epi32(F_SIMD(b, c, d), _mm_add_epi32(x, _mm_set1_epi32(ac))); \
  tmp = _mm_add_epi32(a, tmp); \
  tmp = ROTATELEFT_SIMD(tmp, s); \
  tmp = _mm_add_epi32(tmp, b); \
  a = _mm_blendv_epi8(a, tmp, mask); \
}

#define GG_SIMD(a, b, c, d, x, s, ac, mask) { \
  __m128i tmp = _mm_add_epi32(G_SIMD(b, c, d), _mm_add_epi32(x, _mm_set1_epi32(ac))); \
  tmp = _mm_add_epi32(a, tmp); \
  tmp = ROTATELEFT_SIMD(tmp, s); \
  tmp = _mm_add_epi32(tmp, b); \
  a = _mm_blendv_epi8(a, tmp, mask); \
}

#define HH_SIMD(a, b, c, d, x, s, ac, mask) { \
  __m128i tmp = _mm_add_epi32(H_SIMD(b, c, d), _mm_add_epi32(x, _mm_set1_epi32(ac))); \
  tmp = _mm_add_epi32(a, tmp); \
  tmp = ROTATELEFT_SIMD(tmp, s); \
  tmp = _mm_add_epi32(tmp, b); \
  a = _mm_blendv_epi8(a, tmp, mask); \
}

#define II_SIMD(a, b, c, d, x, s, ac, mask) { \
  __m128i tmp = _mm_add_epi32(I_SIMD(b, c, d), _mm_add_epi32(x, _mm_set1_epi32(ac))); \
  tmp = _mm_add_epi32(a, tmp); \
  tmp = ROTATELEFT_SIMD(tmp, s); \
  tmp = _mm_add_epi32(tmp, b); \
  a = _mm_blendv_epi8(a, tmp, mask); \
}

void MD5Hash(string input, bit32 *state);

void SIMDMD5Hash(string *input, bit32 *state);