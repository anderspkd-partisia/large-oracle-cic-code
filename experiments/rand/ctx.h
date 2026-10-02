#ifndef KS_RAND_CTX_H
#define KS_RAND_CTX_H

#include <unordered_map>

#include <scl/math/matrix.h>

#include "util.h"

class Context {
 public:
  static void init(std::size_t number_of_parites, std::size_t number_of_rands);

  static const scl::math::Matrix<Field>& HIM() {
    return m_him;
  }

  static std::size_t batchesRequired() {
    return m_batches_required;
  }

  static void storeMerkleRoots(std::size_t id,
                               std::size_t rid,
                               const Hash::DigestType& root) {
    m_roots[id][rid] = root;
  }

  static Hash::DigestType getMerkleRoot(std::size_t id, std::size_t rid) {
    return m_roots[id][rid];
  }

 private:
  Context() {}

  static scl::math::Matrix<Field> m_him;
  static std::size_t m_batches_required;
  static std::vector<std::vector<Hash::DigestType>> m_roots;
};

#endif  // KS_RAND_CTX_H
