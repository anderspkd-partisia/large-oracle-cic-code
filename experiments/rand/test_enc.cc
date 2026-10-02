#include <iostream>

#include <scl/net/packet.h>
#include <scl/ss/feldman.h>

#include "enc.h"
#include "gf16.h"
#include "util.h"

#define CHECK(expr)                                        \
  do {                                                     \
    if (!(expr)) {                                         \
      std::cout << "check failed at " << __LINE__ << "\n"; \
      exit(0);                                             \
    }                                                      \
  } while (0)

using namespace scl;
using Elem = math::FF<GF2_16>;

int main() {
  const std::size_t n = 10;
  const std::size_t t = threshold(n);

  auto prg = util::PRG::create("rand");

  std::vector<unsigned char> input(14212);
  prg.next(input);

  auto blocks = encode(input.data(), input.size(), t, n);

  CHECK(blocks.size() == 10);

  std::vector<std::size_t> ids = {1, 6, 7, 9};
  std::vector<Block> blocks_ = {blocks[0], blocks[5], blocks[6], blocks[8]};
  auto dec = decode(blocks_, ids, t);

  std::size_t i = 0;
  for (; i < input.size(); ++i) {
    if (dec[i] != input[i]) {
      std::cout << "error at index " << i << "\n";
      std::exit(1);
    }
  }

  // leftovers should be 0.
  for (; i < dec.size(); ++i) {
    if (dec[i] != 0) {
      std::cout << "left-over is not 0\n";
      std::exit(2);
    }
  }
}
