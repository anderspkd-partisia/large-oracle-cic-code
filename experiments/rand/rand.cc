#include "rand.h"

#include <cstdint>
#include <string>

#include <scl/protocol/result.h>
#include <scl/ss/pedersen.h>
#include <scl/util/digest.h>
#include <scl/util/merkle.h>

#include "ctx.h"
#include "enc.h"
#include "network.h"
#include "util.h"

using namespace scl;

coro::Task<proto::ProtocolResult> Extract::run(proto::Env& env) const {
  NetworkWithBroadcast nw(env.network);

  // only party 0 will do the actual reconstruction. This speeds up everything
  // quite a bit. Alternatively, we can benchmark the application of the hyper
  // invertible matrix and then use a dummy implementation here, similar to the
  // ZK proofs in the mul protocol experiment.
  if (nw.myId() == 0) {
    const auto n = nw.size();
    const auto t = threshold(n);
    const auto batch_size = n - t;

    std::vector<PVSS> shares;
    shares.reserve(batch_size * Context::batchesRequired());

    using offset_t = typename std::vector<PVSS>::difference_type;
    offset_t offset = 0;
    for (std::size_t batch = 0; batch < Context::batchesRequired(); batch++) {
      auto begin = m_shares.begin() + offset;
      auto end = m_shares.end() + static_cast<offset_t>(offset + batch_size);
      offset += static_cast<offset_t>(batch_size);

      auto shrs = ss::apply<Curve>(begin, end, Context::HIM());

      shares.insert(shares.end(), shrs.begin(), shrs.end());
    }

    co_return proto::ProtocolResult::done(shares);
  }

  co_return proto::ProtocolResult::done();
}

coro::Task<proto::ProtocolResult> Protocol::run(proto::Env& env) const {
  NetworkWithBroadcast nw(env.network);

  const auto n = nw.size();
  const auto t = threshold(n);
  const auto batches = Context::batchesRequired();

  auto prg =
      util::PRG::create(std::string("rand_") + std::to_string(nw.myId()));

  std::vector<net::Packet> share_pkts(n);
  net::Packet vss_pkt;

  for (std::size_t batch = 0; batch < batches; batch++) {
    const auto s = Field::random(prg);
    const auto vss = ss::pedersenSecretShare<Curve>(s, t, n, prg, pedersenH());
    vss_pkt << vss.commitments;
    for (std::size_t i = 0; i < n; i++) {
      share_pkts[i] << vss.getShare(i).getShare() << vss.getShare(i).getRand();
    }
  }

  const auto h1 = Hash{}.update(vss_pkt.get(), vss_pkt.size()).finalize();

  const auto enc = encode(vss_pkt.get(), vss_pkt.size(), t, n);
  const auto h2 = util::MerkleTree<Hash, Block>::hash(enc);

  net::Packet data_to_bc;
  data_to_bc << h1 << h2;
  co_await nw.broadcast(data_to_bc);

  for (std::size_t i = 0; i < nw.size(); i++) {
    co_await nw.party(i)->send(vss_pkt);
    co_await nw.party(i)->send(std::move(share_pkts[i]));
  }

  co_return proto::ProtocolResult::next(std::make_unique<Agree>(batches));
}
