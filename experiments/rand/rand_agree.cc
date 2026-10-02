#include <cstdint>
#include <stdexcept>
#include <unordered_map>

#include <scl/protocol/result.h>
#include <scl/serialization/serializer.h>
#include <scl/ss/pedersen.h>
#include <scl/util/bitmap.h>
#include <scl/util/digest.h>
#include <scl/util/merkle.h>
#include <scl/util/merkle_proof.h>

#include "ctx.h"
#include "enc.h"
#include "network.h"
#include "rand.h"
#include "util.h"

using namespace scl;

struct Hashes {
  Digest h1;
  Digest h2;
};

namespace {

util::Bitmap createBitmap(std::size_t size) {
  util::Bitmap bm(size);
  for (std::size_t i = 0; i < size; i++) {
    bm.set(i, true);
  }
  return bm;
}

std::size_t validateBcData(net::Packet& bc_data, std::size_t t) {
  const std::size_t m = bc_data.read<int>();
  if (m <= t) {
    throw std::logic_error("not enough parties in broadcast");
  }
  return m;
}

std::optional<std::vector<PVSS>> readShares(std::size_t batches,
                                            net::Packet& vss_pkt,
                                            net::Packet& shr_pkt,
                                            std::size_t my_id) {
  std::vector<PVSS> shares;
  shares.reserve(batches);

  for (std::size_t batch = 0; batch < batches; batch++) {
    math::Vector<Curve> c = vss_pkt.read<math::Vector<Curve>>();
    math::Array<Field, 2> s;
    s[0] = shr_pkt.read<Field>();
    s[1] = shr_pkt.read<Field>();

    const auto share = PVSS{s, c};

    if (!ss::pedersenVerify(share, my_id + 1, pedersenH())) {
      return {};
    }

    shares.emplace_back(share);
  }

  return shares;
}

std::optional<std::vector<PVSS>> validate(std::size_t batches,
                                          net::Packet& vss_pkt,
                                          net::Packet& shr_pkt,
                                          const Hashes& hashes,
                                          std::size_t my_id,
                                          std::size_t n,
                                          std::size_t t) {
  // verify h1
  const auto h1 = Hash{}.update(vss_pkt.get(), vss_pkt.size()).finalize();
  if (h1 != hashes.h1) {
    return {};
  }

  // verify h2
  const auto enc = encode(vss_pkt.get(), vss_pkt.size(), t, n);
  const auto h2 = util::MerkleTree<Hash, Block>::hash(enc);
  if (h2 != hashes.h2) {
    return {};
  }

  return readShares(batches, vss_pkt, shr_pkt, my_id);
}

}  // namespace

coro::Task<proto::ProtocolResult> Agree::run(proto::Env& env) const {
  NetworkWithBroadcast nw(env.network);

  const auto n = nw.size();
  const auto t = threshold(n);

  auto bc_data = co_await nw.broadcast();

  util::Bitmap v = createBitmap(n);

  const auto m = validateBcData(bc_data, t);

  std::vector<std::optional<std::vector<PVSS>>> shares(n);

  for (std::size_t i = 0; i < m; i++) {
    const std::size_t id = bc_data.read<int>();
    const Hashes hashes{.h1 = bc_data.read<Digest>(),
                        .h2 = bc_data.read<Digest>()};

    // save for later
    Context::storeMerkleRoots(nw.myId(), id, hashes.h2);

    // By the time the broadcast finishes, we also assume that the broadcasting
    // party will have sent data to us. We consider it dead if this is not the
    // case.
    if (co_await nw.party(id)->hasData()) {
      auto vss_pkt = co_await nw.party(id)->recv();
      auto shr_pkt = co_await nw.party(id)->recv();
      const auto shares_i =
          validate(m_batches, vss_pkt, shr_pkt, hashes, nw.myId(), n, t);

      if (shares_i.has_value()) {
        shares[id] = shares_i;
        v.set(id, false);
      }
    }
  }

  net::Packet to_bc;
  to_bc << v;
  co_await nw.broadcast(to_bc);

  co_return proto::ProtocolResult::next(
      std::make_unique<Fill>(std::move(shares)));
}

