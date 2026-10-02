#ifndef KS_PBCTS_PROTOCOL_H
#define KS_PBCTS_PROTOCOL_H

#include <scl/protocol/base.h>
#include <scl/protocol/result.h>
#include <scl/scl.h>

#include "pbcts.h"

/**
 * @brief First round. Special because this round contains peer-to-peer
 * communication.
 */
class ProtocolRound1 final : public scl::proto::Protocol {
 public:
  ProtocolRound1(GoInt64 session) : m_session(session) {}

  scl::coro::Task<scl::proto::ProtocolResult> run(
      scl::proto::Env& env) const override;

  std::string name() const override {
    return "round 1&2";
  }

 private:
  GoInt64 m_session;
};

/**
 * @brief Rest of the rounds, which all only use the broadcast channel.
 */
class ProtocolRoundN final : public scl::proto::Protocol {
 public:
  ProtocolRoundN(GoInt64 session, std::size_t round)
      : m_session(session), m_round(round) {}

  scl::coro::Task<scl::proto::ProtocolResult> run(
      scl::proto::Env& env) const override;

  std::string name() const override {
    return "round " + std::to_string(m_round);
  }

 private:
  GoInt64 m_session;
  std::size_t m_round;
};

class PBCTS final {
 public:
  static std::unique_ptr<scl::proto::Protocol> create(std::size_t id,
                                                      std::size_t n,
                                                      std::size_t m);
};

#endif  // KS_PBCTS_PROTOCOL_H
