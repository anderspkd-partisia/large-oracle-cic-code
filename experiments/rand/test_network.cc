#include <scl/scl.h>
#include <scl/simulation/config.h>
#include <scl/simulation/manager.h>

#include "manager.h"
#include "network.h"

using namespace scl;
using namespace std::chrono_literals;

class P final : public proto::Protocol {
 public:
  static std::unique_ptr<proto::Protocol> make(bool should_send);

  P(bool should_send) : m_should_send(should_send) {}

  coro::Task<proto::ProtocolResult> run(proto::Env& env) const override {
    NetworkWithBroadcast nw(env.network);

    if (m_should_send) {
      net::Packet pkt;
      pkt << (int)nw.myId() << 1 << 2 << 3;

      co_await 200ms;

      co_await nw.broadcast(pkt);

      auto p = co_await nw.broadcast();

      if (nw.myId() == 0) {
        int n = p.read<int>();

        std::cout << "got " << n << "messages\n";

        for (std::size_t i = 0; i < (std::size_t)n; i++) {
          auto id = p.read<int>();
          std::cout << id << " sent something\n";

          std::cout << p.read<int>() << p.read<int>() << p.read<int>()
                    << p.read<int>() << "\n";
        }
      }
    }

    co_return proto::ProtocolResult::done();
  }

 private:
  bool m_should_send;
};

std::unique_ptr<proto::Protocol> P::make(bool should_send) {
  return std::make_unique<P>(should_send);
}

class M final : public sim::ManagerWithOutputToStream {
 public:
  M() : sim::ManagerWithOutputToStream(std::cout) {}
  std::vector<std::unique_ptr<proto::Protocol>> protocol() override {
    std::vector<std::unique_ptr<proto::Protocol>> protocols;

    protocols.emplace_back(P::make(true));
    protocols.emplace_back(P::make(true));
    protocols.emplace_back(P::make(false));
    protocols.emplace_back(P::make(true));

    protocols.emplace_back(Broadcast::create(500ms, 2000ms, 4));

    return protocols;
  }
};

int main() {
  auto man = std::make_unique<M>();
  man->addHook<CancelBcHook>(sim::EventType::STOP, 4);

  sim::simulate(std::move(man));
}
