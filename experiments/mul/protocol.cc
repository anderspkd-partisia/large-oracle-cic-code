#include "protocol.h"

#include <memory>
#include <utility>

#include <scl/math/lagrange.h>
#include <scl/math/vector.h>
#include <scl/protocol/base.h>
#include <scl/protocol/result.h>
#include <scl/ss/pedersen.h>
#include <scl/ss/shamir.h>
#include <scl/util/prg.h>

#include "apocm.h"
#include "ctx.h"
#include "prover.h"
#include "rands.h"
#include "util.h"

using namespace scl;

using PVSSItDiff = std::vector<PVSS>::difference_type;

coro::Task<std::vector<Field>> MulProtocolRecv::recvU(
    net::Network& network) const {
  using namespace std::chrono_literals;

  const std::size_t n = Context::numberOfParties();
  const std::size_t t = Context::threshold();
  const std::size_t needed = 2 * t + 1;
  const std::size_t batch_count = m_input.batch_count;

  // use this struct to keep track of shares we've received so far. A received
  // share is either indeterminate (no data received so far), good (valid share
  // received), or bad (malformed message).
  struct Data {
    enum class Status { IND, GOOD, BAD };

    Status status = Status::IND;
    std::vector<Field> us;
  };

  std::vector<Data> data(n);
  std::size_t good_count = 1;
  std::size_t bad_count = 0;

  // Read data from the local party. No need to verify proof here.
  {
    data[network.myId()].status = Data::Status::GOOD;
    net::Packet pkt = co_await network.me()->recv();
    data[network.myId()].us.reserve(batch_count);
    for (std::size_t batch = 0; batch < batch_count; batch++) {
      data[network.myId()].us.emplace_back(pkt.read<Field>());
    }
  }

  while (good_count < needed && bad_count < t) {
    for (std::size_t i = 0; i < n; i++) {
      // skip parties we've already received from
      if (data[i].status != Data::Status::IND) {
        continue;
      }

      // check if the party has data for us.
      if (co_await network.party(i)->hasData()) {
        net::Packet pkt = co_await network.party(i)->recv();

        if (checkMsgId(pkt, MSG1)) {
          std::vector<Field> us;
          us.reserve(m_input.batch_count);

          for (std::size_t batch = 0; batch < m_input.batch_count; batch++) {
            const PVSSItDiff offset = static_cast<PVSSItDiff>(batch * t);
            auto as_it = m_input.as.begin() + offset;
            auto bs_it = m_input.bs.begin() + offset;
            auto rs_it = m_input.r2s.begin() + offset;

            const auto stmt = ApocmStmt::create(as_it, bs_it, rs_it, i);

            const auto u = pkt.read<Field>();
            const auto iproof = pkt.read<ApocmProof>();
            const auto dproof = pkt.read<PLinProof>();

            const auto proof_valid = co_await m_verifier->run(stmt,
                                                              iproof,
                                                              dproof,
                                                              u,
                                                              network.myId());

            if (!proof_valid) {
              data[i].status = Data::Status::BAD;
              break;
            }

            us.emplace_back(u);
          }

          if (data[i].status == Data::Status::IND) {
            data[i].status = Data::Status::GOOD;
            data[i].us = us;
            good_count++;

            // exit early if possible.
            if (good_count >= needed) {
              goto shares_received;
            }

          } else {
            data[i].status = Data::Status::BAD;
            bad_count++;
          }
        } else {
          data[i].status = Data::Status::BAD;
        }
      }
    }
  }

  if (bad_count > t) {
    throw std::runtime_error("barf");
  }

shares_received:

  // reconstruct the u share.

  std::vector<Field> ids;
  ids.reserve(needed);
  std::vector<std::vector<Field>> shares(m_input.batch_count);

  for (std::size_t i = 0; i < n; i++) {
    if (data[i].status == Data::Status::GOOD) {
      ids.emplace_back(i + 1);

      for (std::size_t batch = 0; batch < m_input.batch_count; batch++) {
        shares[batch].emplace_back(data[i].us[batch]);
      }
    }
  }

  std::vector<Field> us;
  us.reserve(m_input.batch_count);
  for (std::size_t batch = 0; batch < m_input.batch_count; batch++) {
    us.emplace_back(
        ss::shamirRecoverP<Field>(shares[batch], ids, Field::zero()));
  }

  co_return us;
}

