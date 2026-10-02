#ifndef KS_MUL_MANAGER_H
#define KS_MUL_MANAGER_H

#include <chrono>
#include <memory>
#include <unordered_map>

#include <scl/protocol/base.h>
#include <scl/simulation/event.h>
#include <scl/simulation/manager.h>
#include <scl/ss/shamir.h>

#include "protocol.h"
#include "rands.h"
#include "util.h"

class Manager final : public scl::sim::ManagerWithOutputToStream {
 public:
  Manager(std::ostream& stream,
          std::size_t number_of_parties,
          std::size_t number_of_muls,
          bool check,
          bool fake_zk)
      : scl::sim::ManagerWithOutputToStream(stream),
        m_number_of_parties(number_of_parties),
        m_number_of_muls(number_of_muls),
        m_check(check),
        m_fake_zk(fake_zk),
        m_inputs(RandBundle::create(number_of_parties, number_of_muls)),
        m_drands(DoubleRandBundle::create(number_of_parties, number_of_muls)) {}

  std::vector<std::unique_ptr<scl::proto::Protocol>> protocol() override;

  void handleProtocolOutput(std::size_t party_id,
                            const std::any& output) override;

 private:
  std::size_t m_number_of_parties;
  std::size_t m_number_of_muls;
  bool m_check;
  bool m_fake_zk;

  // inputs to the multiplication protocol.
  RandBundle m_inputs;

  // double shares used for multiplications
  DoubleRandBundle m_drands;

  std::vector<std::vector<Field>> m_outputs;
  std::vector<Field> m_output_ids;
};

#endif  // KS_MUL_MANAGER_H
