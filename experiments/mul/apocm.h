#ifndef KS_MUL_APOCM_H
#define KS_MUL_APOCM_H

#include <random>

#include <scl/coro/coroutine.h>
#include <scl/serialization/serializer.h>
#include <scl/ss/pedersen.h>
#include <scl/util/measurement.h>
#include <scl/util/time.h>

#include "ctx.h"
#include "util.h"

#define PROVE_IND_KEY "prove_ind"
#define PROVE_DEP_KEY "prove_dep"
#define VERIFY_KEY "verify"

/**
 * @brief Statement for a P-Lin proof.
 */
struct PLinStmt {
  std::vector<Field> rho_v;
  Curve P;
  Field y;
};

/**
 * @brief Witness for a P-Lin proof.
 */
struct PLinWitness {
  std::vector<Field> x;
  Field gamma;
};

/**
 * @brief E-Lin proof.
 */
struct ELinProof {
  std::vector<Field> x;
  std::vector<Curve> As;
  std::vector<Curve> Bs;
};

/**
 * @brief P-Lin proof.
 */
struct PLinProof {
  ELinProof elin_proof;
  Curve F;
  Field t;
};

/**
 * @brief Public part of the APOCM proof.
 */
struct ApocmStmt {
  /**
   * @brief Create a new statement for an APOCM proof.
   * @param as shares.
   * @param bs shares.
   * @param rs shares.
   * @param t the batch size.
   * @param id the id of the receiver.
   */
  template <typename IT>
  static ApocmStmt create(IT as, IT bs, IT rs, std::size_t id) {
    ApocmStmt stmt;

    stmt.id = id + 1;

    const auto t = Context::threshold();

    stmt.A.reserve(t);
    stmt.B.reserve(t);
    stmt.R.reserve(t);

    for (std::size_t i = 0; i < t; i++) {
      stmt.A.emplace_back(
          scl::ss::computeCommitmentForIndex(as->commitments, id + 1));
      stmt.B.emplace_back(
          scl::ss::computeCommitmentForIndex(bs->commitments, id + 1));
      stmt.R.emplace_back(
          scl::ss::computeCommitmentForIndex(rs->commitments, id + 1));

      as++;
      bs++;
      rs++;
    }

    return stmt;
  }

  std::size_t id;

  // commitments
  std::vector<Curve> A;
  std::vector<Curve> B;
  std::vector<Curve> R;
};

/**
 * @brief Private part of the APOCM proof.
 */
struct ApocmWitness {
  /**
   * @brief Create a new witness for an APOCM proof.
   * @param as shares.
   * @param bs shares.
   * @param rs shares.
   * @param t the batch size.
   */
  template <typename IT>
  static ApocmWitness create(IT as, IT bs, IT rs) {
    ApocmWitness witness;

    const auto t = Context::threshold();

    witness.a.reserve(t);
    witness.b.reserve(t);
    witness.r.reserve(t);
    witness.x.reserve(t);
    witness.y.reserve(t);
    witness.z.reserve(t);

    for (std::size_t i = 0; i < t; i++) {
      witness.a.emplace_back(as->getShare());
      witness.x.emplace_back(as->getRand());
      as++;

      witness.b.emplace_back(bs->getShare());
      witness.y.emplace_back(bs->getRand());
      bs++;

      witness.r.emplace_back(rs->getShare());
      witness.z.emplace_back(rs->getRand());
      rs++;
    }

    return witness;
  }

  // shares
  std::vector<Field> a;
  std::vector<Field> b;
  std::vector<Field> r;

  // commit randomness
  std::vector<Field> x;
  std::vector<Field> y;
  std::vector<Field> z;
};

/**
 * @brief An APOCM proof.
 */
struct ApocmProof {
  Curve P;
  Curve R;
  std::vector<Curve> Qs;
  Field ya;
  Field yb;
  Field yc;
  std::vector<Field> yalphas;
  Field ybeta;
  PLinProof plin_proof;
};

/**
 * @brief K parameter used in the P-Lin proofs.
 */
inline Curve paramK() {
  static Curve K = Curve::generator() * Field(-4242);
  return K;
}

/**
 * @brief Helper method to ensure that a list is of even size.
 */
template <typename F>
void ensureEvenSize(std::vector<F>& lst) {
  if (lst.size() % 2 == 1) {
    lst.emplace_back(F{});
  }
}

/**
 * @brief Compute the challenge used in the E-Lin proof.
 */
inline Field computeELinChallenge(const Field& c,
                                  const Curve& A,
                                  const Curve& B) {
  Hash h;
  h.update(c).update(A).update(B);
  return Field::read(h.finalize().data());
}

