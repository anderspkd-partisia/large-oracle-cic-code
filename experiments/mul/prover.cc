#include "prover.h"

#include "apocm.h"

using namespace scl;

namespace {

struct AB {
  Curve A;
  Curve B;
};

AB computeAB(const std::vector<Field>& x,
             const std::vector<Curve>& G,
             const std::vector<Field>& rho_v) {
  Curve A;
  Curve B;

  if (x.size() != G.size()) {
    std::cout << x.size() << "\n";
    std::cout << G.size() << "\n";
    std::cout << "foo\n";
    throw -1;
  }

  const auto K = paramK();

  std::size_t half = x.size() / 2;
  for (std::size_t i = 0; i < half; i++) {
    A += x[i] * G[half + i] + (rho_v[half + i] * x[i]) * K;
    B += x[half + i] * G[i] + (rho_v[i] * x[half + i]) * K;
  }

  return {A, B};
}

ELinProof createELinProof(std::vector<Curve>& Gbar,
                          std::vector<Field>& xbar,
                          std::vector<Field>& rhobar,
                          Curve& Pbar,
                          const Field& seed) {
  std::vector<Curve> As;
  std::vector<Curve> Bs;

  if (xbar.size() <= 2) {
    throw std::invalid_argument("xbar.size() <= 2");
  }

  while (Gbar.size() > 2) {
    ensureEvenSize(xbar);
    ensureEvenSize(Gbar);
    ensureEvenSize(rhobar);

    const auto [A, B] = computeAB(xbar, Gbar, rhobar);
    const auto c = computeELinChallenge(seed, A, B);

    const std::size_t half = xbar.size() / 2;
    for (std::size_t i = 0; i < half; i++) {
      Gbar[i] = c * Gbar[i] + Gbar[half + i];
      xbar[i] = xbar[i] + c * xbar[half + i];
      rhobar[i] = c * rhobar[i] + rhobar[half + i];
    }

    Gbar.resize(half);
    xbar.resize(half);
    rhobar.resize(half);

    Pbar = A + c * (Pbar + c * B);

    As.emplace_back(A);
    Bs.emplace_back(B);
  }

  return {xbar, As, Bs};
}

std::vector<Field> computeXBar(const PLinWitness& witness,
                               const math::Vector<Field>& r,
                               const Field& rho,
                               const Field& c0) {
  std::vector<Field> xbar;
  xbar.reserve(r.size() + 1);
  for (std::size_t i = 0; i < witness.x.size(); i++) {
    xbar.emplace_back(witness.x[i] * c0 + r[i]);
  }
  xbar.emplace_back(witness.gamma * c0 + rho);
  return xbar;
}

}  // namespace

PLinProof createPLinProof(const PLinStmt& stmt,
                          const PLinWitness& witness,
                          util::PRG& prg,
                          std::shared_ptr<ProofTimer> pt) {
  (void)pt;

  const auto r = math::Vector<Field>::random(stmt.rho_v.size(), prg);
  const auto rho = Field::random(prg);
  const auto t = math::innerProd<Field>(r.begin(), r.end(), stmt.rho_v.begin());

  const auto F = computeCommitment(r.begin(), r.end(), rho);

  const auto [c0, c1] = computePLinChallenge(stmt, t, F);

  auto Pbar = computePBar(F, c0, c1, stmt.P, stmt.y, t);
  auto rhobar = computeRhoBar(stmt.rho_v, c1);
  auto Gbar = computeGBar(stmt.rho_v.size());
  auto xbar = computeXBar(witness, r, rho, c0);

  const auto elin_proof = createELinProof(Gbar, xbar, rhobar, Pbar, c0);

  return {elin_proof, F, t};
}

