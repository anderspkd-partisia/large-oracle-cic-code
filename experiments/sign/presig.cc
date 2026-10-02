#include "presig.h"

#include <scl/ss/pedersen.h>
#include <scl/util/prg.h>

#include "util.h"

using namespace scl;

PreSigBundle PreSigBundle::create(std::size_t n) {
  const auto t = threshold(n);

  auto prg = scl::util::PRG::create("signer_pre_sigs");

  const auto sk = Field::random(prg);
  const auto G = Curve::generator();

  const auto a = Field::random(prg);
  const auto k = Field::random(prg);

  const auto R = k * G;
  const auto b = a * sk;
  const auto c = a * k;

  auto a_shares = ss::pedersenSecretShare<Curve>(a, t, n, prg, pedersenH());
  auto b_shares = ss::pedersenSecretShare<Curve>(b, t, n, prg, pedersenH());
  auto c_shares = ss::pedersenSecretShare<Curve>(c, t, n, prg, pedersenH());

  std::vector<PreSig> presigs;
  presigs.reserve(n);

  for (std::size_t i = 0; i < n; ++i) {
    presigs.emplace_back(PreSig{
        R,
        a_shares.getShare(i),
        b_shares.getShare(i),
        c_shares.getShare(i),
    });
  }

  const auto pk = sk * G;

  return {presigs, pk};
}
