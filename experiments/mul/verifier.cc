#include "verifier.h"

#include <numeric>

#include "apocm.h"

// Define this symbol to enable an alternative way of computing the
// recursive step in the verification of the e-lin proof.

#define ELIN_REC_ALT

using namespace scl;

namespace {

#ifdef ELIN_REC_ALT

std::vector<Field> computeChallenges(const ELinProof& proof,
                                     const Field& seed) {
  const auto n = proof.As.size();
  std::vector<Field> challenges;
  challenges.reserve(n);

  for (std::size_t i = 0; i < n; i++) {
    challenges.emplace_back(
        computeELinChallenge(seed, proof.As[i], proof.Bs[i]));
  }

  return challenges;
}

struct ScalarAndIdx {
  static ScalarAndIdx create(std::size_t idx) {
    return ScalarAndIdx{Field::one(), idx};
  }

  static std::vector<ScalarAndIdx> dummy() {
    std::vector<ScalarAndIdx> x;
    x.emplace_back(ScalarAndIdx{Field::zero(), std::size_t(-1)});
    return x;
  }

  bool isDummy() const {
    return idx == (std::size_t)-1;
  }

  Field scalar;
  std::size_t idx;
};

void mulChallenge(std::vector<std::vector<ScalarAndIdx>>& sidx,
                  const Field& c) {
  const std::size_t half = sidx.size() / 2;
  for (std::size_t i = 0; i < half; i++) {
    for (auto& si : sidx[i]) {
      si.scalar *= c;
    }
  }
}

std::pair<std::array<Curve, 2>, std::array<Field, 2>> computeFolding(
    const std::vector<Field>& challenges,
    const std::vector<Curve>& Gbar,
    const std::vector<Field>& rhobar) {
  // construct a n-by-n matrix here with 1s on the diagonal and 0s
  // everywhere else. For each iteration, split the matrix in half
  // along its rows and add the top matrix to the bottom one,
  // multiplying the current challenge unto the top part.

  const auto n = Gbar.size();

  std::vector<std::vector<ScalarAndIdx>> idx;
  idx.reserve(n);
  for (std::size_t i = 0; i < n; i++) {
    std::vector<ScalarAndIdx> ix;
    ix.emplace_back(ScalarAndIdx::create(i));
    idx.emplace_back(ix);
  }

  auto cptr = challenges.begin();

  while (idx.size() > 2) {
    // makes life easier
    if (idx.size() % 2 == 1) {
      idx.emplace_back(ScalarAndIdx::dummy());
    }

    mulChallenge(idx, *cptr++);

    const std::size_t half = idx.size() / 2;
    for (std::size_t i = 0; i < half; i++) {
      idx[i].insert(idx[i].end(), idx[half + i].begin(), idx[half + i].end());
    }

    idx.resize(half);
  }

  std::array<Curve, 2> G;
  std::array<Field, 2> r;

  for (const auto& is : idx[0]) {
    if (!is.isDummy()) {
      G[0] += Gbar[is.idx] * is.scalar;
      r[0] += rhobar[is.idx] * is.scalar;
    }
  }

  for (const auto& is : idx[1]) {
    if (!is.isDummy()) {
      G[1] += Gbar[is.idx] * is.scalar;
      r[1] += rhobar[is.idx] * is.scalar;
    }
  }

  return {G, r};
}

#endif

bool verifyELinProof(const ELinProof& proof,
                     const Field& seed,
                     Curve& Pbar,
                     std::vector<Curve>& Gbar,
                     std::vector<Field>& rhobar) {
  std::size_t p = 0;

#ifndef ELIN_REC_ALT

  while (rhobar.size() > 2) {
    ensureEvenSize(Gbar);
    ensureEvenSize(rhobar);

    const auto A = proof.As[p];
    const auto B = proof.Bs[p];

    const auto c = computeELinChallenge(seed, A, B);

    const std::size_t half = rhobar.size() / 2;
    for (std::size_t i = 0; i < half; i++) {
      Gbar[i] = c * Gbar[i] + Gbar[half + i];
      rhobar[i] = c * rhobar[i] + rhobar[half + i];
    }

    Gbar.resize(half);
    rhobar.resize(half);

    Pbar = A + c * (Pbar + c * B);
    p++;
  }

  if (Gbar.size() != 2 && rhobar.size() != 2 && proof.x.size() != 2) {
    throw std::logic_error("something went wrong");
  }

  // lhs = <x', G'> + L'(x')K
  const auto lhs = (proof.x[0] * Gbar[0] + proof.x[1] * Gbar[1]) +
                   (rhobar[0] * proof.x[0] + rhobar[1] * proof.x[1]) * paramK();

#else

  const auto challs = computeChallenges(proof, seed);
  const auto [G, r] = computeFolding(challs, Gbar, rhobar);

  for (const auto& c : challs) {
    const auto& A = proof.As[p];
    const auto& B = proof.Bs[p];
    Pbar = A + c * (Pbar + c * B);
    p++;
  }

  const auto lhs = (proof.x[0] * G[0] + proof.x[1] * G[1]) +
                   (r[0] * proof.x[0] + r[1] * proof.x[1]) * paramK();
#endif

  return lhs == Pbar;
}

}  // namespace