namespace {

// Computes a polynomial as represented by a list of evaluation points at some
// trivial positions.
std::vector<Field> createPolynomial(const Field& evp0,
                                    const std::vector<Field>& evps) {
  std::vector<Field> p;
  p.reserve(evps.size() + 1);
  p.emplace_back(evp0);
  for (const auto& evp : evps) {
    p.emplace_back(evp);
  }

  return p;
}

// Computes a polynomial in evaluation-point representation as the product of
// two other polynomials. The resulting polynomial has degree 2t, where t is the
// degree of the two input polynomials. The input polynomials are assumed to be
// in evaluated in the same "trivial" points.
std::vector<Field> computePolynomial(const std::vector<Field>& p0,
                                     const std::vector<Field>& p1) {
  const std::size_t t = p0.size() - 1;
  std::vector<Field> p;
  p.reserve(2 * t + 1);
  // first t + 1 points are defined as p(i) = p0(i)*p1(i)
  for (std::size_t i = 0; i < t + 1; i++) {
    p.emplace_back(p0[i] * p1[i]);
  }
  // get the next t points of p by evaluating it
  const auto ids = math::Vector<Field>::range(t + 1);
  for (std::size_t i = t + 1; i <= 2 * t; i++) {
    const auto& basis = Context::lagrangeBasisT(i);
    const auto p0i =
        math::innerProd<Field>(basis.begin(), basis.end(), p0.begin());
    const auto p1i =
        math::innerProd<Field>(basis.begin(), basis.end(), p1.begin());
    p.emplace_back(p0i * p1i);
  }

  return p;
}

// Evaluate a polynomial in evaluation point representation.
Field evalPolynomial(const std::vector<Field>& evp, const Field& x) {
  const auto ids = math::Vector<Field>::range(evp.size());
  const auto basis = math::computeLagrangeBasis(ids, x);
  return math::innerProd<Field>(basis.begin(), basis.end(), evp.begin());
}

std::vector<Field> computeXVec(const std::vector<Field>& A,
                               const std::vector<Field>& B,
                               const std::vector<Field>& C,
                               const std::vector<Field>& rs,
                               const std::vector<Field>& xs,
                               const std::vector<Field>& ys,
                               const std::vector<Field>& zs,
                               const std::vector<Field>& us,
                               const std::vector<Field>& fs,
                               const std::vector<Field>& alphas,
                               const Field& beta) {
  std::vector<Field> x_vec;
  x_vec.reserve(Context::generators().size());

#define INSERT(v)                                      \
  do {                                                 \
    x_vec.insert(x_vec.end(), (v).begin(), (v).end()); \
  } while (0)

  INSERT(A);
  INSERT(B);
  INSERT(C);
  INSERT(rs);
  INSERT(xs);
  INSERT(ys);
  INSERT(zs);
  INSERT(us);
  INSERT(fs);
  INSERT(alphas);

#undef INSERT

  x_vec.emplace_back(beta);

  // sanity check.
  if (x_vec.size() != Context::generators().size()) {
    throw std::runtime_error("invalid x_vec size");
  }

  return x_vec;
}

std::vector<Curve> computeQs(const std::vector<Field>& us,
                             const std::vector<Field>& fs) {
  const auto psi = Context::psi();
  const auto mu = Context::mu();

  std::vector<Curve> Qs;
  Qs.reserve(psi);

  for (std::size_t i = 0; i < psi; i++) {
    Curve Q = pedersenH() * fs[i];
    for (std::size_t j = 0; j < mu; j++) {
      Q += us[i * mu + j] * Context::generator(j);
    }
    Qs.emplace_back(Q);
  }

  return Qs;
}

std::vector<Field> computeYAlphas(const std::vector<Field>& alphas,
                                  const std::vector<Field>& as,
                                  const std::vector<Field>& bs,
                                  const std::vector<Field>& rs,
                                  const std::vector<Field>& us,
                                  const std::vector<Field>& deltas) {
  const auto mu = Context::mu();
  const auto psi = Context::psi();
  const auto t = Context::threshold();

  std::vector<Field> yalphas;
  yalphas.reserve(mu);

  for (std::size_t i = 0; i < mu; i++) {
    Field a = alphas[i];

    for (std::size_t j = 0; j < psi; j++) {
      a += us[mu * j + i] * deltas[3 * t + j];
    }
    yalphas.emplace_back(a);
  }

  // compute first term in y_{\alpha,i}.
  auto& ya1 = yalphas[0];
  for (std::size_t i = 0; i < t; i++) {
    ya1 +=
        deltas[i] * as[i] + deltas[t + i] * bs[i] + deltas[2 * t + i] * rs[i];
  }

  return yalphas;
}

Field computeYBeta(const std::vector<Field>& xs,
                   const std::vector<Field>& ys,
                   const std::vector<Field>& zs,
                   const std::vector<Field>& fs,
                   const std::vector<Field>& deltas,
                   const Field& beta) {
  const auto t = Context::threshold();
  const auto psi = Context::psi();

  Field ybeta = beta;
  for (std::size_t i = 0; i < t; i++) {
    ybeta +=
        deltas[i] * xs[i] + deltas[t + i] * ys[i] + deltas[2 * t + i] * zs[i];
  }
  for (std::size_t i = 0; i < psi; i++) {
    ybeta += deltas[3 * t + i] * fs[i];
  }
  return ybeta;
}

}  // namespace

