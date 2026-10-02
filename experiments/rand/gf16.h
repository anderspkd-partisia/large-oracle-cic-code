#ifndef KS_RAND_GF16_H
#define KS_RAND_GF16_H

#include <scl/math/fields/ff_ops.h>

/**
 * @brief GF(2^16) field. Used for error correcting.
 *
 * The reason for using a 2-byte galois field is that the number of corrupt
 * parties in simulation may exceed 255. In that case using GF(2^8) would not
 * suffice.
 */
struct GF2_16 {
  using ValueType = std::array<unsigned char, 2>;
  constexpr static const char* NAME = "GF(2^16)";
  constexpr static const std::size_t BYTE_SIZE = 2;
  constexpr static const std::size_t BIT_SIZE = 16;
};

#endif  // KS_RAND_GF16_H
