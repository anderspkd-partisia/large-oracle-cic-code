#include "manager.h"

#include <memory>

#include <scl/simulation/context.h>
#include <scl/simulation/event.h>
#include <scl/simulation/hook.h>

#include "network.h"
#include "rand.h"

using namespace scl;

std::vector<std::unique_ptr<proto::Protocol>> Manager::protocol() {
  using namespace std::chrono_literals;

  std::vector<std::unique_ptr<proto::Protocol>> protocols;
  for (std::size_t i = 0; i < m_number_of_parties; i++) {
    protocols.emplace_back(std::make_unique<Protocol>(m_number_of_rands));
  }
  protocols.emplace_back(Broadcast::create(5000ms, m_number_of_parties));

  return protocols;
}

std::unique_ptr<sim::Manager> Manager::create(std::ostream& stream,
                                              std::size_t n,
                                              std::size_t m,
                                              bool check) {
  auto man = std::make_unique<Manager>(stream, n, m, check);
  man->addHook<CancelBcHook>(sim::EventType::STOP, n);
  return man;
}
