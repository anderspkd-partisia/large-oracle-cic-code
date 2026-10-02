#include "enc.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <scl/math/ff.h>
#include <scl/math/lagrange.h>
#include <scl/math/matrix.h>
#include <scl/math/vector.h>

#include "gf16.h"

using namespace scl;

using Elem = scl::math::FF<GF2_16>;

namespace {

Elem createElem(unsigned int b0, unsigned int b1) {
  return Elem((int)(b0 | (b1 << 8)));
}

/**
 * @brief Encode a chunk of a data.
 * @param size number of bytes to encode.
 * @param n chunk size.
 * @param van a Vandermode matrix used for encoding.
 */
math::Vector<Elem> encodeChunk(const unsigned char* data,
                               std::size_t size,
                               std::size_t n,
                               const math::Matrix<Elem>& van) {
  std::vector<Elem> chunk_data;
  chunk_data.reserve(n / 2);
  // Encode actual data.
  for (std::size_t i = 0; i < size - 1; i += 2) {
    chunk_data.emplace_back(createElem(data[i], data[i + 1]));
  }

  // Handle case where size is odd.
  if ((size % 2) != 0) {
    chunk_data.emplace_back(createElem(data[size - 1], 0));
  }

  // Pad with 0s.
  for (std::size_t i = size; i < n - 1; i += 2) {
    chunk_data.emplace_back(Elem::zero());
  }

  return van.multiply(math::Vector<Elem>(chunk_data));
}

}  // namespace

std::vector<Block> encode(const unsigned char* buffer,
                          std::size_t size,
                          std::size_t t,
                          std::size_t n) {
  std::vector<Block> encoded(n);

  // We encode t + 1 points at a time. This results in degree t polynomials,
  // which will require t + 1 evaluation points to recover.
  const std::size_t chunk_size = Elem::byteSize() * (t + 1);
  const std::size_t num_chunks = std::ceil((double)size / (double)chunk_size);

  // Reserve data for the blocks. Encoding is easier when we know we're gonna
  // encode a multiple of chunk_size. This introduces a bit of redundancies (in
  // the form of 0s). But w/e.
  for (Block& block : encoded) {
    block.resize(Elem::byteSize() * num_chunks);
  }

  const auto van = math::Matrix<Elem>::vandermonde(n, t + 1);

  std::size_t eo = 0;
  std::size_t bo = 0;
  while (size > 0) {
    const std::size_t sz = chunk_size > size ? size : chunk_size;
    const math::Vector<Elem> chunk =
        encodeChunk(buffer + bo, sz, chunk_size, van);
    size -= sz;
    bo += sz;

    for (std::size_t j = 0; j < n; ++j) {
      chunk[j].write(encoded[j].data() + eo);
    }
    eo += Elem::byteSize();
  }

  return encoded;
}

namespace {

math::Vector<Elem> computeAlphas(const std::vector<std::size_t>& ids) {
  std::vector<Elem> a;
  a.reserve(ids.size());
  for (const std::size_t& id : ids) {
    a.emplace_back(Elem((int)id));
  }
  return a;
}

// Check that all blocks have the same size, and return that size. Throw an
// exception otherwise.
std::size_t getBlockSize(const std::vector<Block>& blocks) {
  const std::size_t block0_sz = blocks[0].size();
  if (std::all_of(blocks.begin(), blocks.end(), [block0_sz](const Block& b) {
        return b.size() == block0_sz;
      })) {
    return block0_sz;
  }
  throw std::logic_error("blocks are un-equal size");
}

}  // namespace

std::vector<unsigned char> decode(const std::vector<Block>& blocks,
                                  const std::vector<std::size_t>& ids,
                                  std::size_t t) {
  // Sanity checks.
  if (ids.size() != t + 1 || blocks.size() != t + 1) {
    throw std::invalid_argument("invalid number of IDs or blocks provided");
  }

  const auto as = computeAlphas(ids);
  const auto vand = math::Matrix<Elem>::vandermonde(t + 1, t + 1, as).invert();

  std::size_t block_size = getBlockSize(blocks);
  std::vector<unsigned char> data;
  data.reserve(t * block_size);

  // temp buffer.
  std::vector<Elem> points;
  points.reserve(t + 1);
  std::size_t i = 0;

  // Process the blocks two bytes at a time. The construction of the blocks
  // originally ensures that block_size is a power of 2.
  while (block_size > 0) {
    points.clear();

    for (const Block& block : blocks) {
      points.emplace_back(createElem(block[i], block[i + 1]));
    }

    // Recover the original coefficients. I.e., the data that was encoded.
    const math::Vector<Elem> cf = vand.multiply(points);

    for (std::size_t j = 0; j < t + 1; ++j) {
      unsigned char d[2];
      cf[j].write(d);
      data.emplace_back(d[0]);
      data.emplace_back(d[1]);
    }

    block_size -= 2;
    i += 2;
  }

  return data;
}
