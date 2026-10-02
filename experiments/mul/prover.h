#ifndef KS_MUL_PROVER_H
#define KS_MUL_PROVER_H

#include "apocm.h"
#include "timer.h"

using ProofAndFs = std::pair<ApocmProof, std::vector<Field>>;

/**
 * @brief Create a partial proof that is independent of the receiver.
 * @param stmt the proof statement.
 * @param witness the proof witness.
 * @param us a list of shares.
 * @param prg a random generator for randomness.
 * @return A proof and list of randomness needed to complete the proof.
 */
ProofAndFs createReceiverIndependentProof(
    const ApocmStmt& stmt,
    const ApocmWitness& witness,
    const std::vector<Field>& us,
    scl::util::PRG& prg,
    std::shared_ptr<ProofTimer> pt = nullptr);

/**
 * @brief Create a receiver dependent proof. This completes the ZK proof.
 * @param Qs commitments from createReceiverIndependetProof.
 * @param us a list of shares.
 * @param fs commitments output from createReceiverIndependetProof.
 * @param index the of the share to prove something about.
 * @param prg a random generator for randomness.
 * @return a small proof of correct multiplication.
 */
PLinProof createReceiverDependentProof(
    const std::vector<Curve>& Qs,
    const std::vector<Field>& us,
    const std::vector<Field>& fs,
    std::size_t index,
    scl::util::PRG& prg,
    std::shared_ptr<ProofTimer> pt = nullptr);

/**
 * @brief Prover interface.
 */
struct Prover {
  /**
   * @brief Create the independent proof.
   */
  virtual scl::coro::Task<ProofAndFs> initialize(const ApocmStmt& stmt,
                                                 const ApocmWitness& witness,
                                                 const std::vector<Field>& us,
                                                 scl::util::PRG& prg) = 0;

  /**
   * @brief Create the dependent proof.
   */
  virtual scl::coro::Task<PLinProof> run(const std::vector<Curve>& Qs,
                                         const std::vector<Field>& us,
                                         const std::vector<Field>& fs,
                                         std::size_t index,
                                         scl::util::PRG& prg) = 0;
};

#endif  // KS_MUL_PROVER_H
