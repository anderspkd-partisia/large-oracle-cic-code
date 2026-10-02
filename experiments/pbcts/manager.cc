#include "manager.h"

#include <memory>

#include <scl/protocol/base.h>
#include <scl/simulation/manager.h>

#include "network.h"
#include "protocol.h"

using namespace scl;

std::vector<std::unique_ptr<proto::Protocol>> Manager::protocol() {
  using namespace std::chrono_literals;

  std::vector<std::unique_ptr<proto::Protocol>> protocols;
  protocols.reserve(m_number_of_parties + 1);
  for (std::size_t i = 0; i < m_number_of_parties; i++) {
    protocols.emplace_back(
        PBCTS::create(i, m_number_of_parties, m_number_of_psigs));
  }
  protocols.emplace_back(Broadcast::create(5000ms, m_number_of_parties));

  return protocols;
}

std::unique_ptr<sim::Manager> Manager::create(std::ostream& stream,
                                              std::size_t n,
                                              std::size_t m) {
  auto man = std::make_unique<Manager>(stream, n, m);
  man->addHook<CancelBcHook>(sim::EventType::STOP, n);
  return man;
}