std::pair<ApocmProof, std::vector<Field>> createReceiverIndependentProof(
    const ApocmStmt& stmt,
    const ApocmWitness& witness,
    const std::vector<Field>& us,
    util::PRG& prg,
    std::shared_ptr<ProofTimer> pt) {
  PT_START(pt, PROVE_IND_KEY);

  // step 2
  const auto alphas =
      math::Vector<Field>::random(Context::mu(), prg).toStlVector();
  const auto beta = Field::random(prg);
  const auto gamma = Field::random(prg);
  const auto fs =
      math::Vector<Field>::random(Context::psi(), prg).toStlVector();

  // step 3
  const auto A = createPolynomial(Field::random(prg), witness.a);
  const auto B = createPolynomial(Field::random(prg), witness.b);
  const auto C = computePolynomial(A, B);

  // step 4
  const auto x_vec = computeXVec(A,
                                 B,
                                 C,
                                 witness.r,
                                 witness.x,
                                 witness.y,
                                 witness.z,
                                 us,
                                 fs,
                                 alphas,
                                 beta);
  const auto P = computeCommitment(x_vec.begin(), x_vec.end(), gamma);
  const auto R = computeCommitment(alphas.begin(), alphas.end(), beta);
  const auto Qs = computeQs(us, fs);

  // step 5
  const auto rho = computeApocmChallenge(stmt, P, R, Qs);

  // step 6
  const auto ya = evalPolynomial(A, rho);
  const auto yb = evalPolynomial(B, rho);
  const auto yc = evalPolynomial(C, rho);

  // step 7. Instead of just computing delta = H(ya || yb || yc || rho), we
  // compute a list (delta, delta^{2}, ..., delta^{3t+psi}) here.
  const auto delta = computeApocmChallenge(ya, yb, yc, rho);
  const auto deltas = computeDeltas(delta);

  // step 8.
  const auto yalphas =
      computeYAlphas(alphas, witness.a, witness.b, witness.r, us, deltas);
  const auto ybeta =
      computeYBeta(witness.x, witness.y, witness.z, fs, deltas, beta);

  // step 9
  const auto zeta = computeApocmChallenge(yalphas, ybeta, delta);

  // step 10
  const auto rho_vec = computeRhoVec(rho, deltas, zeta);

  // step 11
  const auto y = computeY(ya, yb, yc, yalphas, ybeta, zeta);

  // step 12
  const auto plin_proof = createPLinProof(PLinStmt{rho_vec, P, y},
                                          PLinWitness{x_vec, gamma},
                                          prg,
                                          pt);

  PT_END(pt, PROVE_IND_KEY);

  return {{P, R, Qs, ya, yb, yc, yalphas, ybeta, plin_proof}, fs};
}

PLinProof createReceiverDependentProof(const std::vector<Curve>& Qs,
                                       const std::vector<Field>& us,
                                       const std::vector<Field>& fs,
                                       std::size_t index,
                                       scl::util::PRG& prg,
                                       std::shared_ptr<ProofTimer> pt) {
  PT_START(pt, PROVE_DEP_KEY);

  // step 13
  const auto mu = Context::mu();
  const auto h = index / mu;

  const auto rho_vec = computeRhoVec(index);

  std::vector<Field> x_vec;
  x_vec.reserve(mu + 1);
  for (std::size_t i = 0; i < mu; i++) {
    x_vec.emplace_back(us[h * mu + i]);
  }

  auto plin_proof =
      createPLinProof({rho_vec, Qs[h], us[index]}, {x_vec, fs[h]}, prg, pt);

  PT_END(pt, PROVE_DEP_KEY);

  return plin_proof;
}
