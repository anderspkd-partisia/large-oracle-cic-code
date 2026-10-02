#ifndef KS_MANAGER_H
#define KS_MANAGER_H

#include <scl/simulation/config.h>
#include <scl/simulation/context.h>
#include <scl/simulation/event.h>
#include <scl/simulation/hook.h>
#include <scl/simulation/manager.h>

#include "network.h"
#include "rand.h"

/**
 * @brief Manager class for the Rand protocol.
 */
class Manager final : public scl::sim::ManagerWithOutputToStream {
 public:
  Manager(std::ostream& stream,
          std::size_t number_of_parties,
          std::size_t number_of_rands,
          bool check)
      : scl::sim::ManagerWithOutputToStream(stream),
        m_number_of_parties(number_of_parties),
        m_number_of_rands(number_of_rands),
        m_check(check),
        // the broadcast party is always the (n+1)'th party.
        m_bc_party_id(number_of_parties) {}

  static std::unique_ptr<scl::sim::Manager> create(std::ostream& stream,
                                                   std::size_t n,
                                                   std::size_t m,
                                                   bool check);

  std::vector<std::unique_ptr<scl::proto::Protocol>> protocol() override;

 private:
  std::size_t m_number_of_parties;
  std::size_t m_number_of_rands;
  bool m_check;

  std::size_t m_bc_party_id;
};

#endif  // KS_MANAGER_H
