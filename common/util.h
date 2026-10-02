#ifndef KS_UTIL_H
#define KS_UTIL_H

#include <cstdint>
#include <optional>
#include <type_traits>

#include <scl/math/curves/secp256k1.h>
#include <scl/math/ec.h>
#include <scl/math/ff.h>
#include <scl/math/vector.h>
#include <scl/net/channel.h>
#include <scl/net/tcp_utils.h>
#include <scl/serialization/serializer.h>
#include <scl/ss/feldman.h>
#include <scl/ss/pedersen.h>
#include <scl/util/cmdline.h>
#include <scl/util/merkle.h>
#include <scl/util/sha256.h>

using Curve = scl::math::EC<scl::math::ec::Secp256k1>;
using Field = Curve::ScalarField;

using Hash = scl::util::Sha256;
using Digest = Hash::DigestType;

template <>
struct std::hash<Digest> {
  std::size_t operator()(const Digest& d) const {
#define INT(x) static_cast<std::size_t>((x))

    std::size_t hash = INT(d[0]) |        //
                       INT(d[1]) << 8 |   //
                       INT(d[2]) << 16 |  //
                       INT(d[3]) << 24 |  //
                       INT(d[4]) << 32 |  //
                       INT(d[5]) << 40 |  //
                       INT(d[6]) << 48 |  //
                       INT(d[7]) << 56;
#undef INT
    return hash;
  }
};

using PVSS = scl::ss::PedersenShare<Curve>;

/**
 * @brief A pre-signature.
 */
struct PreSig {
  Curve R;
  PVSS a;
  PVSS b;
  PVSS c;
};

/**
 * @brief Get the corruption threshold for a number of parties.
 */
inline std::size_t threshold(std::size_t n) {
  return (n - 1) / 3;
}

/**
 * @brief Number of batches required for a particular m.
 */
inline std::size_t batchesRequired(std::size_t batch_size, std::size_t m) {
  return (m - 1) / batch_size + 1;
}

/**
 * @brief H value used in the pedersen VSS.
 */
inline Curve pedersenH() {
  static Curve h = Curve::generator() * Field(-42);
  h.normalize();
  return h;
}

#endif  // KS_UTIL_H
