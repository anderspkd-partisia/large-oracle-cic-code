#include "rands.h"

#include <scl/ss/feldman.h>
#include <scl/ss/pedersen.h>

#include "util.h"

using namespace scl;

namespace {

void expandCommits(math::Vector<Curve>& commits, std::size_t n) {
  const auto m = n - commits.size();
  std::vector<Curve> missing;
  missing.reserve(m);

  for (std::size_t i = commits.size(); i <= n; i++) {
    missing.emplace_back(ss::computeCommitmentForIndex(commits, i));
  }

  commits.toStlVector().insert(commits.end(), missing.begin(), missing.end());
}

}  // namespace

RandBundle RandBundle::create(std::size_t number_of_parties,
                              std::size_t number_of_muls) {
  const auto t = threshold(number_of_parties);
  const auto c = batchesRequired(t, number_of_muls);

  // std::cout << "--------------\n";
  // std::cout << "Creating " << c << " batches (total_muls=" << (c * t) << ")\n";
  // std::cout << "--------------\n";

  auto prg = util::PRG::create("RandBundle");

  RandBundle bundle;
  bundle.as.resize(number_of_parties);
  bundle.bs.resize(number_of_parties);
  bundle.as_clear.reserve(c * t);
  bundle.bs_clear.reserve(c * t);

  for (std::size_t batch = 0; batch < c; batch++) {
    for (std::size_t i = 0; i < t; i++) {
      const auto a = Field::random(prg);
      bundle.as_clear.emplace_back(a);
      const auto b = Field::random(prg);
      bundle.bs_clear.emplace_back(b);

      auto a_shrs = ss::pedersenSecretShare<Curve>(a,
                                                   t,
                                                   number_of_parties,
                                                   prg,
                                                   pedersenH());
      auto b_shrs = ss::pedersenSecretShare<Curve>(b,
                                                   t,
                                                   number_of_parties,
                                                   prg,
                                                   pedersenH());

      expandCommits(a_shrs.commitments, number_of_parties);
      expandCommits(b_shrs.commitments, number_of_parties);

      for (std::size_t j = 0; j < number_of_parties; j++) {
        bundle.as[j].emplace_back(a_shrs.getShare(j));
        bundle.bs[j].emplace_back(b_shrs.getShare(j));
      }
    }
  }

  return bundle;
}

DoubleRandBundle DoubleRandBundle::create(std::size_t number_of_parties,
                                          std::size_t number_of_muls) {
  const auto t = threshold(number_of_parties);
  const auto c = batchesRequired(t, number_of_muls);

  auto prg = util::PRG::create("DoubleRandBundle");

  DoubleRandBundle bundle;
  bundle.r1s.resize(number_of_parties);
  bundle.r2s.resize(number_of_parties);

  for (std::size_t batch = 0; batch < c; batch++) {
    for (std::size_t i = 0; i < t; i++) {
      const auto r = Field::random(prg);
      auto r1_shrs = ss::pedersenSecretShare<Curve>(r,
                                                    t,
                                                    number_of_parties,
                                                    prg,
                                                    pedersenH());
      auto r2_shrs = ss::pedersenSecretShare<Curve>(r,
                                                    2 * t,
                                                    number_of_parties,
                                                    prg,
                                                    pedersenH());

      expandCommits(r2_shrs.commitments, number_of_parties);

      for (std::size_t j = 0; j < number_of_parties; j++) {
        bundle.r1s[j].emplace_back(r1_shrs.getShare(j));
        bundle.r2s[j].emplace_back(r2_shrs.getShare(j));
      }
    }
  }

  return bundle;
}
