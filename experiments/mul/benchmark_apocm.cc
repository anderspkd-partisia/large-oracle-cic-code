#include <scl/scl.h>
#include <scl/serialization/serializer.h>
#include <scl/util/cmdline.h>
#include <scl/util/measurement.h>

#include "apocm.h"
#include "ctx.h"
#include "prover.h"
#include "rands.h"
#include "timer.h"
#include "verifier.h"

using namespace scl;

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

int main(int argc, char** argv) {
  const auto args =
      util::ProgramOptions::Parser("APOCM benchmark")
          .add(util::ProgramArg::required("n", "uint", "number of parties"))
          .add(util::ProgramArg::required("psi", "uint", "psi parameter"))
          .add(util::ProgramArg::optional("it", "uint", "10", "iterations"))
          .parse(argc, argv);

  const auto n = args.get<std::size_t>("n");
  const auto psi = args.get<std::size_t>("psi");
  const auto it = args.get<std::size_t>("it");

  Context::init(n, psi);

  const auto inputs = RandBundle::create(n, 1);
  const auto drands = DoubleRandBundle::create(n, 1);

  const auto t = Context::threshold();
  const std::size_t id = 0;

  const auto stmt = ApocmStmt::create(inputs.as[0].begin(),
                                      inputs.bs[0].begin(),
                                      drands.r2s[0].begin(),
                                      id);
  const auto witness = ApocmWitness::create(inputs.as[0].begin(),
                                            inputs.bs[0].begin(),
                                            drands.r2s[0].begin());
  const auto us = computeUs(inputs.as[0], inputs.bs[0], drands.r2s[0], n, t);

  auto prg = util::PRG::create("bench_apocm");

  auto pt = std::make_shared<ProofTimer>();

  std::pair<ApocmProof, std::vector<Field>> proof;

  for (std::size_t i = 0; i < it; i++) {
    proof = createReceiverIndependentProof(stmt, witness, us, prg, pt);
  }

  const auto p_all = std::get<0>(proof);
  const auto fs = std::get<1>(proof);
  const auto p_all_sz = seri::Serializer<ApocmProof>::sizeOf(p_all);

  std::cout << "receiver independent proof size: ";
  std::cout << p_all_sz << " bytes\n";

  PLinProof proof1;

  for (std::size_t i = 0; i < it; i++) {
    proof1 = createReceiverDependentProof(p_all.Qs, us, fs, id, prg, pt);
  }

  const auto proof1_sz = seri::Serializer<PLinProof>::sizeOf(proof1);

  std::cout << "receiver dependent proof size: ";
  std::cout << proof1_sz << " bytes\n";

  std::cout << "total proof size: ";
  std::cout << (p_all_sz + proof1_sz) << " bytes\n";

  for (std::size_t i = 0; i < it; i++) {
    if (!verifyApocmProof(stmt, p_all, proof1, us[id], id, pt)) {
      std::cout << "not good!\n";
      std::exit(2);
    }
  }

  std::cout << "--------------\n";

  pt->print("prove_ind", "receiver independent");
  pt->print("prove_dep", "receiver dependent");
  pt->print("verify", "verification");
}