/**
 * @brief Compute the challenge used in the P-Lin proof.
 */
inline std::array<Field, 2> computePLinChallenge(const PLinStmt& stmt,
                                                 const Field& t,
                                                 const Curve& F) {
  Hash h;
  h.update(t).update(F).update(stmt.y).update(stmt.P).update(stmt.rho_v);
  auto copy = h;
  Field c0 = Field::read(copy.finalize().data());
  h.update(c0);
  return {c0, Field::read(h.finalize().data())};
}

/**
 * @brief Computes \f$\bar{P}\f$ in the P-Lin proof.
 */
inline Curve computePBar(const Curve& F,
                         const Field& c0,
                         const Field& c1,
                         const Curve& P,
                         const Field& y,
                         const Field& t) {
  return F + c0 * P + (c1 * (c0 * y + t)) * paramK();
}

/**
 * @brief Computes \f$\bar{\vec{rho}}\f$ in the P-Lin proof.
 */
std::vector<Field> computeRhoBar(const std::vector<Field>& rho_v,
                                 const Field& c1);

/**
 * @brief Computes \f$\bar{\vec{G}}\f$ in the P-Lin proof.
 */
std::vector<Curve> computeGBar(std::size_t size);

/**
 * @brief Computes a vector commitment.
 * @param x the thing to commit to.
 * @param r the randomness.
 */
template <typename IT>
Curve computeCommitment(IT begin, IT end, const Field& r) {
  Curve C =
      scl::math::innerProd<Curve>(begin, end, Context::generators().begin());
  return C + r * pedersenH();
}

/**
 * @brief Compute a challenge (rho) used in the APOCM proof.
 */
inline Field computeApocmChallenge(const ApocmStmt& stmt,
                                   const Curve& P,
                                   const Curve& R,
                                   const std::vector<Curve>& Qs) {
  Hash h;
  h.update(stmt.A).update(stmt.B).update(stmt.R);
  h.update(P).update(R).update(Qs);
  return Field::read(h.finalize().data());
}

/**
 * @brief Compute a challenge (delta) used in the APOCM proof.
 */
inline Field computeApocmChallenge(const Field& ya,
                                   const Field& yb,
                                   const Field& yc,
                                   const Field& rho) {
  Hash h;
  h.update(ya).update(yb).update(yc).update(rho);
  return Field::read(h.finalize().data());
}

/**
 * @brief Compute a challenge (zeta) used in the APOCM proof.
 */
inline Field computeApocmChallenge(const std::vector<Field>& yalphas,
                                   const Field& ybeta,
                                   const Field& delta) {
  Hash h;
  h.update(yalphas).update(ybeta).update(delta);
  return Field::read(h.finalize().data());
}

/**
 * @brief Expand the delta value into a list of successive powers
 *
 * Expands \f$\delta\f$ into \f$\delta, \delta^{2}, \dots, \delta^{3t+\psi}\f$.
 */
std::vector<Field> computeDeltas(const Field& delta);

/**
 * @brief Compute the target y.
 */
Field computeY(const Field& ya,
               const Field& yb,
               const Field& yc,
               const std::vector<Field>& yalphas,
               const Field& ybeta,
               const Field& zeta);

/**
 * @brief Compute \f$\vec{rho}\f$ used to define the function L.
 */
std::vector<Field> computeRhoVec(const Field& rho,
                                 const std::vector<Field>& deltas,
                                 const Field& zeta);

/**
 * @brief Compute a much smaller \f$\vec{rho}\f$ for use in the APOCM.
 */
std::vector<Field> computeRhoVec(std::size_t index);

namespace scl::seri {

// Serializer for ApocmProof
template <>
struct Serializer<ApocmProof> {
  static std::size_t sizeOf(const ApocmProof& proof);
  static std::size_t read(ApocmProof& proof, const unsigned char* buf);
  static std::size_t write(const ApocmProof& proof, unsigned char* buf);
};

// Serializer for an E-Lin proof
template <>
struct Serializer<ELinProof> {
  static std::size_t sizeOf(const ELinProof& proof);
  static std::size_t read(ELinProof& proof, const unsigned char* buf);
  static std::size_t write(const ELinProof& proof, unsigned char* buf);
};

// Serializer for an P-Lin proof
template <>
struct Serializer<PLinProof> {
  static std::size_t sizeOf(const PLinProof& proof);
  static std::size_t read(PLinProof& proof, const unsigned char* buf);
  static std::size_t write(const PLinProof& proof, unsigned char* buf);
};

}  // namespace scl::seri

#endif  // KS_MUL_APOCM_H
