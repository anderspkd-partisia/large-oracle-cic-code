#include "gf16.h"

#include <iostream>
#include <stdexcept>

#include <scl/math/fields/ff_ops.h>

using namespace scl;

using ElemType = std::array<unsigned char, 2>;

template <>
void math::ff::convertTo<GF2_16>(ElemType& out, int value) {
  out[0] = value & 0xFF;
  out[1] = (value >> 8) & 0xFF;
}

template <>
void math::ff::add<GF2_16>(ElemType& out, const ElemType& op) {
  out[0] ^= op[0];
  out[1] ^= op[1];
}

template <>
void math::ff::subtract<GF2_16>(ElemType& out, const ElemType& op) {
  add<GF2_16>(out, op);
}

namespace {

unsigned char mul8(unsigned char x, unsigned char y) {
  unsigned char r = 0;
  while (x != 0 && y != 0) {
    if ((y & 1) != 0) {
      r ^= x;
    }
    if ((x & 0x80) != 0) {
      x = (x << 1) ^ 0x11d;
    } else {
      x <<= 1;
    }
    y >>= 1;
  }

  return r;
}

}  // namespace

template <>
void math::ff::multiply<GF2_16>(ElemType& out, const ElemType& op) {
  // (a1 + b1X) * (a2 + b2X) == a1a2 - b1b2 + (a1b2 + b1a2)X
  const unsigned char a1 = out[0];
  const unsigned char b1 = out[1];
  out[0] = mul8(a1, op[0]) ^ mul8(b1, op[1]);
  out[1] = mul8(a1, op[1]) ^ mul8(b1, op[0]);
}

template <>
void math::ff::negate<GF2_16>(ElemType& out) {
  (void)out;
}

namespace {

unsigned char invert8(unsigned char x) {
  if (x == 0) {
    throw std::invalid_argument("0 is not invertible");
  }

  static constexpr unsigned char inv_table[] = {
      0,   1,   142, 244, 71,  167, 122, 186, 173, 157, 221, 152, 61,  170, 93,
      150, 216, 114, 192, 88,  224, 62,  76,  102, 144, 222, 85,  128, 160, 131,
      75,  42,  108, 237, 57,  81,  96,  86,  44,  138, 112, 208, 31,  74,  38,
      139, 51,  110, 72,  137, 111, 46,  164, 195, 64,  94,  80,  34,  207, 169,
      171, 12,  21,  225, 54,  95,  248, 213, 146, 78,  166, 4,   48,  136, 43,
      30,  22,  103, 69,  147, 56,  35,  104, 140, 129, 26,  37,  97,  19,  193,
      203, 99,  151, 14,  55,  65,  36,  87,  202, 91,  185, 196, 23,  77,  82,
      141, 239, 179, 32,  236, 47,  50,  40,  209, 17,  217, 233, 251, 218, 121,
      219, 119, 6,   187, 132, 205, 254, 252, 27,  84,  161, 29,  124, 204, 228,
      176, 73,  49,  39,  45,  83,  105, 2,   245, 24,  223, 68,  79,  155, 188,
      15,  92,  11,  220, 189, 148, 172, 9,   199, 162, 28,  130, 159, 198, 52,
      194, 70,  5,   206, 59,  13,  60,  156, 8,   190, 183, 135, 229, 238, 107,
      235, 242, 191, 175, 197, 100, 7,   123, 149, 154, 174, 182, 18,  89,  165,
      53,  101, 184, 163, 158, 210, 247, 98,  90,  133, 125, 168, 58,  41,  113,
      200, 246, 249, 67,  215, 214, 16,  115, 118, 120, 153, 10,  25,  145, 20,
      63,  230, 240, 134, 177, 226, 241, 250, 116, 243, 180, 109, 33,  178, 106,
      227, 231, 181, 234, 3,   143, 211, 201, 66,  212, 232, 117, 127, 255, 126,
      253};

  return inv_table[x];
}

}  // namespace

template <>
void math::ff::invert<GF2_16>(ElemType& out) {
  // (a + bX) * ((a - bX) / (a^2 + b^2))
  //   = (a + bX)(a - bX) / (a^2 + b^2)
  //   = (a^2 + b^2) / (a^2 + b^2)
  //   = 1
  //
  // So inverse is (a - bX) / (a^2 + b^2).
  const unsigned char ab = out[0] ^ out[1];
  const unsigned char denom = invert8(mul8(ab, ab));  // freshman's dream
  out[0] = mul8(out[0], denom);
  out[1] = mul8(out[1], denom);
}

template <>
bool math::ff::equal<GF2_16>(const ElemType& in1, const ElemType& in2) {
  return in1 == in2;
}

template <>
void math::ff::toBytes<GF2_16>(unsigned char* dest, const ElemType& src) {
  *dest = src[0];
  *(dest + 1) = src[1];
}

template <>
void math::ff::fromBytes<GF2_16>(ElemType& dest, const unsigned char* src) {
  dest[0] = *src;
  dest[1] = *(src + 1);
}

template <>
std::string math::ff::toString<GF2_16>(const ElemType& in) {
  const std::string as_int_str = std::to_string(in[0] | ((int)in[1] << 8));
  return "GF{" + as_int_str + "}";
}

template <>
void math::ff::convertTo<GF2_16>(ElemType& out, const std::string& src) {
  // TODO: Might not be necessary.
  out[0] = 0;
  out[1] = 0;
  (void)src;
}
