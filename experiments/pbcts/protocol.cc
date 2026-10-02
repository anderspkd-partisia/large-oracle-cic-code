#include "protocol.h"

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <memory>
#include <stdexcept>

#include <scl/protocol/base.h>
#include <scl/protocol/result.h>

#include "network.h"
#include "pbcts.h"

using namespace scl;

const static GoString DB_NAME = {":memory:", 8};
const static GoString DB_BACKUP_NAME = {"", 0};
const static GoString SESSION_ID = {"x", 1};
const static unsigned char KEY[32] = {0};
const static GoSlice DB_KEY = {(void*)KEY, 32, 32};

std::unique_ptr<proto::Protocol> PBCTS::create(std::size_t id,
                                               std::size_t n,
                                               std::size_t m) {
  const auto player_ref = pbctsNew(DB_NAME, DB_BACKUP_NAME, DB_KEY);

  if (player_ref.error != 0) {
    throw std::runtime_error("error obtaining a player ref\n");
  }

  const auto sess_ref = pbctsNewKeyGenSession(player_ref.reference,
                                              (GoInt)id,
                                              (GoInt)n,
                                              (GoInt)m,
                                              SESSION_ID);

  if (sess_ref.error != 0) {
    throw std::runtime_error("error obtaining a session ref\n");
  }

  return std::make_unique<ProtocolRound1>(sess_ref.reference);
}

namespace {

void freeData(const Data& data) {
  std::free(data.data);
}

void freeData(const Round1Data& data) {
  freeData(data.unicastData);
  freeData(data.unicastLengths);
  freeData(data.broadcastData);
}

void freeSlice(const GoSlice& slice) {
  std::free(slice.data);
}

void free2dSlice(const GoSlice& slice) {
  for (std::size_t i = 0; i < slice.len; i++) {
    freeSlice(*((GoSlice*)(slice.data) + i));
  }
  freeSlice(slice);
}

net::Packet dataToPacket(const void* data,
                         std::size_t size,
                         std::size_t offset) {
  const unsigned char* ptr = ((const unsigned char*)data) + offset;
  std::vector<unsigned char> x(ptr, ptr + size);
  net::Packet pkt(size + sizeof(long long));
  pkt << x;
  return pkt;
}

net::Packet dataToPacket(const Data& data) {
  return dataToPacket(data.data, data.size, 0);
}

long long getSize(const Data& data, std::size_t i) {
  return *((long long*)data.data + i);
}

void printData(const unsigned char* data, std::size_t n) {
  const std::size_t m = n > 100 ? 100 : n;
  std::cout << std::hex << std::setfill('0') << std::setw(2);
  for (std::size_t i = 0; i < m; i++) {
    std::cout << (int)*(data + i);
  }
  std::cout << "\n";
}

void printSlice(const GoSlice& slice) {
  // std::cout << slice.data << ", " << slice.cap << ", " << slice.len << "\n";
  printData((unsigned char*)slice.data, slice.len);
}

void print2dSlice(const GoSlice& slice) {
  std::cout << "===================\n";
  std::cout << slice.data << ", " << slice.cap << ", " << slice.len << "\n";
  for (std::size_t i = 0; i < slice.len; i++) {
    printSlice(*(((GoSlice*)slice.data) + i));
  }
}

GoSlice createSlice(const unsigned char* data, std::size_t size) {
  GoSlice go_slice;
  go_slice.data = std::malloc(size);
  go_slice.cap = size;  // NOLINT
  go_slice.len = size;  // NOLINT
  std::memcpy(go_slice.data, data, size);
  return go_slice;
}

GoSlice combineUnicastMessages(
    std::vector<std::optional<net::Packet>>&& packets) {
  GoSlice go_slice_outer;

  go_slice_outer.data = std::malloc(packets.size() * sizeof(GoSlice));
  go_slice_outer.len = packets.size();  // NOLINT
  go_slice_outer.cap = packets.size();  // NOLINT

  std::size_t offset = 0;
  for (const std::optional<net::Packet>& p : packets) {
    GoSlice slice = {.data = nullptr, .len = 0, .cap = 0};

    if (p.has_value()) {
      auto pkt = p.value();
      const auto data = pkt.read<std::vector<unsigned char>>();
      slice = createSlice(data.data(), data.size());
    }

    std::memcpy(((GoSlice*)go_slice_outer.data) + offset,
                &slice,
                sizeof(GoSlice));

    offset++;
  }

  return go_slice_outer;
}

GoSlice combineBroadcastMessages(net::Packet&& packet, std::size_t n) {
  GoSlice go_slice;
  go_slice.data = std::malloc(n * sizeof(GoSlice));
  go_slice.len = n;  // NOLINT
  go_slice.cap = n;  // NOLINT

  auto m = packet.read<int>();
  auto id = packet.read<int>();
  auto bytes = packet.read<std::vector<unsigned char>>();
  m--;

  for (std::size_t i = 0; i < n; i++) {
    GoSlice slice = {.data = nullptr, .len = 0, .cap = 0};

    if (i == id) {
      slice = createSlice(bytes.data(), bytes.size());

      if (m-- > 0) {
        id = packet.read<int>();
        bytes = packet.read<std::vector<unsigned char>>();
      }
    }

    std::memcpy(((GoSlice*)go_slice.data) + i, &slice, sizeof(GoSlice));
  }

  return go_slice;
}

}  // namespace

