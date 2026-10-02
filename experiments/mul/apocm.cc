#include "apocm.h"

#include <stdexcept>

#include <scl/math/ff.h>
#include <scl/math/lagrange.h>
#include <scl/math/vector.h>
#include <scl/serialization/serializer.h>
#include <scl/ss/shamir.h>
#include <scl/util/prg.h>

#include "ctx.h"
#include "util.h"

using namespace scl;

std::vector<Field> computeRhoBar(const std::vector<Field>& rho_v,
                                 const Field& c1) {
  std::vector<Field> rhobar;
  rhobar.reserve(rho_v.size() + 1);
  for (const auto& p : rho_v) {
    rhobar.emplace_back(p * c1);
  }
  rhobar.emplace_back(Field::zero());
  return rhobar;
}

std::vector<Curve> computeGBar(std::size_t size) {
  std::vector<Curve> gbar;
  gbar.reserve(size + 1);

  std::vector<Curve>::difference_type offset =
      (std::vector<Curve>::difference_type)(size);

  gbar.insert(gbar.begin(),
              Context::generators().begin(),
              Context::generators().begin() + offset);
  gbar.emplace_back(pedersenH());

  return gbar;
}

std::vector<Field> computeDeltas(const Field& delta) {
  std::vector<Field> deltas;
  const auto m = 3 * Context::threshold() + Context::psi();
  deltas.reserve(m);

  deltas.emplace_back(delta);
  for (std::size_t i = 1; i < m; i++) {
    deltas.emplace_back(deltas.back() * delta);
  }

  return deltas;
}

Field computeY(const Field& ya,
               const Field& yb,
               const Field& yc,
               const std::vector<Field>& yalphas,
               const Field& ybeta,
               const Field& zeta) {
  Field y = ybeta;

  for (std::size_t i = yalphas.size(); i-- > 0;) {
    y = y * zeta + yalphas[i];
  }

  y *= math::exp(zeta, Context::numberOfParties());

  y = y * zeta + yc;
  y = y * zeta + yb;
  y = y * zeta + ya;

  return y;
}

namespace {

void computeRhoAB(std::vector<Field>& rho_vec,
                  const Field& zeta,
                  const Field& rho) {
  const auto t = Context::threshold();
  const auto lambda_rho =
      math::computeLagrangeBasis(math::Vector<Field>::range(t + 1), rho);

  for (std::size_t i = 0; i < t + 1; i++) {
    rho_vec[i] = lambda_rho[i];
    rho_vec[lambda_rho.size() + i] = lambda_rho[i] * zeta;
  }
}

void computeRhoC(std::vector<Field>& rho_vec,
                 const Field& zeta,
                 const Field& rho) {
  const auto t = Context::threshold();
  const auto lambda_rho =
      math::computeLagrangeBasis(math::Vector<Field>::range(2 * t + 1), rho);

  for (std::size_t i = 0; i < 2 * t + 1; i++) {
    rho_vec[2 * t + 2 + i] = lambda_rho[i] * zeta;
  }
}

void computeRhoU(std::vector<Field>& rho_vec,
                 const Field& zeta,
                 std::size_t idx) {
  const auto t = Context::threshold();
  const auto thetas = Context::thetas(idx);

  for (std::size_t i = 0; i < t; i++) {
    rho_vec[2 * t + 3 + i] += thetas[i] * zeta;
    rho_vec[4 * t + 3 + i] -= thetas[i] * zeta;
  }
  rho_vec[8 * t + 3 + idx] -= zeta;
}

void computeRhoAlpha(std::vector<Field>& rho_vec,
                     const std::vector<Field>& deltas,
                     const Field& zeta,
                     std::size_t idx) {
  const auto t = Context::threshold();
  const auto mu = Context::mu();
  const auto psi = Context::psi();

  for (std::size_t i = 0; i < psi; i++) {
    // offset: 3 + 8t + i*\mu + idx
    rho_vec[3 + 8 * t + mu * i + idx] += deltas[3 * t + i] * zeta;
  }

  const auto N = 8 * t + Context::numberOfParties() + psi + mu + 4;
  // offset: \mu - 1 - idx from the end of the vector.
  rho_vec[N - (mu - 1 - idx) - 2] = zeta;
}

void computeRhoAlpha0(std::vector<Field>& rho_vec,
                      const std::vector<Field>& deltas,
                      const Field& zeta) {
  const auto t = Context::threshold();

  for (std::size_t i = 0; i < t; i++) {
    rho_vec[1 + i] += deltas[i] * zeta;
    rho_vec[2 + t + i] += deltas[t + i] * zeta;
    rho_vec[3 + 4 * t + i] += deltas[2 * t + i] * zeta;
  }

  computeRhoAlpha(rho_vec, deltas, zeta, 0);
}

void computeRhoBeta(std::vector<Field>& rho_vec,
                    const std::vector<Field>& deltas,
                    const Field& zeta) {
  const auto t = Context::threshold();
  const auto n = Context::numberOfParties();
  const auto psi = Context::psi();

  for (std::size_t i = 0; i < 3 * t; i++) {
    rho_vec[3 + 5 * t + i] += deltas[i] * zeta;
  }

  for (std::size_t i = 0; i < psi; i++) {
    rho_vec[3 + 8 * t + n + i] += deltas[3 * t + i] * zeta;
  }

  rho_vec.back() = zeta;
}

}  // namespace

