#ifndef KS_PBCTS_MANAGER_H
#define KS_PBCTS_MANAGER_H

#include <scl/simulation/config.h>
#include <scl/simulation/manager.h>

#include "network.h"

class Manager final : public scl::sim::ManagerWithOutputToStream {
 public:
  static std::unique_ptr<scl::sim::Manager> create(std::ostream& stream,
                                                   std::size_t n,
                                                   std::size_t m);
  Manager(std::ostream& stream,
          std::size_t number_of_parties,
          std::size_t number_of_psigs)
      : scl::sim::ManagerWithOutputToStream(stream),
        m_number_of_parties(number_of_parties),
        m_number_of_psigs(number_of_psigs) {}

  std::vector<std::unique_ptr<scl::proto::Protocol>> protocol() override;

 private:
  std::size_t m_number_of_parties;
  std::size_t m_number_of_psigs;
};

#endif  // KS_PBCTS_MANAGER_H
