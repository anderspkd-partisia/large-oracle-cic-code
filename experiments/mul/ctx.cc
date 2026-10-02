#include "ctx.h"

#include <unordered_map>

#include <scl/math/lagrange.h>

#include "util.h"

using namespace scl;

std::size_t Context::m_number_of_parties = 0;
std::size_t Context::m_threshold = 0;
std::vector<Curve> Context::m_generators = {};
std::vector<Field> Context::m_thetas = {};
std::vector<std::vector<Field>> Context::m_pow_thetas = {};
std::unordered_map<std::size_t, math::Vector<Field>> Context::m_lagrange_basis;
std::size_t Context::m_mu = 0;
std::size_t Context::m_psi = 0;

namespace {

std::vector<Curve> initGenerators(std::size_t t,
                                  std::size_t n,
                                  std::size_t mu,
                                  std::size_t psi) {
  const auto N = 8 * t + n + psi + mu + 4;
  std::vector<Curve> generators;

  generators.reserve(n);

  // generators = {2G, 3G, ..., (n + 2)G}. Not secure, but good enough for
  // government work.
  generators.emplace_back(Curve::generator());
  generators.back().normalize();
  for (std::size_t i = 1; i < N; i++) {
    generators.emplace_back(generators.back() + Curve::generator());
    generators.back().normalize();
  }

  return generators;
}

std::vector<Field> initThetas(std::size_t n) {
  std::vector<Field> thetas;
  thetas.reserve(n);
  for (std::size_t i = 0; i < n; i++) {
    thetas.emplace_back(-((int)i + 1));
  }
  return thetas;
}

std::vector<std::vector<Field>> initPowThetas(const std::vector<Field>& thetas,
                                              std::size_t t) {
  const auto n = thetas.size();
  std::vector<std::vector<Field>> powt;
  powt.reserve(thetas.size());

  for (std::size_t i = 0; i < n; i++) {
    std::vector<Field> thts;
    thts.reserve(t);
    thts.emplace_back(thetas[i]);

    for (std::size_t j = 1; j < t; j++) {
      thts.emplace_back(thts.back() * thetas[i]);
    }

    powt.emplace_back(thts);
  }

  return powt;
}

std::unordered_map<std::size_t, math::Vector<Field>> initLagrangeBasis(
    std::size_t t) {
  std::unordered_map<std::size_t, math::Vector<Field>> lgb;
  const auto ids = math::Vector<Field>::range(t + 1);
  for (std::size_t i = t + 1; i <= 2 * t; i++) {
    lgb[i] = math::computeLagrangeBasis(ids, (int)i);
  }
  return lgb;
}

}  // namespace

void Context::init(std::size_t number_of_parties, std::size_t psi) {
  if (number_of_parties % psi != 0) {
    std::cerr << "n % psi != 0";
    std::exit(1);
  }

  Context::m_number_of_parties = number_of_parties;
  Context::m_threshold = ::threshold(number_of_parties);

  Context::m_psi = psi;
  Context::m_mu = number_of_parties / psi;

  const auto t = Context::m_threshold;
  const auto n = Context::m_number_of_parties;
  const auto mu = Context::m_mu;

  Context::m_generators = initGenerators(t, n, mu, psi);

  Context::m_thetas = initThetas(n);
  Context::m_pow_thetas = initPowThetas(Context::m_thetas, t);
  Context::m_lagrange_basis = initLagrangeBasis(t);
}
