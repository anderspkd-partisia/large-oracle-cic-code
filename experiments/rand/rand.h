#ifndef KS_RAND_H
#define KS_RAND_H

#include <scl/coro/coroutine.h>
#include <scl/protocol/base.h>
#include <scl/protocol/result.h>

#include "util.h"

/**
 * @brief Protocol 4, Share
 */
class Protocol final : public scl::proto::Protocol {
 public:
  Protocol(std::size_t number_of_rands) : m_number_of_rands(number_of_rands) {}

  scl::coro::Task<scl::proto::ProtocolResult> run(
      scl::proto::Env& env) const override;

  std::string name() const override {
    return "share";
  }

 private:
  std::size_t m_number_of_rands;
};

/**
 * @brief Protocol 4, Extract.
 */
class Extract final : public scl::proto::Protocol {
 public:
  Extract(std::vector<PVSS>&& shares)
      : m_shares(std::move(shares)) {}

  scl::coro::Task<scl::proto::ProtocolResult> run(
      scl::proto::Env& env) const override;

  std::string name() const override {
    return "extract";
  }

 private:
  std::vector<PVSS> m_shares;
};

/**
 * @brief Protocol 5, Complain.
 */
class Agree final : public scl::proto::Protocol {
 public:
  Agree(std::size_t batches) : m_batches(batches) {}

  scl::coro::Task<scl::proto::ProtocolResult> run(
      scl::proto::Env& env) const override;

  std::string name() const override {
    return "complain";
  }

 private:
  std::size_t m_batches;
};

/**
 * @brief Protocol 5, step 1 of Fill.
 */
class Fill final : public scl::proto::Protocol {
 public:
  Fill(std::vector<std::optional<std::vector<PVSS>>>&& shares)
      : m_shares(std::move(shares)) {}

  scl::coro::Task<scl::proto::ProtocolResult> run(
      scl::proto::Env& env) const override;

  std::string name() const override {
    return "fill";
  }

 private:
  // marked as mutable to allow them to be moved from this protocol to the next.
  mutable std::vector<std::optional<std::vector<PVSS>>> m_shares;
};

using AccusorMap = std::unordered_map<std::size_t, std::vector<std::size_t>>;

/**
 * @brief Protocol 5, step 2 of Fill.
 */
class FillExchange final : public scl::proto::Protocol {
 public:
  FillExchange(
      std::vector<std::optional<std::vector<PVSS>>>&& shares,
      AccusorMap&& accuser_map,
      scl::util::Bitmap&& malicious)
      : m_shares(std::move(shares)),
        m_acc_map(std::move(accuser_map)),
        m_malicious(std::move(malicious)) {}

  scl::coro::Task<scl::proto::ProtocolResult> run(
      scl::proto::Env& env) const override;

  std::string name() const override {
    return "fill-exchange";
  }

 private:
  mutable std::vector<std::optional<std::vector<PVSS>>> m_shares;
  AccusorMap m_acc_map;
  scl::util::Bitmap m_malicious;
};

/**
 * @brief Protocol 5, Finalize step.
 */
class Finalize final : public scl::proto::Protocol {
 public:
  Finalize(std::vector<std::optional<std::vector<PVSS>>>&& shares)
      : m_shares(shares) {}

  scl::coro::Task<scl::proto::ProtocolResult> run(
      scl::proto::Env& env) const override;

  std::string name() const override {
    return "finalize";
  };

 private:
  std::vector<std::optional<std::vector<PVSS>>> m_shares;
};

#endif  // KS_RAND_H
