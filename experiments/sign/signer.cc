#include "./signer.h"

#include <scl/coro/coroutine.h>
#include <scl/math/array.h>
#include <scl/math/lagrange.h>
#include <scl/math/vector.h>
#include <scl/net/packet.h>
#include <scl/protocol/result.h>
#include <scl/util/time.h>

#include "util.h"

using namespace scl;

namespace {

std::array<Field, 2> recover(
    const std::vector<std::optional<net::Packet>>& data,
    std::size_t t) {
  std::vector<Field> idxs;
  idxs.reserve(t + 1);
  std::vector<Field> s_shrs;
  s_shrs.reserve(t + 1);
  std::vector<Field> c_shrs;
  c_shrs.reserve(t + 1);

  std::size_t count = 0;
  for (std::size_t i = 0; i < data.size(); i++) {
    const auto& opkt = data[i];
    if (opkt.has_value()) {
      auto pkt = opkt.value();
      idxs.emplace_back(i + 1);
      s_shrs.emplace_back(pkt.read<Field>());
      c_shrs.emplace_back(pkt.read<Field>());
      count++;
    }

    if (count >= t + 1) {
      break;
    }
  }

  const auto basis = math::computeLagrangeBasis(math::Vector(idxs), 0);
  const auto s =
      math::innerProd<Field>(s_shrs.begin(), s_shrs.end(), basis.begin());
  const auto c =
      math::innerProd<Field>(c_shrs.begin(), c_shrs.end(), basis.begin());

  return {s, c};
}

class RecoverStep final : public proto::Protocol {
 public:
  RecoverStep(std::size_t t,
              std::vector<std::optional<net::Packet>>&& shares,
              const Field& r_x)
      : m_t(t), m_shares(shares), m_r_x(r_x) {}

  coro::Task<proto::ProtocolResult> run(proto::Env& env) const override {
    (void)env;
    std::array<Field, 2> sc = recover(m_shares, m_t + 1);
    util::Signature<util::ECDSA> sig{m_r_x, sc[0] * sc[1].inverse()};

    co_return proto::ProtocolResult::done(sig);
  }

  std::string name() const override {
    return "recover";
  }

 private:
  std::size_t m_t;
  std::vector<std::optional<net::Packet>> m_shares;
  Field m_r_x;
};

}  // namespace

coro::Task<proto::ProtocolResult> Protocol::run(proto::Env& env) const {
  const auto m = util::ECDSA::digestToElement(m_msg);
  const auto r_x = util::ECDSA::conversionFunc(m_presig.R);

  const auto a_shr = m_presig.a.getShare();
  const auto b_shr = m_presig.b.getShare();

  const auto s = m * a_shr + r_x * b_shr;

  net::Packet pkt;
  pkt << s << m_presig.c.getShare();

  co_await env.network.send(pkt);

  const auto t = threshold(env.network.size());

  auto shares = co_await env.network.recv(t + 1);

  co_return proto::ProtocolResult::next(
      std::make_unique<RecoverStep>(t, std::move(shares), r_x));
}

coro::Task<proto::ProtocolResult> ProtocolSimple::run(proto::Env& env) const {
  const auto m = util::ECDSA::digestToElement(m_msg);
  const auto r_x = util::ECDSA::conversionFunc(m_presig.R);

  const auto a_shr = m_presig.a.getShare();
  const auto b_shr = m_presig.b.getShare();

  const auto s = m * a_shr + r_x * b_shr;

  net::Packet pkt;
  pkt << s << m_presig.c.getShare();

  co_await env.network.party(0)->send(pkt);

  co_return proto::ProtocolResult::done();
}