namespace {

// reads a list of bitmaps received from the broadcast primitive.
std::vector<std::optional<util::Bitmap>> parseBitmaps(std::size_t m,
                                                      std::size_t n,
                                                      util::Bitmap& malicious,
                                                      net::Packet& bc_data) {
  const auto t = threshold(n);
  std::vector<std::optional<util::Bitmap>> bitmaps(n);
  std::size_t id = bc_data.read<int>();
  util::Bitmap bitmap = bc_data.read<util::Bitmap>();
  m--;

  // this loop parses both bitmaps that were broadcast, and those that were
  // not. A party P_i is marked as malicious if:
  //  - P_i did not sent a bitmap,
  //  - P_i sent a bitmap with a hamming weight > t.
  for (std::size_t i = 0; i < n; i++) {
    if (i == id) {
      if (bitmap.count() <= t) {
        bitmaps[id] = bitmap;
      } else {
        malicious.set(id, true);
      }

      if (m-- > 0) {
        id = bc_data.read<int>();
        bitmap = bc_data.read<util::Bitmap>();
      }

    } else {
      malicious.set(i, true);
    }
  }

  return bitmaps;
}

}  // namespace

coro::Task<proto::ProtocolResult> Fill::run(proto::Env& env) const {
  NetworkWithBroadcast nw(env.network);

  const auto n = nw.size();
  const auto t = threshold(n);

  auto bc_data = co_await nw.broadcast();

  const auto m = validateBcData(bc_data, t);

  // will be used to track which parties sent an invalid bitmap, or was blamed
  // by too many other parties
  util::Bitmap malicious(n);

  const auto bitmaps = parseBitmaps(m, n, malicious, bc_data);

  // next step is to build an "accuser map" which tells us who's blaiming
  // who. Specifically, acc_map[i] = {k | bitmaps[k][i] == 1}. At the same time,
  // we will also validate the broadcast bitmaps by removing any parties i where
  // either bitmaps[i].count() > t or where acc_map[i].size() > t.
  std::unordered_map<std::size_t, std::vector<std::size_t>> acc_map;

  for (std::size_t i = 0; i < n; i++) {
    // will not consider parties that has been marked as malicious, which in
    // this case means they either sent no bitmap, or an invalid one.
    if (malicious.at(i)) {
      continue;
    }

    // at this point P_i might potentially be a good guy, so we need to find all
    // the other parties that are pointing at them.
    for (std::size_t j = 0; j < n; j++) {
      const auto& obm = bitmaps[j];
      if (obm.has_value()) {
        const auto& bitmap = obm.value();
        if (bitmap.at(i)) {
          acc_map[i].emplace_back(j);
        }
      }
    }
  }

  // finally, before we go to the next step, we will through away all the
  // parties which were blamed by more than t other parties.
  for (const auto& kv : acc_map) {
    if (std::get<1>(kv).size() > t) {
      malicious.set(std::get<0>(kv), true);
      // erase on std::unordered_map does not reorder existing elements, so this
      // is OK
      acc_map.erase(std::get<0>(kv));
    }
  }

  co_return proto::ProtocolResult::next(
      std::make_unique<FillExchange>(std::move(m_shares),
                                     std::move(acc_map),
                                     std::move(malicious)));
}

