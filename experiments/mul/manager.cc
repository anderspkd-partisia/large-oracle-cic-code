#include "manager.h"

#include <stdexcept>

#include <scl/coro/task.h>
#include <scl/math/array.h>
#include <scl/protocol/base.h>
#include <scl/ss/shamir.h>
#include <scl/util/measurement.h>
#include <scl/util/prg.h>
#include <scl/util/time.h>

#include "apocm.h"
#include "delay_sampler.h"
#include "protocol.h"
#include "prover.h"
#include "rands.h"
#include "timer.h"
#include "util.h"
#include "verifier.h"

using namespace scl;

// the number of times to iterate the proof creation/verification when
// measuring them
#define MAX_IT 20

namespace {

std::vector<Field> computeUs(const std::vector<PVSS>& as,
                             const std::vector<PVSS>& bs,
                             const std::vector<PVSS>& r2s,
                             std::size_t n,
                             std::size_t t) {
  // only care about one batch here.
  std::vector<Field> w2s;
  for (std::size_t i = 0; i < t; i++) {
    w2s.emplace_back(as[i].getShare() * bs[i].getShare() - r2s[i].getShare());
  }

  std::vector<Field> us;
  for (std::size_t i = 0; i < n; i++) {
    const auto theta = Context::theta(i);
    Field u;
    for (std::size_t j = t; j-- > 0;) {
      u = (u + w2s[j]) * theta;
    }

    us.emplace_back(u);
  }

  return us;
}

/**
 * @brief Prover which simply sleeps a bit then returns a static proof.
 */
class FakeProver final : public Prover {
 public:
  FakeProver(DelaySampler sampler_ind,
             DelaySampler sampler_dep,
             std::shared_ptr<ProofAndFs> iproof,
             std::shared_ptr<PLinProof> dproof)
      : m_sampler_ind(sampler_ind),
        m_sampler_dep(sampler_dep),
        m_iproof(iproof),
        m_dproof(dproof) {}

  coro::Task<ProofAndFs> initialize(const ApocmStmt& /* unused */,
                                    const ApocmWitness& /* unused */,
                                    const std::vector<Field>& /* unused */,
                                    scl::util::PRG& /* unused */) override {
    co_await m_sampler_ind.get();
    co_return *m_iproof;
  }

  coro::Task<PLinProof> run(const std::vector<Curve>& /* unused */,
                            const std::vector<Field>& /* unused */,
                            const std::vector<Field>& /* unused */,
                            std::size_t /* unused */,
                            scl::util::PRG& /* unused */) override {
    co_await m_sampler_dep.get();
    co_return *m_dproof;
  }

 private:
  DelaySampler m_sampler_ind;
  DelaySampler m_sampler_dep;
  std::shared_ptr<ProofAndFs> m_iproof;
  std::shared_ptr<PLinProof> m_dproof;
};

/**
 * @brief Verifier which simply sleeps a bit then returns true/false.
 */
class FakeVerifier final : public Verifier {
 public:
  FakeVerifier(DelaySampler sampler) : m_sampler(sampler) {}

  coro::Task<bool> run(const ApocmStmt& /* unused */,
                       const ApocmProof& iproof,
                       const PLinProof& /* unused */,
                       const Field& /* unused */,
                       std::size_t /* unused */) override {
    co_await m_sampler.get();
    co_return (iproof.yc == iproof.ya * iproof.yb);
  }

