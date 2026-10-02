#ifndef KS_MUL_VERIFIER_H
#define KS_MUL_VERIFIER_H

#include "apocm.h"
#include "timer.h"

/**
 * @brief Verify an APOCM proof.
 * @param stmt the statement.
 * @param iproof the receiver independent proof.
 * @param dproof the receiver dependent proof.
 * @param u the share the proof concerns.
 * @param index the index of share (i.e., the party id)
 * @return true if the proof was valid, false otherwise.
 */
bool verifyApocmProof(const ApocmStmt& stmt,
                      const ApocmProof& iproof,
                      const PLinProof& dproof,
                      const Field& u,
                      std::size_t index,
                      std::shared_ptr<ProofTimer> pt = nullptr);

/**
 * @brief Verifier interface.
 */
struct Verifier {
  /**
   * @brief Verify an APOCM proof.
   */
  virtual scl::coro::Task<bool> run(const ApocmStmt& stmt,
                                    const ApocmProof& iproof,
                                    const PLinProof& dproof,
                                    const Field& u,
                                    std::size_t index) = 0;
};

#endif  // KS_MUL_VERIFIER_H