bool verifyPLinProof(const PLinStmt& stmt, const PLinProof& proof) {
  const auto [elin_proof, F, t] = proof;
  const auto [c0, c1] = computePLinChallenge(stmt, t, F);

  auto Pbar = computePBar(F, c0, c1, stmt.P, stmt.y, t);
  auto Gbar = computeGBar(stmt.rho_v.size());

  auto rhobar = computeRhoBar(stmt.rho_v, c1);

  return verifyELinProof(elin_proof, c0, Pbar, Gbar, rhobar);
}

bool verifyApocmProof(const ApocmStmt& stmt,
                      const ApocmProof& iproof,
                      const PLinProof& dproof,
                      const Field& u,
                      std::size_t index,
                      std::shared_ptr<ProofTimer> pt) {
  PT_START(pt, "verify");

  const auto t = Context::threshold();
  const auto psi = Context::psi();

  const auto rho = computeApocmChallenge(stmt, iproof.P, iproof.R, iproof.Qs);
  const auto delta =
      computeApocmChallenge(iproof.ya, iproof.yb, iproof.yc, rho);
  const auto zeta = computeApocmChallenge(iproof.yalphas, iproof.ybeta, delta);

  if (iproof.yc != iproof.ya * iproof.yb) {
    std::cout << "yc != ya*yb\n";
    return false;
  }

  const auto deltas = computeDeltas(delta);
  const auto y = computeY(iproof.ya,
                          iproof.yb,
                          iproof.yc,
                          iproof.yalphas,
                          iproof.ybeta,
                          zeta);

  const auto rho_vec = computeRhoVec(rho, deltas, zeta);
  if (!verifyPLinProof({rho_vec, iproof.P, y}, iproof.plin_proof)) {
    std::cout << "invalid plin proof\n";
    return false;
  }

  // verify the receiver specific part

  Curve lhs = iproof.R;
  for (std::size_t i = 0; i < t; i++) {
    lhs += stmt.A[i] * deltas[i];
    lhs += stmt.B[i] * deltas[t + i];
    lhs += stmt.R[i] * deltas[2 * t + i];
  }

  for (std::size_t i = 0; i < psi; i++) {
    lhs += iproof.Qs[i] * deltas[3 * t + i];
  }

  Curve rhs = iproof.ybeta * pedersenH();
  rhs += math::innerProd<Curve>(iproof.yalphas.begin(),
                                iproof.yalphas.end(),
                                Context::generators().begin());

  if (lhs != rhs) {
    std::cout << "lhs != rhs\n";
    return false;
  }

  const auto h = index / Context::mu();
  const auto rho_vec1 = computeRhoVec(index);
  if (!verifyPLinProof({rho_vec1, iproof.Qs[h], u}, dproof)) {
    std::cout << "invalid plin proof for my share\n";
    return false;
  }

  PT_END(pt, "verify");

  return true;
}