namespace {

#define COMMIT_MSG ((unsigned char)0)
#define SHARE_MSG ((unsigned char)1)

bool notIn(std::size_t i, const std::vector<std::size_t>& is) {
  return std::find(is.begin(), is.end(), i) == is.end();
}

// step 2.b: Only run by accused
coro::Task<void> sendShares(const std::vector<std::size_t>& accusors,
                            const util::Bitmap& malicious,
                            NetworkWithBroadcast& network) {
  // This recreates the first half of the rand share protocol, except that we do
  // not compute the commitments.
  auto prg = util::PRG::create("rand_" + std::to_string(network.myId()));

  const auto n = network.size();
  const auto t = threshold(n);
  const auto batches = Context::batchesRequired();

  std::vector<net::Packet> share_pkts(accusors.size());

  for (auto& pkt : share_pkts) {
    pkt << SHARE_MSG << (int)network.myId();
  }

  // recompute the shares/randomness for all parties in the accusors list.
  for (std::size_t batch = 0; batch < batches; batch++) {
    const auto s = Field::random(prg);
    const auto r = Field::random(prg);

    const math::Vector<math::Array<Field, 2>> shares =
        ss::shamirSecretShare(math::Array<Field, 2>{{s, r}}, t, n, prg);

    for (std::size_t i = 0; i < accusors.size(); i++) {
      const auto aid = accusors[i];
      share_pkts[i] << shares[aid][0] << shares[aid][1];
    }
  }

  // send shares of accusors to everyone
  for (std::size_t i = 0; i < n; i++) {
    // We only send shares to non-malicious parties that are not accusing us
    // (these are the only parties that can verify our shares in the echo
    // phase).
    if (i != network.myId() && !malicious.at(i) && notIn(i, accusors)) {
      for (const auto& pkt : share_pkts) {
        co_await network.party(i)->send(pkt);
      }
    }
  }
}

bool verifyShares(net::Packet& packet,
                  const std::vector<PVSS>& shares,
                  std::size_t id) {
  for (const auto& share : shares) {
    math::Array<Field, 2> s;
    s[0] = packet.read<Field>();
    s[1] = packet.read<Field>();

    if (!ss::pedersenVerify<Curve>(s, share.commitments, id + 1, pedersenH())) {
      return false;
    }
  }

  return true;
}

coro::Task<util::Bitmap> echoShares(
    const AccusorMap& acc_map,
    const std::vector<std::optional<std::vector<PVSS>>>& shares,
    NetworkWithBroadcast& network) {
  const auto my_id = network.myId();

  // will contain 1 bits for parties that sends us bad shares.
  util::Bitmap malicious(network.size());
  std::unordered_map<std::size_t, std::vector<net::Packet>> to_echo;

  for (const auto& [accused, accusors] : acc_map) {
    if (accused != my_id && notIn(my_id, accusors)) {
      // alright---otherwise we would be one of the accusors
      const auto& comms = shares[accused].value();  // NOLINT

      for (const auto& accusor : accusors) {
        // This assumes the sender is not dead. In practice this should be
        // guarded by some timeout.
        auto pkt = co_await network.party(accused)->recv();

        pkt.read<unsigned char>();  // discard header
        pkt.read<int>();            // discard ID

        // verify the shares.
        if (verifyShares(pkt, comms, accusor)) {
          to_echo[accusor].emplace_back(pkt);
        } else {
          // if the accused sends us a bad value, we're not gonna bother with
          // the rest it sends
          malicious.set(accused, true);
          break;
        }
      }
    }
  }

  // Shares are echoed here instead of inline above to ensure that an echoed
  // share does not get confused for a share to-be echoed.
  for (const auto& [accusor, packets] : to_echo) {
    for (const auto& pkt : packets) {
      co_await network.party(accusor)->send(pkt);
    }
  }

  co_return malicious;
}

coro::Task<void> sendCommits(std::size_t accused,
                             const std::vector<std::size_t>& accusors,
                             const std::vector<PVSS>& shares,
                             NetworkWithBroadcast& network) {
  // send an encoding of accused's commitments to each party in accusors

  const auto n = network.size();
  const auto t = threshold(n);

  net::Packet vss;
  for (const auto& share : shares) {
    vss << share.commitments;
  }

  const auto enc = encode(vss.get(), vss.size(), t, n);
  const auto proof = util::MerkleTree<Hash, Block>::prove(enc, network.myId());

  net::Packet compressed;
  compressed << COMMIT_MSG;
  compressed << (int)accused;
  compressed << enc[network.myId()];
  compressed << proof;

  for (const auto& accusor : accusors) {
    co_await network.party(accusor)->send(compressed);
  }
}

}  // namespace

coro::Task<proto::ProtocolResult> FillExchange::run(proto::Env& env) const {
  // for each entry [accused, accusors] in the accusation map, we need to do a
  // couple of things things:
  //  1. If we are the accused, then we need to send shares of each of the
  //     accusors to all non-accusor parties.
  //  2. If we are an accusor, then we do nothing.
  //  3. If we are not an accused, nor an accusor, then
  //   3.1. Receive shares from the accused party and check them against its VSS
  //        commitment (we are guaranteed to posses said commitment since we
  //        would otherwise have accused the party)
  //   3.2. Re-send the shares to the accusor
  //  4. Finally, when the echo step above is done, then we send an encoding of
  //  the accused VSS commitment to the accusors.
  //
  // The ordering above differs a tiny bit from the paper: In the paper, the
  // shares are sent parallel with the encoded commitments, whereas in the
  // above, the encoded commitments are sent together with the echo
  // messages. The approach presented here is easier to implement.

  NetworkWithBroadcast nw(env.network);
  const std::size_t my_id = nw.myId();

  // first we send shares
  for (const auto& [accused, accusors] : m_acc_map) {
    if (accused == my_id) {
      co_await sendShares(accusors, m_malicious, nw);
    }
  }

  // then we echo received shares. The malicious bitmap is the combination of
  // the map from the previous round + parties that did not send us a valid
  // share.
  const auto malicious =
      (co_await echoShares(m_acc_map, m_shares, nw)) | m_malicious;

  for (const auto& [accused, accusors] : m_acc_map) {
    if (accused != my_id && notIn(my_id, accusors)) {
      // we are not accusing P_{accused}, and this party is not marked as
      // malicious, so we are guaranteed to have received valid commitments
      const auto& shares = m_shares[accused].value();  // NOLINT
      co_await sendCommits(accused, accusors, shares, nw);
    }
  }

  net::Packet m;
  m << malicious;
  co_await nw.broadcast(m);

  co_return proto::ProtocolResult::next(
      std::make_unique<Finalize>(std::move(m_shares)));
}

