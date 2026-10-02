#include "network.h"

#include <scl/coro/coroutine.h>
#include <scl/protocol/result.h>
#include <scl/util/time.h>

#include "util.h"

using namespace scl;

namespace {

bool timeoutHasBeenReached(std::optional<util::Time::Duration> start,
                           util::Time::Duration now,
                           util::Time::Duration timeout) {
  if (start.has_value()) {
    return now - start.value() >= timeout;
  }
  return false;
}

std::size_t countValues(const std::vector<Message>& messages) {
  std::size_t c = 0;
  for (const auto& m : messages) {
    if (m.has_value()) {
      c++;
    }
  }
  return c;
}

// returns true if all a value is present at all indices in messages where the
// corresponding entry in ignore_set is false.
bool messagesFromAllRelevant(const std::vector<Message>& messages,
                             const util::Bitmap& ignore_set) {
  for (std::size_t i = 0; i < messages.size(); i++) {
    if (!ignore_set.at(i) && !messages[i].has_value()) {
      return false;
    }
  }
  return true;
}

}  // namespace

coro::Task<proto::ProtocolResult> Broadcast::run(proto::Env& env) const {
  using namespace std::chrono_literals;

  NetworkWithBroadcast nw(env.network);
  std::vector<Message> messages(nw.size());

  util::Time::Duration now = env.clock->read();
  std::optional<scl::util::Time::Duration> start = {};

  while (!timeoutHasBeenReached(start, now, m_timeout)) {
    bool got_at_least_one_message = false;

    for (std::size_t i = 0; i < nw.size(); i++) {
      // attempt to receive messages from parties we are not ignoring, and who
      // haven't sent us a message yet.
      if (!m_ignore_set.at(i) && !messages[i].has_value()) {
        if (co_await nw.party(i)->hasData()) {
          got_at_least_one_message = true;
          messages[i] = co_await nw.party(i)->recv();
          if (!start.has_value()) {
            start = now;
          }
        }
      }
    }

    if (!got_at_least_one_message && !start.has_value()) {
      co_return proto::ProtocolResult::next(
          std::make_unique<Broadcast>(m_timeout, m_ignore_set));
    }

    if (messagesFromAllRelevant(messages, m_ignore_set)) {
      break;
    }

    now = env.clock->read();
  }

  co_return proto::ProtocolResult::next(
      std::make_unique<BroadcastSend>(m_timeout,
                                      m_ignore_set,
                                      std::move(messages)));
}

coro::Task<proto::ProtocolResult> BroadcastSend::run(proto::Env& env) const {
  util::Bitmap updated_ignore_set = m_ignore_set;

  net::Packet pkt;
  pkt << (int)countValues(m_messages);
  for (std::size_t i = 0; i < m_messages.size(); i++) {
    const auto& m = m_messages[i];
    if (m.has_value()) {
      pkt << (int)i;
      pkt << m.value();
    } else {
      // if party i did not send a message within the timeout, then we will
      // ignore it moving forward.
      updated_ignore_set.set(i, true);
    }
  }

  NetworkWithBroadcast nw(env.network);
  for (std::size_t i = 0; i < nw.size(); i++) {
    if (!updated_ignore_set.at(i)) {
      co_await nw.party(i)->send(pkt);
    }
  }

  co_return proto::ProtocolResult::next(
      std::make_unique<Broadcast>(m_timeout, updated_ignore_set));
}
