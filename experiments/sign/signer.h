#ifndef KS_SIGN_SIGNER_H
#define KS_SIGN_SIGNER_H

#include <memory>

#include <scl/coro/batch.h>
#include <scl/protocol/base.h>
#include <scl/protocol/result.h>
#include <scl/util/sign.h>

#include "util.h"

/**
 * @brief Signing phase protocol.
 */
class Protocol final : public scl::proto::Protocol {
 public:
  /**
   * @brief Create a new signing protocol.
   * @param msg the message to sign.
   * @param presig the pre-signature to use.
   */
  Protocol(Digest msg, const PreSig& presig) : m_msg(msg), m_presig(presig) {}

  scl::coro::Task<scl::proto::ProtocolResult> run(
      scl::proto::Env& env) const override;

  std::string name() const override {
    return "send, recv";
  }

 private:
  Digest m_msg;
  PreSig m_presig;
};

/**
 * @brief Simplified signing protocol.
 *
 * This protocol skips the reconstruction, receiving and any sort of checks. The
 * intention is to run this for all parties except one, which runs the above
 * protocol. That makes the simulation run faster while the single party running
 * the real protocol still creates meaningful timings.
 */
class ProtocolSimple final : public scl::proto::Protocol {
 public:
  ProtocolSimple(Digest msg, const PreSig& presig)
      : m_msg(msg), m_presig(presig) {}

  scl::coro::Task<scl::proto::ProtocolResult> run(
      scl::proto::Env& env) const override;

  std::string name() const override {
    return "dummy";
  }

 private:
  Digest m_msg;
  PreSig m_presig;
};

#endif  // KS_SIGN_SIGNER_H