 private:
  DelaySampler m_sampler;
};

// std::pair<ApocmProof, PLinProof> createApocmProof(const ApocmStmt& stmt, co)

std::array<DelaySampler, 3> createSamplers(const ApocmStmt& stmt,
                                           const ApocmWitness& witness,
                                           const std::vector<Field>& us) {
  auto pt = std::make_shared<ProofTimer>();
  auto prg = util::PRG::create("fake_zk");

  for (std::size_t i = 0; i < MAX_IT; i++) {
    auto ip = createReceiverIndependentProof(stmt, witness, us, prg, pt);
    auto dp = createReceiverDependentProof(std::get<0>(ip).Qs,
                                           us,
                                           std::get<1>(ip),
                                           0,
                                           prg,
                                           pt);
    if (!verifyApocmProof(stmt, std::get<0>(ip), dp, us[0], 0, pt)) {
      throw std::runtime_error("programmer mistake");
    }
  }

  DelaySampler ips(pt->get(PROVE_IND_KEY));
  // pt->print(PROVE_IND_KEY);

  DelaySampler dps(pt->get(PROVE_DEP_KEY));
  // pt->print(PROVE_DEP_KEY);

  DelaySampler vs(pt->get(VERIFY_KEY));
  // pt->print(VERIFY_KEY);

  return {ips, dps, vs};
}

using DummyProof =
    std::pair<std::shared_ptr<ProofAndFs>, std::shared_ptr<PLinProof>>;

DummyProof createDummyProof(const ApocmStmt& stmt,
                            const ApocmWitness& witness,
                            const std::vector<Field>& us) {
  auto prg = util::PRG::create("dummy");

  auto ip = createReceiverIndependentProof(stmt, witness, us, prg);
  auto dp = createReceiverDependentProof(std::get<0>(ip).Qs,
                                         us,
                                         std::get<1>(ip),
                                         0,
                                         prg);

  return {std::make_shared<ProofAndFs>(ip), std::make_shared<PLinProof>(dp)};
}

void setupWithFakeZK(std::vector<std::unique_ptr<proto::Protocol>>& protocols,
                     const RandBundle& inputs,
                     const DoubleRandBundle& drands,
                     std::size_t batch_count) {
  const auto n = Context::numberOfParties();
  const auto t = Context::threshold();

  const auto stmt = ApocmStmt::create(inputs.as[0].begin(),
                                      inputs.bs[0].begin(),
                                      drands.r2s[0].begin(),
                                      0);
  const auto witness = ApocmWitness::create(inputs.as[0].begin(),
                                            inputs.bs[0].begin(),
                                            drands.r2s[0].begin());
  const auto us = computeUs(inputs.as[0], inputs.bs[0], drands.r2s[0], n, t);

  std::array<DelaySampler, 3> samplers = createSamplers(stmt, witness, us);

  auto proof = createDummyProof(stmt, witness, us);

  for (std::size_t i = 0; i < n; i++) {
    auto pis = samplers[0];
    auto pds = samplers[1];
    auto vs = samplers[2];

    pis.reSeed(i);
    pds.reSeed(i);
    vs.reSeed(i);

    protocols.emplace_back(std::make_unique<MulProtocol>(
        inputs.as[i],
        inputs.bs[i],
        drands.r1s[i],
        drands.r2s[i],
        batch_count,
        std::make_unique<FakeProver>(pis,
                                     pds,
                                     std::get<0>(proof),
                                     std::get<1>(proof)),
        std::make_unique<FakeVerifier>(vs)));
  }
}

// Handle setup when -fake_zk flag is absent. In that case the prover
// and verifier simply run the real prove/verify functions.

struct RealProver final : public Prover {
  coro::Task<ProofAndFs> initialize(const ApocmStmt& stmt,
                                    const ApocmWitness& witness,
                                    const std::vector<Field>& us,
                                    scl::util::PRG& prg) override {
    co_return createReceiverIndependentProof(stmt, witness, us, prg);
  }

  coro::Task<PLinProof> run(const std::vector<Curve>& Qs,
                            const std::vector<Field>& us,
                            const std::vector<Field>& fs,
                            std::size_t index,
                            scl::util::PRG& prg) override {
    co_return createReceiverDependentProof(Qs, us, fs, index, prg);
  }
};

struct RealVerifier final : public Verifier {
  coro::Task<bool> run(const ApocmStmt& stmt,
                       const ApocmProof& iproof,
                       const PLinProof& dproof,
                       const Field& u,
                       std::size_t index) override {
    co_return verifyApocmProof(stmt, iproof, dproof, u, index);
  }
};

void setup(std::vector<std::unique_ptr<proto::Protocol>>& protocols,
           const RandBundle& inputs,
           const DoubleRandBundle& drands,
           std::size_t batch_count) {
  const auto n = Context::numberOfParties();
  for (std::size_t i = 0; i < n; i++) {
    protocols.emplace_back(
        std::make_unique<MulProtocol>(inputs.as[i],
                                      inputs.bs[i],
                                      drands.r1s[i],
                                      drands.r2s[i],
                                      batch_count,
                                      std::make_unique<RealProver>(),
                                      std::make_unique<RealVerifier>()));
  }
}

}  // namespace

std::vector<std::unique_ptr<proto::Protocol>> Manager::protocol() {
  std::vector<std::unique_ptr<proto::Protocol>> protocols;
  protocols.reserve(m_number_of_parties);
  const auto batch_count =
      batchesRequired(threshold(m_number_of_parties), m_number_of_muls);

  if (m_fake_zk) {
    setupWithFakeZK(protocols, m_inputs, m_drands, batch_count);
  } else {
    setup(protocols, m_inputs, m_drands, batch_count);
  }

  return protocols;
}

void Manager::handleProtocolOutput(std::size_t party_id,
                                   const std::any& output) {
  if (!m_check) {
    return;
  }

  const auto t = Context::threshold();
  const auto batch_count = batchesRequired(t, m_number_of_muls);
  const auto m = t * batch_count;

  if (m_outputs.empty()) {
    m_outputs.resize(m);
  }

  if (m_output_ids.size() > 2 * t) {
    return;
  }

  const auto shares = std::any_cast<std::vector<PVSS>>(output);

  if (shares.size() != m) {
    std::cout << "invalid output length from " << party_id << "\n";
    std::cout << "expected " << m << ", got " << shares.size() << "\n";
  }

  for (std::size_t i = 0; i < m; i++) {
    m_outputs[i].emplace_back(shares[i].getShare());
  }

  m_output_ids.emplace_back(party_id + 1);

  if (m_output_ids.size() > 2 * t) {
    auto alphas = math::Vector(m_output_ids);
    for (std::size_t i = 0; i < m; i++) {
      const auto w = ss::shamirRecoverD(math::Vector(m_outputs[i]),
                                        alphas,
                                        t,
                                        t,
                                        Field::zero());
      const auto e = m_inputs.as_clear[i] * m_inputs.bs_clear[i];
      if (w != e) {
        std::cout << "invalid batch: ";
        std::cout << w << " != " << e << "\n";
      } else {
        std::cout << "batch correct!\n";
      }
    }
  }
}
