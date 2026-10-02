#include "ctx.h"

#include <unordered_map>

#include "util.h"

using namespace scl;

math::Matrix<Field> Context::m_him;
std::size_t Context::m_batches_required;
std::vector<std::vector<Hash::DigestType>> Context::m_roots;

void Context::init(std::size_t number_of_parites, std::size_t number_of_rands) {
  const auto n = number_of_parites;
  const auto t = threshold(n);

  Context::m_him = math::Matrix<Field>::hyperInvertible(n - t, n);
  Context::m_batches_required = ::batchesRequired(n - t, number_of_rands);
  Context::m_roots.resize(n);
  for (std::size_t i = 0; i < n; i++) {
    Context::m_roots[i].resize(n);
  }
}
