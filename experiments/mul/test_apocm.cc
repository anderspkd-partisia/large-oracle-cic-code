#include <ios>
#include <vector>

#include <scl/util/prg.h>

#include "apocm.h"
#include "ctx.h"
#include "prover.h"
#include "rands.h"
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

int main() {
  const std::size_t n = 10;
  const std::size_t id = 3;

  Context::init(n, 2);

  const auto t = Context::threshold();

  Context::print();

  const auto inputs = RandBundle::create(n, 1);
  const auto drands = DoubleRandBundle::create(n, 1);

  const auto stmt = ApocmStmt::create(inputs.as[id].begin(),
                                      inputs.bs[id].begin(),
                                      drands.r2s[id].begin(),
                                      id);

  const auto witness = ApocmWitness::create(inputs.as[id].begin(),
                                            inputs.bs[id].begin(),
                                            drands.r2s[id].begin());
  const auto us = computeUs(inputs.as[id], inputs.bs[id], drands.r2s[id], n, t);

  auto prg = util::PRG::create("test_apocm");

  const auto [pi_all, fs] =
      createReceiverIndependentProof(stmt, witness, us, prg, nullptr);

  for (std::size_t i = 0; i < n; i++) {
    const auto p =
        createReceiverDependentProof(pi_all.Qs, us, fs, i, prg, nullptr);

    const auto good = verifyApocmProof(stmt, pi_all, p, us[i], i);

    std::cout << std::boolalpha << good << "\n";

    if (!good) {
      std::cout << i << "\n";
      std::exit(1);
    }
  }
}