std::vector<Field> computeRhoVec(const Field& rho,
                                 const std::vector<Field>& deltas,
                                 const Field& zeta) {
  Field zta = zeta;
  std::vector<Field> rho_vec(Context::generators().size());

  // rho_vec = rho_a
  //  + zeta   * rho_b
  //  + zeta^2 * rho_c
  //  + zeta^3 * rho_{u,0} + ... + zeta^{3+n} * rho_{u,n-1}
  //  + zeta^{3+n+1} * rho_{alpha,0} + ... + zeta^{3+n+mu} * rho_{alpha,mu-1}
  //  + zeta^{3+n+mu+1} * rho_beta

  // rho_vec = rho_a + zeta * rho_b
  computeRhoAB(rho_vec, zta, rho);
  zta *= zeta;

  // rho_vec += rho_c * zeta^2
  computeRhoC(rho_vec, zta, rho);
  zta *= zeta;

  // rho_vec += zeta^3 * rho_{u,0} + ... + zeta^{3+n} * rho_{u,n-1}
  for (std::size_t i = 0; i < Context::numberOfParties(); i++) {
    computeRhoU(rho_vec, zta, i);
    zta *= zeta;
  }

  // rho_vec += zeta^{3+n+1} * rho_{alpha,0}
  computeRhoAlpha0(rho_vec, deltas, zta);
  zta *= zeta;

  // // rho_vec +=  rest of the alphas
  for (std::size_t i = 1; i < Context::mu(); i++) {
    computeRhoAlpha(rho_vec, deltas, zta, i);
    zta *= zeta;
  }

  computeRhoBeta(rho_vec, deltas, zta);

  return rho_vec;
}

std::vector<Field> computeRhoVec(std::size_t index) {
  const auto mu = Context::mu();
  const auto k = index % mu;

  std::vector<Field> rho_vec(mu);
  rho_vec[k] = Field::one();

  return rho_vec;
}

namespace {

template <typename T>
using Pack = seri::Serializer<T>;

template <typename T>
using VecPack = seri::Serializer<std::vector<T>>;

}  // namespace

std::size_t seri::Serializer<ApocmProof>::sizeOf(const ApocmProof& proof) {
  // clang-format off
  return 4 * Pack<Field>::sizeOf(Field::one())  // ya, yb, yc, beta
    + Pack<Curve>::sizeOf(proof.P)
    + Pack<Curve>::sizeOf(proof.R)
    + VecPack<Curve>::sizeOf(proof.Qs)
    + VecPack<Field>::sizeOf(proof.yalphas)
    + Pack<PLinProof>::sizeOf(proof.plin_proof);
  // clang-format on
}

std::size_t seri::Serializer<ApocmProof>::write(const ApocmProof& proof,
                                                unsigned char* buf) {
  // write in same order as defined
  buf += Pack<Curve>::write(proof.P, buf);
  buf += Pack<Curve>::write(proof.R, buf);
  buf += VecPack<Curve>::write(proof.Qs, buf);
  buf += Pack<Field>::write(proof.ya, buf);
  buf += Pack<Field>::write(proof.yb, buf);
  buf += Pack<Field>::write(proof.yc, buf);
  buf += VecPack<Field>::write(proof.yalphas, buf);
  buf += Pack<Field>::write(proof.ybeta, buf);
  buf += Pack<PLinProof>::write(proof.plin_proof, buf);

  return sizeOf(proof);
}

std::size_t seri::Serializer<ApocmProof>::read(ApocmProof& proof,
                                               const unsigned char* buf) {
  buf += Pack<Curve>::read(proof.P, buf);
  buf += Pack<Curve>::read(proof.R, buf);
  buf += VecPack<Curve>::read(proof.Qs, buf);
  buf += Pack<Field>::read(proof.ya, buf);
  buf += Pack<Field>::read(proof.yb, buf);
  buf += Pack<Field>::read(proof.yc, buf);
  buf += VecPack<Field>::read(proof.yalphas, buf);
  buf += Pack<Field>::read(proof.ybeta, buf);
  buf += Pack<PLinProof>::read(proof.plin_proof, buf);

  return sizeOf(proof);
}

std::size_t seri::Serializer<ELinProof>::sizeOf(const ELinProof& proof) {
  // clang-format off
  return VecPack<Field>::sizeOf(proof.x)
    + VecPack<Curve>::sizeOf(proof.As)
    + VecPack<Curve>::sizeOf(proof.Bs);
  // clang-format on
}

std::size_t seri::Serializer<ELinProof>::write(const ELinProof& proof,
                                               unsigned char* buf) {
  buf += VecPack<Field>::write(proof.x, buf);
  buf += VecPack<Curve>::write(proof.As, buf);
  buf += VecPack<Curve>::write(proof.Bs, buf);

  return sizeOf(proof);
}

std::size_t seri::Serializer<ELinProof>::read(ELinProof& proof,
                                              const unsigned char* buf) {
  buf += VecPack<Field>::read(proof.x, buf);
  buf += VecPack<Curve>::read(proof.As, buf);
  buf += VecPack<Curve>::read(proof.Bs, buf);

  return sizeOf(proof);
}

std::size_t seri::Serializer<PLinProof>::sizeOf(const PLinProof& proof) {
  // clang-format off
  return Pack<ELinProof>::sizeOf(proof.elin_proof)
    + Pack<Curve>::sizeOf(proof.F)
    + Pack<Field>::sizeOf(proof.t);
  // clang-format on
}

std::size_t seri::Serializer<PLinProof>::write(const PLinProof& proof,
                                               unsigned char* buf) {
  buf += Pack<ELinProof>::write(proof.elin_proof, buf);
  buf += Pack<Curve>::write(proof.F, buf);
  buf += Pack<Field>::write(proof.t, buf);

  return sizeOf(proof);
}

std::size_t seri::Serializer<PLinProof>::read(PLinProof& proof,
                                              const unsigned char* buf) {
  buf += Pack<ELinProof>::read(proof.elin_proof, buf);
  buf += Pack<Curve>::read(proof.F, buf);
  buf += Pack<Field>::read(proof.t, buf);

  return sizeOf(proof);
}