coro::Task<proto::ProtocolResult> ProtocolRound1::run(proto::Env& env) const {
  NetworkWithBroadcast nw(env.network);

  const Round1Data data = pbctsRound1(m_session);

  const auto bc_pkt = dataToPacket(data.broadcastData);
  co_await nw.broadcast(bc_pkt);

  std::size_t offset = 0;
  for (std::size_t i = 0; i < nw.size(); i++) {
    const long long size = getSize(data.unicastLengths, i);
    auto pkt = dataToPacket(data.unicastData.data, size, offset);
    offset += size;

    co_await nw.party(i)->send(std::move(pkt));
  }

  freeData(data);

  // send data into round 2

  auto bc_data = co_await nw.broadcast();

  std::vector<std::optional<net::Packet>> packets;

  for (std::size_t i = 0; i < nw.size(); i++) {
    if (co_await nw.party(i)->hasData()) {
      packets.emplace_back(co_await nw.party(i)->recv());
    } else {
      packets.emplace_back();
    }
  }

  GoSlice uc_msgs = combineUnicastMessages(std::move(packets));
  GoSlice bc_msgs = combineBroadcastMessages(std::move(bc_data), nw.size());

  const DataAndError data2 = pbctsRound2(m_session, uc_msgs, bc_msgs);

  free2dSlice(uc_msgs);
  free2dSlice(bc_msgs);

  if (data2.error != 0) {
    throw std::runtime_error("error in round 2");
  }

  const auto to_bc = dataToPacket(data2.data);
  freeData(data2.data);

  co_await nw.broadcast(to_bc);

  co_return proto::ProtocolResult::next(
      std::make_unique<ProtocolRoundN>(m_session, 3));
}

coro::Task<proto::ProtocolResult> ProtocolRoundN::run(proto::Env& env) const {
  NetworkWithBroadcast nw(env.network);

  auto bc_data = co_await nw.broadcast();

  GoSlice bc_msgs = combineBroadcastMessages(std::move(bc_data), nw.size());

  DataAndError data;

  if (m_round == 3) {
    data = pbctsRound3(m_session, bc_msgs);
  } else if (m_round == 4) {
    data = pbctsRound4(m_session, bc_msgs);
  } else if (m_round == 5) {
    data = pbctsRound5(m_session, bc_msgs);
  } else if (m_round == 6) {
    data = pbctsRound6(m_session, bc_msgs);
  } else if (m_round == 7) {
    const Round7Data data = pbctsRound7(m_session, bc_msgs);

    co_return proto::ProtocolResult::done();
  }

  if (data.error != 0) {
    throw std::runtime_error("error in round " + std::to_string(m_round));
  }

  const auto to_bc = dataToPacket(data.data);

  freeData(data.data);
  free2dSlice(bc_msgs);

  co_await nw.broadcast(to_bc);

  co_return proto::ProtocolResult::next(
      std::make_unique<ProtocolRoundN>(m_session, m_round + 1));
}
