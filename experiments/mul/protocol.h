#ifndef KS_MUL_H
#define KS_MUL_H

#include <memory>
#include <vector>

#include <scl/coro/coroutine.h>
#include <scl/protocol/base.h>
#include <scl/protocol/env.h>
#include <scl/protocol/result.h>

#include "prover.h"
#include "util.h"
#include "verifier.h"

/**
 * @brief Collection of inputs for the multiplication protocol.
 */
struct MulProtocolInput {
  std::vector<PVSS> as;
  std::vector<PVSS> bs;
  std::vector<PVSS> r1s;
  std::vector<PVSS> r2s;
  std::size_t batch_count;
};

/**
 * @brief Protocol for parallel multiplications.
 */
class MulProtocol final : public scl::proto::Protocol {
 public:
  /**
   * @brief Construct a new multiplication protocol.
   * @param as the [a] shares.
   * @param bs the [b] shares.
   * @param r1s random shares of degree t.
   * @param r2s random shares of degree 2t.
   * @param batch_count the number of batches to perform multiplications in.
   * @param prover ZK prover.
   */
  MulProtocol(const std::vector<PVSS>& as,
              const std::vector<PVSS>& bs,
              const std::vector<PVSS>& r1s,
              const std::vector<PVSS>& r2s,
              std::size_t batch_count,
              std::unique_ptr<Prover> prover,
              std::unique_ptr<Verifier> verifier)
      : m_input{as, bs, r1s, r2s, batch_count},
        m_prover(std::move(prover)),
        m_verifier(std::move(verifier)) {}

  scl::coro::Task<scl::proto::ProtocolResult> run(
      scl::proto::Env& env) const override;

  std::string name() const override {
    return "Batch and prove";
  }

 private:
  mutable MulProtocolInput m_input;  // passed to MulProtocolRecv
  std::unique_ptr<Prover> m_prover;
  mutable std::unique_ptr<Verifier> m_verifier;  // ditto here
};

class MulProtocolRecv final : public scl::proto::Protocol {
 public:
  MulProtocolRecv(MulProtocolInput&& input, std::unique_ptr<Verifier> verifier)
      : m_input(std::move(input)), m_verifier(std::move(verifier)) {}

  scl::coro::Task<scl::proto::ProtocolResult> run(
      scl::proto::Env& env) const override;

  std::string name() const override {
    return "Recv, verify, correct";
  }

 private:
  MulProtocolInput m_input;
  std::unique_ptr<Verifier> m_verifier;

  scl::coro::Task<std::vector<Field>> recvU(scl::net::Network& network) const;
};

#endif  // KS_MUL_H
