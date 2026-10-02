#ifndef KS_NETWORK_H
#define KS_NETWORK_H

#include <iostream>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <scl/protocol/base.h>
#include <scl/protocol/protocol.h>
#include <scl/protocol/result.h>
#include <scl/simulation/context.h>
#include <scl/simulation/hook.h>

#include "util.h"

/**
 * @brief Decorator for net::Network that adds a "broadcast" channel.
 *
 * This takes a network of size <code>n</code> and turns it into a
 * network of size <code>n-1</code> where the <code>n</code>'th party
 * acts as the "broadcast" channel.
 */
class NetworkWithBroadcast {
 public:
  NetworkWithBroadcast(scl::net::Network& network) : m_network(network) {}

  std::size_t size() const {
    return m_network.size() - 1;
  }

  std::size_t myId() const {
    return m_network.myId();
  }

  scl::coro::Task<void> broadcast(const scl::net::Packet& packet) {
    co_await m_network.party(size())->send(packet);
  }

  scl::coro::Task<scl::net::Packet> broadcast() {
    co_return co_await m_network.party(size())->recv();
  }

  scl::net::Channel* party(std::size_t id) {
    return m_network.party(id);
  }

  scl::coro::Task<void> send(const scl::net::Packet& packet) {
    for (std::size_t i = 0; i < size(); i++) {
      co_await m_network.party(i)->send(packet);
    }
  }

  scl::coro::Task<std::vector<std::optional<scl::net::Packet>>> recv(
      std::size_t t) {
    std::vector<scl::coro::Task<scl::net::Packet>> recvs;
    recvs.reserve(size());
    for (std::size_t i = 0; i < size(); i++) {
      recvs.emplace_back(m_network.party(i)->recv());
    }
    co_return co_await scl::coro::batch(std::move(recvs), t);
  }

  scl::coro::Task<std::vector<scl::net::Packet>> recv() {
    std::vector<scl::coro::Task<scl::net::Packet>> recvs;
    recvs.reserve(size());
    for (std::size_t i = 0; i < size(); i++) {
      recvs.emplace_back(m_network.party(i)->recv());
    }
    co_return co_await scl::coro::batch(std::move(recvs));
  }

 private:
  scl::net::Network& m_network;
};

/**
 * @brief Hook which cancels the broadcast party when all other parties have
 * finished running.
 */
struct CancelBcHook final : public scl::sim::Hook {
 public:
  CancelBcHook(std::size_t n) : m_n(n) {}
  void run(std::size_t /* ignored */,
           const scl::sim::SimulationContext& ctx) override {
    // if all parties have stopped running, then we stop the n'th
    // party, which is our broadcast channel.
    for (std::size_t i = 0; i < m_n; i++) {
      if (!ctx.dead(i)) {
        return;
      }
    }

    ctx.cancel(m_n);
  }

 private:
  std::size_t m_n;
};

/**
 * @brief Dummy broadcast protocol.
 */
class Broadcast final : public scl::proto::Protocol {
 public:
  /**
   * @brief Create a new broadcast protocol.
   * @param broadcast_delay the delay induced by the broadcast protocol.
   * @param timeout the timeout of a broadcast round.
   * @param number_of_parties the number of parties.
   */
  static std::unique_ptr<scl::proto::Protocol> create(
      scl::util::Time::Duration timeout,
      std::size_t number_of_parties) {
    using namespace std::chrono_literals;
    return std::make_unique<Broadcast>(timeout,
                                       scl::util::Bitmap(number_of_parties));
  }

  Broadcast(scl::util::Time::Duration timeout, scl::util::Bitmap ignore_set)
      : m_timeout(timeout), m_ignore_set(std::move(ignore_set)) {}

  scl::coro::Task<scl::proto::ProtocolResult> run(
      scl::proto::Env& env) const override;

  std::string name() const override {
    return "bc";
  }

 private:
  scl::util::Time::Duration m_timeout;
  scl::util::Bitmap m_ignore_set;
};

using Message = std::optional<scl::net::Packet>;

class BroadcastSend final : public scl::proto::Protocol {
 public:
  BroadcastSend(scl::util::Time::Duration timeout,
                scl::util::Bitmap ignore_set,
                std::vector<Message>&& messages_to_send)
      : m_timeout(timeout),
        m_ignore_set(std::move(ignore_set)),
        m_messages(std::move(messages_to_send)) {}

  scl::coro::Task<scl::proto::ProtocolResult> run(
      scl::proto::Env& env) const override;

  std::string name() const override {
    return "bc_send";
  }

 private:
  scl::util::Time::Duration m_timeout;
  scl::util::Bitmap m_ignore_set;

  std::vector<Message> m_messages;
};

#endif  // KS_NETWORK_H
