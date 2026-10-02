#include "manager.h"

#include <algorithm>
#include <numeric>
#include <random>

#include <scl/protocol/base.h>
#include <scl/simulation/event.h>

#include "presig.h"

using namespace scl;

std::vector<std::unique_ptr<proto::Protocol>> Manager::protocol() {
  const auto dummy_psigs = PreSigBundle::create(m_number_of_parties);
  m_pk = dummy_psigs.pk;

  std::vector<std::unique_ptr<scl::proto::Protocol>> parties;

  parties.emplace_back(
      std::make_unique<Protocol>(m_message, dummy_psigs.presigs[0]));

  for (std::size_t i = 1; i < m_number_of_parties; ++i) {
    if (m_simple) {
      parties.emplace_back(
          std::make_unique<ProtocolSimple>(m_message, dummy_psigs.presigs[i]));
    } else {
      parties.emplace_back(
          std::make_unique<Protocol>(m_message, dummy_psigs.presigs[i]));
    }
  }

  return parties;
}

void Manager::handleProtocolOutput(std::size_t party_id,
                                   const std::any& output) {
  if (!m_check) {
    return;
  }

  using Signature = scl::util::Signature<scl::util::ECDSA>;
  Signature sig = std::any_cast<Signature>(output);

  if (!scl::util::ECDSA::verify(m_pk, sig, m_message)) {
    std::cout << "Party " << party_id << " produced an invalid signature :(\n";
    throw std::runtime_error("invalid signature");
  }
}