namespace {

coro::Task<std::vector<Field>> recvW(net::Network& network,
                                     std::size_t batch_count) {
  // initialize the received shares to something sensible. Makes it easier to
  // attempt reconstruction later without having to account for missing values.
  const std::size_t n = Context::numberOfParties();
  const std::size_t t = Context::threshold();
  std::vector<std::vector<Field>> batches(batch_count);
  util::Bitmap recvd(n);
  for (std::size_t i = 0; i < batch_count; i++) {
    batches[i].resize(n);
  }

  const math::Vector<Field> thetas = Context::thetas();
  std::vector<Field> w_shares;

  while (true) {
    bool waiting_for_one_more = true;

    for (std::size_t i = 0; i < n; i++) {
      // skip parties we've already received from
      if (recvd.at(i)) {
        continue;
      }

      if (co_await network.party(i)->hasData()) {
        auto pkt = co_await network.party(i)->recv();

        // there's a chance this is the first time we receive from a party, so
        // we need to check the message header to make sure it's the right
        // message we recieve.
        if (checkMsgId(pkt, MSG2)) {
          const std::vector<Field> ws = pkt.read<std::vector<Field>>();

          // sanity check
          if (ws.size() != batch_count) {
            throw std::logic_error("invalid number of shares received");
          }

          for (std::size_t j = 0; j < ws.size(); j++) {
            batches[j][i] = ws[j];
          }

          recvd.set(i, true);
          waiting_for_one_more = false;
        }
      }

      // we can attempt reconstruction after getting 2t + 1 or more shares. If
      // we receive 2t + 1 good shares, then all the missing entries can be
      // corrected for. Otherwise we'll have to wait a bit longer
      if (!waiting_for_one_more && recvd.count() > 2 * t) {
        try {
          for (math::Vector<Field> batch : batches) {
            const auto rec = ss::shamirRecoverC(batch, thetas);
            const auto& vals = rec.f.coefficients();
            w_shares.insert(w_shares.end(), vals.begin() + 1, vals.end());
          }

          goto done;

        } catch (std::exception& e) {
          waiting_for_one_more = true;
        }
      }
    }
  }

  if (w_shares.empty()) {
    throw -1;
  }

done:

  co_return w_shares;
}

coro::Task<std::vector<net::Packet>> computeProofPackets(
    const MulProtocolInput& input,
    std::vector<std::vector<Field>>&& batches,
    std::size_t my_id,
    Prover* prover) {
  const auto t = Context::threshold();
  const auto n = Context::numberOfParties();
  auto prg = util::PRG::create("apocm_" + std::to_string(my_id));

  std::vector<net::Packet> packets(n);

  for (std::size_t i = 0; i < n; i++) {
    if (i != my_id) {
      packets[i] << MSG1;
    }
  }

  for (std::size_t batch = 0; batch < input.batch_count; batch++) {
    const auto offset = static_cast<PVSSItDiff>(batch * t);
    auto as_it = input.as.begin() + offset;
    auto bs_it = input.bs.begin() + offset;
    auto rs_it = input.r2s.begin() + offset;

    const auto witness = ApocmWitness::create(as_it, bs_it, rs_it);
    const auto stmt = ApocmStmt::create(as_it, bs_it, rs_it, my_id);

    const auto [iproof, fs] =
        co_await prover->initialize(stmt, witness, batches[batch], prg);

    for (std::size_t i = 0; i < n; i++) {
      if (i != my_id) {
        const auto dproof =
            co_await prover->run(iproof.Qs, batches[batch], fs, i, prg);

        packets[i] << batches[batch][i] << iproof << dproof;
      }
    }
  }

  co_return packets;
}

}  // namespace

coro::Task<proto::ProtocolResult> MulProtocol::run(proto::Env& env) const {
  const auto n = env.network.size();
  const auto t = threshold(n);
  const auto batch_count = m_input.batch_count;

  std::vector<Field> w2s;
  w2s.reserve(batch_count * t);

  for (std::size_t i = 0; i < batch_count * t; i++) {
    // w = a*b - r2
    const auto w = m_input.as[i].getShare() * m_input.bs[i].getShare() -
                   m_input.r2s[i].getShare();

    w2s.emplace_back(w);
  }

  // std::vector<ProofInput> proof_inputs;
  // proof_inputs.reserve(n);

  std::vector<std::vector<Field>> batches;
  batches.reserve(batch_count);

  for (std::size_t batch = 0; batch < batch_count; batch++) {
    std::vector<Field> us;
    us.reserve(n);

    const auto batch_offset = batch * t;

    for (std::size_t i = 0; i < n; i++) {
      const auto theta = Context::theta(i);
      Field u;

      for (std::size_t j = t; j-- > 0;) {
        u = (u + w2s[batch_offset + j]) * theta;
      }
      us.emplace_back(u);
    }

    batches.emplace_back(us);
  }

  // send shares to ourselves first.
  {
    net::Packet pkt(batch_count * Field::byteSize());
    for (std::size_t batch = 0; batch < batch_count; batch++) {
      pkt << batches[batch][env.network.myId()];
    }
    co_await env.network.me()->send(pkt);
  }

  std::vector<net::Packet> packets =
      co_await computeProofPackets(m_input,
                                   std::move(batches),
                                   env.network.myId(),
                                   m_prover.get());

  for (std::size_t i = 0; i < n; i++) {
    if (i != env.network.myId()) {
      co_await env.network.party(i)->send(packets[i]);
    }
  }

  co_return proto::ProtocolResult::next(
      std::make_unique<MulProtocolRecv>(std::move(m_input),
                                        std::move(m_verifier)));
}

coro::Task<proto::ProtocolResult> MulProtocolRecv::run(proto::Env& env) const {
  const auto t = threshold(env.network.size());

  const std::vector<Field> us = co_await recvU(env.network);

  net::Packet pkt;
  pkt << MSG2 << us;
  co_await env.network.send(pkt);

  const std::vector<Field> ws =
      co_await recvW(env.network, m_input.batch_count);

  std::vector<PVSS> cs;
  cs.reserve(ws.size());
  for (std::size_t i = 0; i < m_input.batch_count * t; i++) {
    cs.emplace_back(
        PVSS{ws[i] + m_input.r1s[i].getShare(), m_input.r1s[i].commitments});
  }

  co_return proto::ProtocolResult::done(cs);
}