namespace {

std::vector<PVSS> fixShare() {
  const auto batches = Context::batchesRequired();
  return std::vector<PVSS>(batches);
}

std::vector<PVSS> flatten(std::vector<std::vector<PVSS>>&& shares) {
  const auto n = shares.size();
  const auto batches = shares[0].size();

  std::vector<PVSS> s;
  s.reserve(n * batches);
  for (std::size_t i = 0; i < n; i++) {
    for (std::size_t batch = 0; batch < batches; batch++) {
      s.emplace_back(shares[i][batch]);
    }
  }

  return s;
}

struct IdAndBlock {
  std::size_t id;
  Block block;
};

coro::Task<std::vector<std::vector<PVSS>>> receiveSharesAndCommits(
    const std::vector<std::size_t>& annoying_parties,
    const util::Bitmap& malicious,
    NetworkWithBroadcast& network) {
  // for each party that to us is annoying, we will receive a set of shares and
  // a commitment block from each other party.
  const auto n = network.size();
  const auto t = threshold(n);
  const auto my_id = network.myId();

  std::unordered_map<std::size_t, std::vector<net::Packet>> s;
  // receive shares
  for (const auto& aid : annoying_parties) {
    for (std::size_t i = 0; i < n; i++) {
      if (i != my_id && i != aid) {
        auto pkt = co_await network.party(i)->recv();

        if (pkt.read<unsigned char>() != SHARE_MSG) {
          throw std::runtime_error("invalid share header");
        }

        const std::size_t id = pkt.read<int>();
        s[id].emplace_back(pkt);
      }
    }
  }

  std::unordered_map<std::size_t, std::vector<IdAndBlock>> c;
  // receive commitments
  for (const auto& aid : annoying_parties) {
    for (std::size_t i = 0; i < n; i++) {
      if (i != my_id && i != aid) {
        auto pkt = co_await network.party(i)->recv();

        if (pkt.read<unsigned char>() != COMMIT_MSG) {
          throw std::runtime_error("invalid commit header");
        }

        const std::size_t accused = pkt.read<int>();
        const auto block = pkt.read<Block>();
        const auto proof = pkt.read<util::MerkleProof<Hash::DigestType>>();
        const auto root = Context::getMerkleRoot(my_id, i);

        if (util::MerkleTree<Hash, Block>::verify(block, root, proof)) {
          c[accused].emplace_back(IdAndBlock{i, block});
        }
      }
    }
  }

  (void)t;
  (void)malicious;

  co_return {};
}

}  // namespace

coro::Task<proto::ProtocolResult> Finalize::run(proto::Env& env) const {
  NetworkWithBroadcast nw(env.network);

  const auto n = nw.size();
  const auto t = threshold(n);

  auto bc_data = co_await nw.broadcast();
  const auto m = validateBcData(bc_data, t);

  util::Bitmap malicious(n);
  const auto bitmaps = parseBitmaps(m, n, malicious, bc_data);

  std::vector<std::vector<PVSS>> shares(n);

  // go through all the shares we received in the first round. If we're missing
  // a share from someone, then either this party is malicious (in which case we
  // simply fix the share to all 0s); otherwise we will receive the share from
  // other parties. In the latter case, we designate the party as "annoying".

  std::vector<std::size_t> annoying_parties;
  for (std::size_t i = 0; i < n; i++) {
    const auto& current_shares = m_shares[i];
    if (current_shares.has_value()) {
      shares[i] = current_shares.value();
    } else {
      if (malicious.at(i)) {
        shares[i] = fixShare();
      } else {
        annoying_parties.emplace_back(i);
      }
    }
  }

  // for each annoying party, we will receive shares and commitments from the
  // other parties
  const auto annoying_shares =
      co_await receiveSharesAndCommits(annoying_parties, malicious, nw);

  for (std::size_t i = 0; i < annoying_shares.size(); i++) {
    shares[annoying_parties[i]] = annoying_shares[i];
  }

  co_return proto::ProtocolResult::next(
      std::make_unique<Extract>(flatten(std::move(shares))));
}
