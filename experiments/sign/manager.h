#ifndef KS_SIGN_MANAGER_H
#define KS_SIGN_MANAGER_H

#include <memory>
#include <vector>

#include <scl/protocol/base.h>
#include <scl/simulation/config.h>
#include <scl/simulation/context.h>
#include <scl/simulation/manager.h>
#include <scl/util/sign.h>

#include "presig.h"
#include "signer.h"
#include "util.h"

/**
 * @brief Manager object for the Sign protocol.
 */
class Manager final : public scl::sim::ManagerWithOutputToStream {
 public:
  /**
   * @brief Construct a new manager for the sign protocol.
   * @param message the message to sign.
   * @param n the number of parties that will run the sign protocol.
   * @param replications the number of replications to run.
   */
  Manager(std::ostream& stream,
          Digest message,
          std::size_t number_of_parties,
          bool simple,
          bool check)
      : scl::sim::ManagerWithOutputToStream(stream),
        m_message(message),
        m_number_of_parties(number_of_parties),
        m_simple(simple),
        m_check(check) {}

  /**
   * @brief Construct an instance of the Sign protocol for simulation.
   *
   * This will return a list of <code>n + 1</code> protocol objects. The last
   * protocol is a <code>nullptr</code>, as it represents the "blockchain" and
   * because this is not needed in this protocol. Each of the remaining
   * <code>n</code> protocol objects will be of type \ref Signer.
   */
  std::vector<std::unique_ptr<scl::proto::Protocol>> protocol() override;

  /**
   * @brief Checks that a valid signature was output.
   *
   * Each party produces a single signature on the message that this Manager was
   * created with. This method will check that the resulting signature is valid
   * under a dummy public key that was generated as part of the Protocol method.
   */
  void handleProtocolOutput(std::size_t party_id,
                            const std::any& output) override;

 private:
  Digest m_message;
  std::size_t m_number_of_parties;
  bool m_simple;
  bool m_check;

  Curve m_pk;
};

#endif  // KS_SIGN_MANAGER_H
